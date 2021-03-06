// loadomf.cxx - load
//
//  Copyright <C> 1989-94, Microsoft Corporation
//
//  Purpose:
//
//  10-Nov-94   BryanT
//      Merge in NT changes.
//      Change the load code so we first call the Shell to see
//      if the symbol load s/b deferred or ignored.
//      Functions changed: OLStart, OLLoadOmf
//      New Functions: OLContinue (the part of OLStart that determines)
//                          what type of file we're looking at).
//                     LoadOmfForReal (the part of OLLoadOmf that actually
//                          performs the symbol load)
//      Replace all the hexg param's with lpexg's.  We have it everywhere
//      it's needed and every function calls LLLock/LLUnlock to get it...
//      Define UnloadOmf.
//
//  07-Jan-96   BryanT
//

#include "shinc.hpp"
#pragma hdrstop

#include <dbghelp.h>

// The exe file information

static LSZ          lszFName;           // file name
static LONG         lfaBase;            // offset of directory info from end of file
static ULONG        cDir;               // number of directory entries
static OMFDirEntry *lpdss;              // pointer to directory table
static OMFDirEntry *lpdssCur;           // pointer to current directory entry
static LONG         lcbFilePos;
static WORD         csegExe;
static PIMAGE_SECTION_HEADER  SecHdr;
static unsigned int SecCount;
static DWORD        ImageAlign;
static WORD         btAlign;            // Alignment bit

extern HLLI HlliExgExe;

BOOL                FKillLazyLoad;
HANDLE              HthdLazyLoad;
HANDLE              HevntLazyLoad;
CRITICAL_SECTION    CsExeModify;

typedef struct _PDB_INFO {
    SIG sig;
    AGE age;
    char sz[_MAX_PATH];
} PDB_INFO;

static  PDB_INFO pdbInfo;

const ULONG sigNB11 = '11BN';
const ULONG sigNB10 = '01BN';
const ULONG sigNB09 = '90BN';
const ULONG sigNB08 = '80BN';
const ULONG sigNB07 = '70BN';
const ULONG sigNB06 = '60BN';
const ULONG sigNB05 = '50BN';

// compile time assert
#if !defined(cassert)
#define cassert(x) extern char dummyAssert[ (x) ]
#endif


SHE  CheckSignature (HANDLE, OMFSignature *, PDB_INFO *);
SHE  OLStart (LPEXG);
BOOL OLMkSegDir (WORD, LPSGD *, LPSGE *, LPEXG);
SHE  OLLoadTypes (LPEXG);
SHE  OLLoadSym (LPEXG);
SHE  OLLoadSrc (LPEXG);
SHE  OLGlobalPubs (LPEXG);
SHE  OLGlobalSym (LPEXG);
SHE  OLStaticSym (LPEXG);
SHE  OLLoadSegMap (LPEXG);
SHE  OLLoadNameIndex (LPEXG);
LPCH OLRwrSrcMod (OMFSourceModule *);
BOOL OLLoadHashSubSec (LPGST, LPB, WidenTi * =0);
SHE  NB10LoadOmf (LPEXG, HEXG);
SHE  LoadPdb (LPEXG, PDB_INFO *, UOFFSET, BOOL);
SHE  NB10LoadModules (LPEXG, ULONG*, HEXG);
VOID LoadSymbols(HEXG, BOOL);
SHE  LoadOmfForReal(LPEXG, HEXG);
SHE  LoadFpo(LPEXG, HANDLE, PIMAGE_DEBUG_DIRECTORY);
SHE  LoadPdata(LPEXG, HANDLE, UOFFSET, UOFFSET, ULONG, ULONG, BOOL);
SHE  LoadOmap(LPEXG, HANDLE, PIMAGE_DEBUG_DIRECTORY);
int  OLMkModule(LPEXG, HEXG);
SHE  OLValidate(HANDLE, PVLDCHK);
SHE  SheFixupConvertedSyms(LPEXG);
void ConvertGlobal16bitSyms(WidenTi*, LPGST, PB, ULONG);

#define MAX_SEARCH_PATH_LEN   512

// This is hard-coded name of the registry location where setup will put the
// pdb path. This should be changed when we have a general mechanism for the
// debugger dlls to get the IDE's root registry key name.

static TCHAR szDefaultKeyName[300];

const TCHAR szKeySuffix[] =
        _T("Build System\\Components\\Platforms\\Win32 (x86)\\Directories");

static const TCHAR szPdbDirs[] = "Pdb Dirs";

TCHAR szSearchPath[MAX_SEARCH_PATH_LEN];
BOOL  fQueriedRegistry;

// CFile is a simple helper class which will force its file to be closed
// as soon as the CFile object is destructed.

class CFile {
    public:
        HANDLE m_hfile;

        CFile() { m_hfile = INVALID_HANDLE_VALUE; }
        void ReInit() {
            if (m_hfile != INVALID_HANDLE_VALUE) {
                SYClose(m_hfile);
                m_hfile = INVALID_HANDLE_VALUE;
            }
        }
        HANDLE Open(LSZ lszName) {
            m_hfile = SYOpen(lszName);
            return(m_hfile);
        }

        ~CFile() {
            if(m_hfile != INVALID_HANDLE_VALUE) {
                SYClose (m_hfile);
                m_hfile = INVALID_HANDLE_VALUE;
            }
        }

        int Int() {
            return (int) HandleToLong(m_hfile);
        }

        operator HANDLE&() { return m_hfile; }
};

VOID
LoadDefered(
    HEXG  hexg
    )
{
    LoadSymbols(hexg, TRUE);
    return;
}

VOID
UnloadDefered(
    HEXG hexg
    )
{
    return;
}

SHE UnloadNow(HEXG hexg)
{
    LPEXG       lpexg;

    lpexg = (LPEXG) LLLock(hexg);
    OLUnloadOmf(lpexg);
    VoidCaches();
    lpexg->fOmfDefered = TRUE;
    lpexg->sheLoadStatus = sheDeferSyms;
    lpexg->sheLoadError = sheNoSymbols;
    LLUnlock(hexg);
    return sheNone;
}

inline BOOL
fSheGoodReturn(SHE she)
{
    return ((she == sheNone) ||
            (she == sheSymbolsConverted) ||
            (she == sheConvertTIs));
}

//  OLLoadOmf - load omf information from exe
//
//  error = OLLoadOmf (hexg)
//
//  Entry   hexg = handle to executable information struct
//
//  Exit
//
//  Returns An error code suitable for errno.

SHE
OLLoadOmf(
    HEXG    hexg,
    VLDCHK *pVldChk,
    UOFFSET dllLoadAddress
    )
{
    SHE     sheRet = sheNone;
    LSZ     lszFname = NULL;
    LPEXG   lpexg = (LPEXG) LLLock (hexg);

    if (lpexg->fOmfLoaded) {
        return sheNone;
    }

    // Query the shell and see if we should load this one now.

    lszFname = lpexg->lszName;

    //      SYGetDefaultShe () always returns TRUE and leavs the parameters
    //      unchanged; so this bit of code is dead.
#ifndef NT_BUILD_ONLY
#undef SYGetDefaultShe
#define SYGetDefaultShe(a, b) ((*b=sheNone), TRUE)
#endif // NT_BUILD_ONLY

    if (!SYGetDefaultShe(lszFname, &sheRet)) {
        if (lpexg->lszAltName) {
            lszFname = lpexg->lszAltName;
            if (!SYGetDefaultShe(lszFname, &sheRet)) {
                SYGetDefaultShe(NULL, &sheRet);
                lszFname = lpexg->lszName;
            }
        } else {
            SYGetDefaultShe(NULL, &sheRet);
        }
    }

    // SYGetDefaultShe is expected to return one of the following
    // values:
    //
    // sheSuppressSyms - Don't load, just keep track of the name/start
    // sheNoSymbols - This module has already been processed and there are no symbols
    // sheDeferSyms - Defer symbol loading until needed
    // sheSymbolsConverted - The symbols are already loaded
    // sheNone - Go ahead and load the symbols now.

    // Regardless of the load type, save some stuff

    lpexg->LoadAddress  = dllLoadAddress;
    lpexg->ulTimeStamp  = pVldChk->ImgTimeDateStamp;
    lpexg->ulCheckSum   = pVldChk->ImgCheckSum;
    lpexg->ulImageSize  = pVldChk->ImgSize;

    lpexg->sheLoadStatus = sheRet;


    switch( sheRet ) {
    case sheNone:
    case sheSymbolsConverted:

        //
        // If we made it this far, we must load the symbols
        //

        LoadSymbols(hexg, FALSE);

        if (lpexg->fOmfMissing) {
            sheRet = sheNoSymbols;
        } else if (lpexg->fOmfSkipped) {
            sheRet = sheSuppressSyms;
        } else if (lpexg->fOmfDefered) {
            sheRet = sheDeferSyms;
        }

        lpexg->sheLoadStatus = sheRet;
        break;

    case sheNoSymbols:
        lpexg->fOmfMissing = TRUE;
        break;

    case sheSuppressSyms:
        lpexg->fOmfSkipped = TRUE;
        break;

    case sheDeferSyms:
        lpexg->fOmfDefered = TRUE;
        SetEvent(HevntLazyLoad);
        break;

    }

    LLUnlock(hexg);
    return sheRet;

}


//  LoadSymbols
//
//  Purpose: This function loads a defered module's symbols.  After
//      the symbols are loaded the shell is notified of the completed
//      module load.
//
//  Input:  hpds - Handle to process to load the symbols for
//          hexg - exg handle for the module to be added
//          fNotifyShell - Should shell be notified on load.
//
//  Return: None

VOID
LoadSymbols(
    HEXG hexg,
    BOOL fNotifyShell
    )
{
    SHE     sheRet;
    LPEXG   lpexg = NULL;
    LPSTR   lpname = NULL;
    HPDS    hpdsLast;

 Retry:
    EnterCriticalSection( &CsExeModify );

    //  lock down the necessary data structure

    lpexg = (LPEXG) LLLock(hexg);
    if (!lpexg) {
        goto done;
    }

    //
    //  We need to find out if the symbols are already being loaded by
    //  somebody else.  If so then we need wait for them to finish what
    //  we need done.
    //

    if (lpexg->fOmfLoading) {
        //
        //  Somebody beat us to it.  Need to wait for them
        //
        LLUnlock(hexg);
        LeaveCriticalSection(&CsExeModify);
        Sleep(1000);
        goto Retry;
    }

    //  mark the module as being loaded
    lpexg->fOmfLoading = TRUE;

    LeaveCriticalSection( &CsExeModify );

    //  load the symbols (yes, pass both lpexg and hexg.
    //          OlMkModule needs hexg for creating the lpmds)

    sheRet = LoadOmfForReal(lpexg, hexg);

    EnterCriticalSection( &CsExeModify );

    switch (sheRet) {
        case sheNoSymbols:
            lpexg->fOmfMissing = TRUE;
            break;

        case sheSuppressSyms:
            lpexg->fOmfSkipped = TRUE;
            break;

        case sheNone:
        case sheSymbolsConverted:
            lpexg->fOmfLoaded   = TRUE;
            break;

        default:
            lpexg->fOmfMissing = TRUE;
            break;
    }

    if (fNotifyShell) {
        //
        // notify the shell that symbols have been loaded
        //
        if (lpexg->lszAltName) {
            lpname = lpexg->lszAltName;
        } else {
            lpname = lpexg->lszName;
        }
        DLoadedSymbols(sheRet, lpname);
    }

    // update the module flags

    lpexg->fOmfDefered = FALSE;
    lpexg->fOmfLoading = FALSE;

done:

    LeaveCriticalSection( &CsExeModify );

    // free resources

    if (lpexg) {
        LLUnlock(hexg);
    }

    return;
}

//  LoadOmfForReal
//
//  Purpose: Here's where the symbolic is actually loaded from the image.
//
//  Input:  lpexg - The pointer to the exg structure
//          hexg  - The handle of the exg structure
//
//  Return: Standard she error codes.

SHE
LoadOmfForReal(
    LPEXG  lpexg,
    HEXG   hexg
    )
{
    SHE     sheRet = sheNone;
    SHE     sheRetSymbols = sheNone;
    WORD    cbMod = 0;
    ULONG   cMod;
    ULONG   iDir;

    csegExe = 0;

    __try {

        // Open and verify the exe.

        sheRet = sheRetSymbols = OLStart(lpexg);

        // If there was an error, bail.
        //  (sheNone doesn't mean "no symbols", it means "error None")

        if (!fSheGoodReturn(sheRet)) {
            goto returnhere;
        }

        if (lpexg->ppdb) {
            sheRet = NB10LoadOmf(lpexg, hexg);
            goto returnhere;
        }

        if (sheRet == sheConvertTIs) {
            // set up to do the conversions
            if (!WidenTi::fCreate(lpexg->pwti)) {
                sheRet = sheOutOfMemory;
                goto returnhere;
            }
        }

        btAlign = (WORD)(lfaBase & 1);

        lpdssCur = lpdss;
        iDir = 0;

        // Load up the module table.

        // First, count up how many sstModule entries we have.  The spec
        // requires all the sstModules to be before any other.

        while (iDir < cDir && lpdssCur->SubSection == sstModule) {
            lpdssCur++;
            iDir++;
        }

        // If there's no modules, there's no sense continuing.
        if (iDir == 0) {
            sheRet = sheNoSymbols;
            goto returnhere;
        }

        lpexg->cMod = cMod = iDir;

        // Allocate the rgMod buffer and load each dir entry in.

        lpexg->rgMod = (LPMDS)MHAlloc((cMod+2) * sizeof(MDS));
        if (lpexg->rgMod == NULL) {
            sheRet = sheOutOfMemory;
            goto returnhere;
        }
        memset(lpexg->rgMod, 0, sizeof(MDS)*(cMod+2));
        lpexg->rgMod[cMod+1].imds = (WORD) -1;

        // Go through the list of directory entries and process all of the sstModule records.

        for (iDir = 0, lpdssCur = lpdss;
             iDir < cMod;
             iDir += 1, lpdssCur++) {

            if (!OLMkModule (lpexg, hexg)) {
                sheRet = sheOutOfMemory;
                goto returnhere;
            }
        }

        // Set up map of modules.  This function is used to create a map
        // of contributer segments to modules.  This is then used when
        // determining which module is used for an address.
        lpexg->csgd = csegExe;
        if (!OLMkSegDir (csegExe, &lpexg->lpsgd, &lpexg->lpsge, lpexg)) {
            sheRet = sheOutOfMemory;
            goto returnhere;
        }

        // continue through the directory entries

        for (; iDir < cDir; lpdssCur++, iDir++) {
            if (lpdssCur->cb == 0) {
                // if nothing in this entry
                continue;
            }

            switch (lpdssCur->SubSection) {
                case sstSrcModule:
                    sheRet = OLLoadSrc(lpexg);
                    break;

                case sstAlignSym:
                    sheRet = OLLoadSym(lpexg);
                    break;

                case sstGlobalTypes:
                    sheRet = OLLoadTypes(lpexg);
                    break;

                case sstGlobalPub:
                    sheRet = OLGlobalPubs(lpexg);
                    break;

                case sstGlobalSym:
                    sheRet = OLGlobalSym(lpexg);
                    break;

                case sstSegMap:
                    sheRet = OLLoadSegMap(lpexg);
                    break;

                case sstLibraries:          // ignore this table
                case sstMPC:                // until this table is implemented
                case sstSegName:            // until this table is implemented
                case sstModule:             // Handled elsewhere
                    break;

                case sstFileIndex:
                    sheRet = OLLoadNameIndex(lpexg);
                    break;

                case sstStaticSym:
                    sheRet = OLStaticSym(lpexg);
                    break;

                default:
                    sheRet = sheCorruptOmf;
                    break;
            }

            if (sheRet == sheCorruptOmf) {
                sheRet = sheNoSymbols;
            }
        }
        if (lpexg->pwti) {
            sheRet = SheFixupConvertedSyms(lpexg);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        sheRet = sheNoSymbols;
    }

returnhere:

    if (SecHdr) {
        MHFree(SecHdr);
        SecHdr = NULL;
        SecCount = 0;
    }

    return sheRet;
}

SHE
NB10LoadOmf(
    LPEXG   lpexg,
    HEXG    hexg
    )
{
    SHE     sheRet = sheNone;
    WORD    cbMod = 0;
    ULONG   ModCnt = 0;

    btAlign = (WORD)(lfaBase & 1);

    // we need to allocate a buffer large enough to read the largest module
    // table entry

    if ((sheRet = NB10LoadModules (lpexg, &ModCnt, hexg)) != sheNone) {
        return sheRet;
    }

    if (ModCnt == 0L) {
        // if no symbols found
        return sheNoSymbols;
    }

    lpexg->cMod = ModCnt;

    if(!DBIOpenGlobals(lpexg->pdbi, &(lpexg->pgsiGlobs)) ||
       !DBIOpenPublics(lpexg->pdbi, &(lpexg->pgsiPubs)))
    {
        return sheOutOfMemory;
    }

    if((sheRet = OLLoadSegMap(lpexg)) != sheNone ||
       (sheRet = OLLoadNameIndex(lpexg)) != sheNone)
    {
        return sheRet;
    }

    return sheRet;
}

#define cbFileMax   (_MAX_CVFNAME + _MAX_CVEXT)

// OLStart - get exe debug information
//
//  Purpose: To open the file specified and get the offset to the directory
//           and get the base that everyone is offset from.
//
//  Entry   hexg = handle to exe to get info for
//
//  Exit    lfaBase = base offset of debug information
//          cDir = count of number of directory entries
//          lpdss = directory entries
//
//  Returns 
//          returns a 'she' value that clasified as symbol load is deferred,
//          symbol load failed, or symbols load succeeded

#define UNKNOWN_IMAGE   0
#define DOS_IMAGE       1
#define VCDBG_IMAGE     2
#define WIN16_IMAGE     3
#define PE_IMAGE        4
#define ROM_IMAGE       5
#define NTDBG_IMAGE     6

SHE
OLStart(
    LPEXG   lpexg
    )
{
    // This is the 'she' value that will be returned to the calling value.
    // The values it returns falls into 3 categories, symbol loading was:
    //      deferred, failed, or succeeded
    SHE                     sheRet;
    // This value compliments 'sheRet' by providing additional information
    // about the load process. Even though a symbol may have been 
    // successfully loaded, an error may have occurred such as a checksum 
    // error, etc.
    SHE                     sheError;
    ULONG                   DirSize;
    OMFSignature            Signature;
    OMFDirHeader            DirHeader;
    IMAGE_DOS_HEADER        doshdr;            // Old format MZ header
    IMAGE_NT_HEADERS        pehdr;
    IMAGE_ROM_HEADERS       romhdr;
    IMAGE_SEPARATE_DEBUG_HEADER  sepHdr;
    PIMAGE_FILE_HEADER      pfile;
    IMAGE_DEBUG_DIRECTORY   dbgDir;
    IMAGE_DEBUG_DIRECTORY   cvDbgDir;
    DWORD                   cbData;
    UOFFSET                 dllLoadAddress;
    DWORD                   ul;
    VLDCHK                  vldChk = {0};
    LSZ                     szFName = NULL;
    char                    szNewName[_MAX_PATH];
    int                     ImageType = UNKNOWN_IMAGE;
    DWORD                   cDebugDir;
    DWORD                   offDbgDir;
    DWORD                   cObjs;
    CFile                   hfile;
    UOFFSET                 ImageBase;

    if (lpexg->lszAltName) {
        szFName = lpexg->lszAltName;
    } else {
        szFName = lpexg->lszName;
    }

    // lpexg->lszDebug is the file where we pull the symbolic from.

    dllLoadAddress          = lpexg->LoadAddress;
    vldChk.ImgTimeDateStamp = lpexg->ulTimeStamp;
    vldChk.ImgCheckSum      = lpexg->ulCheckSum;
    vldChk.ImgSize          = lpexg->ulImageSize;
    ImageAlign              = 0;

    hfile.Open(szFName);

    if (hfile == INVALID_HANDLE_VALUE) {
retry:
        if (lpexg->lszDebug) {
            MHFree(lpexg->lszDebug);
            lpexg->lszDebug = 0;
        }
        hfile = SYFindExeFile(szFName, szNewName, sizeof(szNewName), &vldChk, OLValidate, &sheError);        
        if (hfile == INVALID_HANDLE_VALUE) {
            // 'SYFindExeFile' failed. Return a generic failure code in sheRet, but
            // preserve the specific failure error returned by 'SYFindExeFile'
            sheRet = sheFileOpen;
            goto ReturnHere;
        } else {
            // We succeeded. But save sheError, since the value of sheRet may
            // be changed.
            sheRet = sheError;
        }

        if ( ! ( lpexg->lszDebug = (LSZ) MHAlloc ( _ftcslen ( szNewName ) + 1 ) ) ) {
            sheRet = sheOutOfMemory;
            goto ReturnHere;
        }
        _tcscpy ( lpexg->lszDebug, szNewName );

    } else {
        // Assert that the input file is OK.  We only get here
        // when using the file name as passed in from the DM.

        sheError = 
        sheRet = OLValidate(hfile, &vldChk);
        if ((sheRet == sheBadChecksum) ||
            (sheRet == sheBadTimeStamp) ||
            (sheRet == sheNoSymbols))
        {
            hfile.ReInit();
            goto retry;
        }
        if ( ! ( lpexg->lszDebug = (LSZ) MHAlloc ( _ftcslen ( szFName ) + 1 ) ) ) {
            sheRet = sheOutOfMemory;
            goto ReturnHere;
        }
        _tcscpy ( lpexg->lszDebug, szFName );

    }

    // HACK: If we are pre-loading symbols lpexg->[ulTimeStamp|ulCheckSum] will be 0
    // at this point. However vldChk will be updated to have the appropriate
    // information. Update the lpexg structures with the right value.

    if (lpexg->ulTimeStamp == 0) {
        lpexg->ulTimeStamp = vldChk.ImgTimeDateStamp;
    }

    // Now figure out what we're looking at.  Here are the possible formats:
    // 1. Image starts with a DOS MZ header and e_lfanew is zero
    //     - Standard DOS exe.
    // 2. Image starts with a DOS NE header and e_lfanew is non-zero
    //     - If e_lfanew points to a PE header, this is a PE image
    //     - Otherwise, it's probably a Win16 image.
    // 3. Image starts with a PE header.
    //     - Image is a PE image built with -stub:none
    // 4. Image starts with a ROM PE header.
    //     - Image is a ROM image.  If characteristics flag
    //          doesn't have IMAGE_FILE_DEBUG_STRIPPED set, the debug
    //          directory is at the start of rdata.
    // 5. Image starts with a DBG file header
    //     - Image is an NT DBG file (symbols only).
    // 6. None of the signatures match.
    //     - This may be a Languages DBG file.  Seek to the end
    //       of the file and attempt to read the CV signature/offset
    //       from there (a Languages DBG file is made by chopping an
    //       image at the start of the debug data and writing the end
    //       in a new file.  In the CV format, the signature/offset at the
    //       end of the file points back to the beginning of the data).

    if ((SYSeek(hfile, 0, SEEK_SET) == 0) &&
        sizeof(doshdr) == SYReadFar (hfile, (LPB) &doshdr, sizeof(doshdr)))
    {
        switch (doshdr.e_magic) {
            case IMAGE_DOS_SIGNATURE:
                //  This is a DOS NE header.
                if (doshdr.e_lfanew == 0) {
                    ImageType = DOS_IMAGE;
                } else {
                    if ((SYSeek(hfile, doshdr.e_lfanew, SEEK_SET) == doshdr.e_lfanew) &&
                        (SYReadFar(hfile, (LPB) &pehdr, sizeof(pehdr)) == sizeof(pehdr)))
                    {
                        if (pehdr.Signature == IMAGE_NT_SIGNATURE) {
                            ImageType = PE_IMAGE;
                            ImageAlign = pehdr.OptionalHeader.SectionAlignment;
                            pfile = &pehdr.FileHeader;
                        } else {
                            ImageType = WIN16_IMAGE;
                        }
                    } else {
                        // No luck reading from the image.  Must be corrupt.
                        sheRet = sheCorruptOmf;
                        goto ReturnHere;
                    }
                }
                break;

            case IMAGE_SEPARATE_DEBUG_SIGNATURE:
                // This image is an NT DBG file.
                ImageType = NTDBG_IMAGE;
                if ((SYSeek(hfile, 0, SEEK_SET) != 0) ||
                    (SYReadFar(hfile, (LPB) &sepHdr, sizeof(sepHdr)) != sizeof(sepHdr)))
                {
                    // No luck reading from the image.  Must be corrupt.
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }

                // If there's no debug info, we can't continue further.

                if (sepHdr.DebugDirectorySize / sizeof(dbgDir) == 0) {
                    sheRet = sheNoSymbols;
                    goto ReturnHere;
                }
                break;

            default:
                // None of the above.  See if it's a ROM image.
                // Note: The only way we think we're working on a ROM image
                // is if the optional header size is correct.  Not really foolproof.

                if ((SYSeek(hfile, 0, SEEK_SET) == 0) &&
                    (SYReadFar(hfile, (LPB) &romhdr, sizeof(romhdr)) == sizeof(romhdr)))
                {
                    if (romhdr.FileHeader.SizeOfOptionalHeader == IMAGE_SIZEOF_ROM_OPTIONAL_HEADER) {
                        // If we think we have a ROM image, make sure there's
                        // symbolic to look for.
                        if (romhdr.FileHeader.Characteristics & IMAGE_FILE_DEBUG_STRIPPED) {
                            sheRet = sheNoSymbols;
                            goto ReturnHere;
                        } else {
                            ImageType = ROM_IMAGE;
                            pfile = &romhdr.FileHeader;
                        }
                    } else {
                        ImageType = VCDBG_IMAGE;
                    }
                } else {
                    // No luck reading from the image.  Must be corrupt.
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }
                break;
        }

    } else {
        // No luck reading from the image.  Must be corrupt.
        sheRet = sheCorruptOmf;
        goto ReturnHere;
    }

    // Now, we know what kind of image we're looking at.
    // Either obtain the pointer to the CV debug data (and other
    // relevant data along the way) or convert whatever we do find
    // to CV debug data.

    lpexg->fSymConverted = FALSE;

    switch (ImageType) {
        case DOS_IMAGE:
        case VCDBG_IMAGE:
        case WIN16_IMAGE:
            // Easy.  Skip to the end and look back.
            ul = SYSeek (hfile, -((LONG)sizeof (OMFSignature)), SEEK_END);
            if ((sheRet = CheckSignature (hfile, &Signature, &pdbInfo)) == sheNone) {
                // seek to the base and read in the new key

                lfaBase = SYSeek (hfile, -Signature.filepos, SEEK_END);
                sheRet = CheckSignature(hfile, &Signature, &pdbInfo);
                cbData = ul - lfaBase;
            }

            // Only possible architecture of the image file

            lpexg->machine = IMAGE_FILE_MACHINE_I386;

            // If the CV signature is invalid, see if we can convert what we do
            // have (perhaps a .sym file?)

            if (sheRet != sheNone) {
                if (pfConvertSymbolsForImage) {
                    lpexg->lpbData = (LPB) (pfConvertSymbolsForImage)(
                                             hfile, 
                                             lpexg->lszDebug);
                }
                // If no symbols converted, bail.  Nothing more we can do.
                if (lpexg->lpbData == 0) {
                    sheRet = sheNoSymbols;
                    goto ReturnHere;
                }
                Signature = *(OMFSignature*)lpexg->lpbData;
                lpexg->fSymConverted = TRUE;
            }
            break;

        case PE_IMAGE:
        case ROM_IMAGE:
            // In both the PE image and ROM image, we're past the FILE
            // and OPTIONAL header by now.  Walk through the section
            // headers and pick up interesting data.  We make a
            // a copy of the section headers in case we need to
            // reconstruct the original values for a Lego'd image

            cObjs = pfile->NumberOfSections;
            SecCount = pfile->NumberOfSections;
            ImageBase = pehdr.OptionalHeader.ImageBase;

            ul = SecCount * sizeof(IMAGE_SECTION_HEADER);

            // Note: SecHdr is free'd by LoadOmfForReal.
            SecHdr = (PIMAGE_SECTION_HEADER) MHAlloc(ul);

            // Capture the architecture of the image file
            lpexg->machine = pfile->Machine;

            if (!SecHdr) {
                sheRet = sheNoSymbols;
                goto ReturnHere;
            }

            if (SYReadFar(hfile, (LPB) SecHdr, ul) != ul) {
                sheRet = sheNoSymbols;
                goto ReturnHere;
            }


            if (ImageType == PE_IMAGE) {
                // look for the .pdata section on RISC platforms
                if ((pfile->Machine == IMAGE_FILE_MACHINE_ALPHA) ||
                    (pfile->Machine == IMAGE_FILE_MACHINE_ALPHA64) ||
                    (pfile->Machine == IMAGE_FILE_MACHINE_IA64))
                {
                    for (ul=0; ul < cObjs; ul++) {
                        if (_tcscmp((char *) SecHdr[ul].Name, ".pdata") == 0) {
                            LoadPdata(lpexg,
                                      hfile,
                                      dllLoadAddress,
                                      ImageBase,
                                      SecHdr[ul].PointerToRawData,
                                      SecHdr[ul].SizeOfRawData,
                                      FALSE);
                            lpexg->debugData.lpOriginalRtf = (PVOID)(ImageBase + SecHdr[ul].VirtualAddress);
                            break;
                        }
                    }
                }

                // If the debug info has been stripped, close this handle
                // and look for the .dbg file...

                if (pfile->Characteristics & IMAGE_FILE_DEBUG_STRIPPED) {
                    // The debug info has been stripped from this image.
                    // Close this file handle and look for the .DBG file.
                    hfile.ReInit();
                    ImageType = UNKNOWN_IMAGE;
                    MHFree(SecHdr);
                    SecHdr = 0;
                    goto retry;
                }

                // Find the debug directory and the number of entries in it.

                // For PE images, walk the section headers looking for the
                // one that's got the debug directory.
                for (ul=0; ul < cObjs; ul++) {
                    if ((SecHdr[ul].VirtualAddress <=
                         pehdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress) &&
                        (pehdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress <
                         SecHdr[ul].VirtualAddress + SecHdr[ul].SizeOfRawData)) {

                        // This calculation really isn't necessary nor is the range test
                        // above.  Like ROM images, it s/b at the beginning of .rdata.  The
                        // only time it won't be is when a pre NT 1.0 image is split sym'd
                        // creating a new MISC debug entry and relocating the directory
                        // to the DEBUG section...

                        offDbgDir = SecHdr[ul].PointerToRawData +
                            pehdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress -
                            SecHdr[ul].VirtualAddress;
                        cDebugDir = pehdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size /
                                 sizeof(IMAGE_DEBUG_DIRECTORY);
                        break;
                    }
                }
            } else {
                // For ROM images, there's much less work to do.  We only
                // need to search for the .rdata section.  There's no need
                // to look for .pdata (it will never exist) or worry about
                // stripped debug symbolic (that case was already handled above).
                for (ul=0; ul < cObjs; ul++) {
                    if (!_tcscmp((char *)SecHdr[ul].Name, ".rdata")) {
                        offDbgDir = SecHdr[ul].PointerToRawData;
                        if (SYSeek(hfile, offDbgDir, SEEK_SET) != (LONG) offDbgDir) {
                            sheRet = sheCorruptOmf;
                            goto ReturnHere;
                        }

                        // The linker stores an empty directory entry for ROM
                        // images to terminate the list.

                        cDebugDir = 0;
                        do {
                            if (SYReadFar(hfile, (LPB) &dbgDir, sizeof(dbgDir)) != sizeof(dbgDir)) {
                                sheRet = sheNoSymbols;
                                goto ReturnHere;
                            }
                            cDebugDir++;
                        } while (dbgDir.Type != 0);

                        break;
                    }
                }
            }

            // Assuming we haven't exhausted the list of section headers,
            // we should have the debug directory now.
            if (ul == cObjs) {
                // We didn't find any CV info.  Try converting what we did
                // find.
                if (pfConvertSymbolsForImage) {
                    lpexg->lpbData = (LPB)(pfConvertSymbolsForImage)( hfile, lpexg->lszDebug);
                }
                if (lpexg->lpbData == 0) {
                    sheRet = sheNoSymbols;
                    goto ReturnHere;
                }
                Signature = *(OMFSignature*)lpexg->lpbData;
                lpexg->fSymConverted = TRUE;
                break;
            }

            // Now search the debug directory for relevant entries.

            if (SYSeek(hfile, offDbgDir, SEEK_SET) != (LONG) offDbgDir) {
                sheRet = sheCorruptOmf;
                goto ReturnHere;
            }

            ZeroMemory(&cvDbgDir, sizeof(cvDbgDir) );

            for (ul=0; ul < cDebugDir; ul++) {
                if (SYReadFar(hfile, (LPB) &dbgDir, sizeof(dbgDir)) != sizeof(dbgDir)) {
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }

                if (dbgDir.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
                    cvDbgDir = dbgDir;
                    continue;
                }

                if (dbgDir.Type == IMAGE_DEBUG_TYPE_FPO) {
                    LoadFpo(lpexg, hfile, &dbgDir);
                }

                if (dbgDir.Type == IMAGE_DEBUG_TYPE_OMAP_FROM_SRC ||
                    dbgDir.Type == IMAGE_DEBUG_TYPE_OMAP_TO_SRC) {
                    LoadOmap(lpexg, hfile, &dbgDir);
                }
            }

            if (cvDbgDir.Type != IMAGE_DEBUG_TYPE_CODEVIEW) {
                // We didn't find any CV info.  Try converting what we did
                // find.
                if (pfConvertSymbolsForImage) {
                    lpexg->lpbData = (LPB)(pfConvertSymbolsForImage)( hfile, lpexg->lszDebug);
                }
                if (lpexg->lpbData == 0) {
                    sheRet = sheNoSymbols;
                    goto ReturnHere;
                }
                Signature = *(OMFSignature*)lpexg->lpbData;
                lpexg->fSymConverted = TRUE;
            } else {
                // Otherwise, calculate the location/size so we can load it.
                lfaBase = cvDbgDir.PointerToRawData;
                cbData =  cvDbgDir.SizeOfData;
                if (SYSeek(hfile, lfaBase, SEEK_SET) != lfaBase) {
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }
                sheRet = CheckSignature (hfile, &Signature, &pdbInfo);
                if ((sheRet != sheNone) && (sheRet != sheConvertTIs)) {
                    goto ReturnHere;
                }
            }

            break;

        case NTDBG_IMAGE:
            SecCount = sepHdr.NumberOfSections;
            ImageBase= sepHdr.ImageBase;

            if (sepHdr.SectionAlignment) {
                // The first reserved slot hold the original image alignment
                // for use with recreating the pre-lego section headers.
                ImageAlign = sepHdr.SectionAlignment;
            }

            ul = SecCount * sizeof(IMAGE_SECTION_HEADER);

            // Note: SecHdr is free'd by LoadOmfForReal.

            SecHdr = (PIMAGE_SECTION_HEADER) MHAlloc(ul);
            if (!SecHdr) {
                sheRet = sheNoSymbols;
                goto ReturnHere;
            }

            // Read in the section headers.

            if (SYReadFar(hfile, (LPB) SecHdr, ul) != ul) {
                sheRet = sheCorruptOmf;
                goto ReturnHere;
            }

            // Capture the architecture of the image file

            lpexg->machine = sepHdr.Machine;

            // Skip over the exported names.

            SYSeek(hfile, sepHdr.ExportedNamesSize, SEEK_CUR);

            // Look for the interesting debug data.

            ZeroMemory(&cvDbgDir, sizeof(cvDbgDir));

            for (ul=0; ul < (sepHdr.DebugDirectorySize/sizeof(dbgDir)); ul++) {
                if (SYReadFar(hfile, (LPB) &dbgDir, sizeof(dbgDir)) != sizeof(dbgDir)) {
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }

                switch (dbgDir.Type) {
                    case IMAGE_DEBUG_TYPE_CODEVIEW :
                        cvDbgDir = dbgDir;
                        break;

                    case IMAGE_DEBUG_TYPE_FPO :
                        LoadFpo(lpexg, hfile, &dbgDir);
                        break;

                    case IMAGE_DEBUG_TYPE_EXCEPTION :
                        // UNDONE: We can eliminate this load for images
                        // that we've already processed the pdata from the
                        // real image...

                        LoadPdata(lpexg,
                                  hfile,
                                  dllLoadAddress,
                                  ImageBase,
                                  dbgDir.PointerToRawData,
                                  dbgDir.SizeOfData,
                                  TRUE);

                        ULONG ul2;
                        for (ul2=0; ul2 < cObjs; ul2++) {
                            if (_tcscmp((char *) SecHdr[ul2].Name, ".pdata") == 0) {
                                lpexg->debugData.lpOriginalRtf = (PVOID)SecHdr[ul].PointerToRawData;
                                break;
                            }
                        }
                        break;

                    case IMAGE_DEBUG_TYPE_OMAP_TO_SRC :
                    case IMAGE_DEBUG_TYPE_OMAP_FROM_SRC :
                        LoadOmap(lpexg, hfile, &dbgDir);
                        break;
                }
            }

            if (cvDbgDir.Type != IMAGE_DEBUG_TYPE_CODEVIEW) {
                if (pfConvertSymbolsForImage) {
                    lpexg->lpbData = (LPB)(pfConvertSymbolsForImage)( hfile, lpexg->lszDebug);
                }
                if (lpexg->lpbData == 0) {
                    sheRet = sheNoSymbols;
                    goto ReturnHere;
                }
                Signature = *(OMFSignature*)lpexg->lpbData;
                lpexg->fSymConverted = TRUE;
            } else {
                lfaBase = cvDbgDir.PointerToRawData;
                cbData =  cvDbgDir.SizeOfData;
                if (SYSeek(hfile, lfaBase, SEEK_SET) != lfaBase) {
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }
                sheRet = CheckSignature (hfile, &Signature, &pdbInfo);

                if ((sheRet != sheNone) && (sheRet != sheConvertTIs))
                {
                    goto ReturnHere;
                }
            }
            break;

        default:
            // No way we should get here, but assert if we do.
            assert(FALSE);
    }

    // O.K.  Everything's loaded.  If we're looking at a pdb file,
    // load it and get out.

    if ((*(LONG UNALIGNED *)(Signature.Signature)) == sigNB10) {
        sheRet = LoadPdb(lpexg, &pdbInfo, ImageBase, !SYIgnoreAllSymbolErrors());
    } else {
        // No PDB.
        // If the symbols weren't synthesized, allocate a buffer and
        //  copy them in...

        if (!lpexg->fSymConverted) {
            HANDLE hMap;
            HANDLE hFileMap;

            hFileMap = CreateFile(lpexg->lszDebug,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

            if (hFileMap != INVALID_HANDLE_VALUE) {
                hMap = CreateFileMapping(hFileMap,
                                        NULL,
                                        PAGE_WRITECOPY,
                                        0,
                                        0,
                                        NULL);
                if (hMap != NULL) {
                    // Map in the symbolic (only).
                    SYSTEM_INFO si;
                    DWORD dwAllocStart, dwAllocDiff;

                    GetSystemInfo(&si);

                    dwAllocStart = lfaBase & (~(si.dwAllocationGranularity - 1));
                    dwAllocDiff = lfaBase - dwAllocStart;

                    lpexg->pvSymMappedBase = MapViewOfFile(hMap,
                                                           FILE_MAP_COPY,
                                                           0,
                                                           dwAllocStart,
                                                           cbData + dwAllocDiff);
                    if (lpexg->pvSymMappedBase) {
                        lpexg->lpbData = ((BYTE *) lpexg->pvSymMappedBase) + dwAllocDiff;
                    }

                    CloseHandle(hMap);
                }

                CloseHandle(hFileMap);
            }

            if (lpexg->lpbData == NULL) {

                // Unable to map the image.  Read the whole blob in from disk.

                lpexg->lpbData = (LPB)MHAlloc(cbData);
                if (!lpexg->lpbData) {
                    sheRet = sheNoSymbols;
                    goto ReturnHere;
                }

                if ((SYSeek (hfile, lfaBase, SEEK_SET) != lfaBase) ||
                    (SYReadFar (hfile, lpexg->lpbData, cbData) != cbData))
                {
                    // Failed to read in the data... Must be corrupt.
                    MHFree(lpexg->lpbData);
                    lpexg->lpbData = 0;
                    sheRet = sheCorruptOmf;
                    goto ReturnHere;
                }
            }
        }

        // We now have a pointer to the CV debug data.  Setup the
        //  pointers to the CV Directory header and return.

        LPB     lpb;
        lpexg->ppdb = NULL;
        lpexg->ptpi = NULL;
        lpexg->pdbi = NULL;

        lpb = Signature.filepos + lpexg->lpbData;

        DirHeader = *(OMFDirHeader *) lpb;
        cDir = DirHeader.cDir;

        // check to make sure somebody has not messed with omf structure
        if (DirHeader.cbDirEntry != sizeof (OMFDirEntry)) {
            sheRet = sheCorruptOmf;
            goto ReturnHere;
        }

        lpdss = (OMFDirEntry *)(lpb + sizeof(DirHeader));

        if (lpexg->fSymConverted) {
            sheRet = sheSymbolsConverted;
            goto ReturnHere;
        }

        if (sheRet == sheConvertTIs) {
            goto ReturnHere;
        }

        sheRet = sheNone;
    }

ReturnHere:

    lpexg->sheLoadStatus = sheRet;
    lpexg->sheLoadError = sheError;

    return sheRet;
}


SHE
LoadFpo(
    LPEXG                   lpexg,
    HANDLE                  hfile,
    PIMAGE_DEBUG_DIRECTORY  dbgDir
    )
{
    LONG fpos;

    fpos = SYTell(hfile);

    lpexg->fIsRisc = FALSE;

    if (SYSeek(hfile, dbgDir->PointerToRawData, SEEK_SET) != (LONG) dbgDir->PointerToRawData) {
        return(sheCorruptOmf);
    }

    if(!(lpexg->debugData.lpFpo = (PFPO_DATA) MHAlloc(dbgDir->SizeOfData))) {
        return sheOutOfMemory;
    }

    SYReadFar(hfile, (LPB) lpexg->debugData.lpFpo, dbgDir->SizeOfData);
    lpexg->debugData.cRtf = dbgDir->SizeOfData / SIZEOF_RFPO_DATA;

    SYSeek(hfile, fpos, SEEK_SET);

    return sheNone;
}

SHE
LoadOmap(
    LPEXG                   lpexg,
    HANDLE                  hfile,
    PIMAGE_DEBUG_DIRECTORY  dbgDir
    )
{
    LONG    fpos;
    LPVOID  lpOmap;
    DWORD   dwCount;

    fpos = SYTell(hfile);

    if (SYSeek(hfile, dbgDir->PointerToRawData, SEEK_SET) != (LONG) dbgDir->PointerToRawData) {
        return(sheCorruptOmf);
    }
    if(!(lpOmap = (LPVOID) MHAlloc(dbgDir->SizeOfData)))
        return sheOutOfMemory;
    SYReadFar(hfile, (LPB) lpOmap, dbgDir->SizeOfData);

    dwCount = dbgDir->SizeOfData / sizeof(OMAP);

    SYSeek(hfile, fpos, SEEK_SET);

    if(dbgDir->Type == IMAGE_DEBUG_TYPE_OMAP_FROM_SRC) {
        lpexg->debugData.lpOmapFrom = (LPOMAP) lpOmap;
        lpexg->debugData.cOmapFrom = dwCount;
    } else
    if(dbgDir->Type == IMAGE_DEBUG_TYPE_OMAP_TO_SRC) {
        lpexg->debugData.lpOmapTo = (LPOMAP) lpOmap;
        lpexg->debugData.cOmapTo = dwCount;
    } else {
        MHFree(lpOmap);
    }
    return sheNone;
}

PVOID
CreateRuntimeFunctionTable (
    PVOID   dbgRf,
    ULONG   cFunc,
    UOFFSET imageBase,
    DWORD   dwSizRTFEntry 
    )
{
    ULONG   index;
    PVOID   rf;

    if(!(rf = MHAlloc(cFunc * dwSizRTFEntry))) {
        return NULL;
    }

    //
    // This fixes up the function table to look as it does in the un-fixed up image.
    //
    switch(GetTargetMachine()) {
        case mptdaxp: 
        // Case 32bit mptdaxp
        if (dwSizRTFEntry == sizeof(IMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)) {
            for (index=0; index<cFunc; index++) {
                ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].BeginAddress       = ((PIMAGE_FUNCTION_ENTRY)dbgRf)[index].StartingAddress + (DWORD)imageBase;
                ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].EndAddress         = ((PIMAGE_FUNCTION_ENTRY)dbgRf)[index].EndingAddress + (DWORD)imageBase;
                ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].PrologEndAddress   = ((PIMAGE_FUNCTION_ENTRY)dbgRf)[index].EndOfPrologue + (DWORD)imageBase;
                ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].ExceptionHandler   = 0;
                ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].HandlerData        = 0;
            }
        // Case 64bit mptdaxp
        } else {
            for (index=0; index<cFunc; index++) {
                ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].BeginAddress       = ((PIMAGE_FUNCTION_ENTRY64)dbgRf)[index].StartingAddress + imageBase;
                ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].EndAddress         = ((PIMAGE_FUNCTION_ENTRY64)dbgRf)[index].EndingAddress + imageBase;
                ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].PrologEndAddress   = ((PIMAGE_FUNCTION_ENTRY64)dbgRf)[index].EndOfPrologue + imageBase;
                ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].ExceptionHandler   = 0;
                ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].HandlerData        = 0;
            }
        }
        break;
        case mptia64:
            break;

        default:
            assert(!"Unsupported platform");
            break;
    }
    return rf;
}


SHE
LoadPdata(
    LPEXG                   lpexg,
    HANDLE                  hfile,
    UOFFSET                 loadAddress,
    UOFFSET                 imageBase,
    ULONG                   start,
    ULONG                   size,
    BOOL                    fDbgFile
    )
{
    ULONG   cFunc;
    UOFFSET diff;
    ULONG   index;
    PVOID   rf;
    PVOID   tf;
    LONG    fpos;

    DWORD dwSizRTFEntry;
    DWORD dwSizFEntry;

    lpexg->debugData.lpRtf = NULL;
    lpexg->debugData.cRtf  = 0;

    if(size == 0) {
        return sheNone;    // No data to read...  Just return.
    }

    switch (lpexg->machine) {
        case IMAGE_FILE_MACHINE_ALPHA:
                    dwSizRTFEntry = sizeof(IMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY);
                    dwSizFEntry = sizeof(IMAGE_FUNCTION_ENTRY);
                    break;

        case IMAGE_FILE_MACHINE_ALPHA64:
                    dwSizRTFEntry = sizeof(IMAGE_AXP64_RUNTIME_FUNCTION_ENTRY);
                    dwSizFEntry = sizeof(IMAGE_FUNCTION_ENTRY64);
                    break;

        case IMAGE_FILE_MACHINE_IA64:
                    dwSizRTFEntry = sizeof(IMAGE_IA64_RUNTIME_FUNCTION_ENTRY);
                    dwSizFEntry = sizeof(IMAGE_FUNCTION_ENTRY);
                    break;

        default:
            assert(!"Unsupported platform");
            break;
    }
    
    if(fDbgFile) {
        cFunc = size / dwSizFEntry;
        diff = 0;
    } else {
        cFunc = size / dwSizRTFEntry;
        if (loadAddress) {
            diff = loadAddress - imageBase;
        } else {
            diff = 0;
        }
    }

    lpexg->fIsRisc = TRUE;

    fpos = SYTell(hfile);

    if (SYSeek(hfile, start, SEEK_SET) != (LONG) start) {
        return(sheCorruptOmf);
    }

    if(fDbgFile) {
        PVOID   dbgRf;
        if(!(dbgRf = MHAlloc(size))) {
            return sheOutOfMemory;
        }
        SYReadFar(hfile, (LPB)dbgRf, size);
        tf = rf = CreateRuntimeFunctionTable(dbgRf, cFunc, imageBase, dwSizRTFEntry);
        MHFree(dbgRf);

    } else {
        if(!(tf = rf = MHAlloc(size))) {
            return sheOutOfMemory;
        }
        SYReadFar(hfile, (LPB)rf, size);
    }

    // If this is an ilink'd image, there'll be padding at the end of the pdata section
    //  (to allow for insertion later).  Shrink the table if this is true.

    // Find the start of the padded page (end of the real data)

    assert(dwSizRTFEntry !=0); 
      
    switch (lpexg->machine) {
        case IMAGE_FILE_MACHINE_ALPHA:
            for(index=0; index<cFunc && ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)tf)->BeginAddress; tf = (PBYTE)tf + dwSizRTFEntry,index++) {
                ;
            }
            break;

        case IMAGE_FILE_MACHINE_ALPHA64:
            for(index=0; index<cFunc && ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)tf)->BeginAddress; tf = (PBYTE)tf + dwSizRTFEntry,index++) {
                ;
            }
            break;

        case IMAGE_FILE_MACHINE_IA64:
            for(index=0; index<cFunc && ((PIMAGE_IA64_RUNTIME_FUNCTION_ENTRY)tf)->BeginAddress; tf = (PBYTE)tf + dwSizRTFEntry,index++) {
                ;
            }
            break;
        
        default:
            assert(!"Unsupported platform");
            break;
    }

    if(index < cFunc) {
        cFunc = index;
        size  = index * dwSizRTFEntry;
        if(!(rf = MHRealloc(rf, size))) {
            return sheOutOfMemory;
        }
    }

    if (diff != 0) {
        switch (lpexg->machine) {
            case IMAGE_FILE_MACHINE_ALPHA:
                for (index=0; index<cFunc; index++) {
                    ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].BeginAddress       += (DWORD)diff;
                    ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].EndAddress         += (DWORD)diff;
                    ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].PrologEndAddress   += (DWORD)diff;
                    ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].ExceptionHandler   = 0;
                    ((PIMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY)rf)[index].HandlerData        = 0;
            }
            break;

            case IMAGE_FILE_MACHINE_ALPHA64:
                for (index=0; index<cFunc; index++) {
                    ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].BeginAddress       += diff;
                    ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].EndAddress         += diff;
                    ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].PrologEndAddress   += diff;
                    ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].ExceptionHandler   = 0;
                    ((PIMAGE_AXP64_RUNTIME_FUNCTION_ENTRY)rf)[index].HandlerData        = 0;
            }
            break;

            case IMAGE_FILE_MACHINE_IA64:
                break;

            default:
                assert(!"Unsupported platform");
                break;
        }
    }

    lpexg->debugData.lpRtf = rf;
    lpexg->debugData.cRtf  = cFunc;

    SYSeek(hfile, fpos, SEEK_SET);
    return sheNone;
}


//  CheckSignature - check file signature
//
//  she = CheckSignature (HANDLE hfile, OMFSignature *pSig)
//
//  Entry   hfile = handle to file
//          pSig  = location where signature should be written to
//          ppdb  = PDB information.
//
//  Exit    none
//
//  Return  sheNoSymbols if exe has no signature
//          sheMustRelink if exe has NB00 to NB06 or NB07 (qcwin) signature
//          sheNotPacked if exe has NB08 signature
//          sheNone if exe has NB11 signature
//          sheConvertTIs if exe has NB09 signature
//          sheFutureSymbols if exe has NB12 to NB99 signature

SHE
CheckSignature(
    HANDLE          hfile,
    OMFSignature    *pSig,
    PDB_INFO        *ppdb
    )
{
    UINT    uSig;

    if ((SYReadFar (hfile, (LPB) pSig, sizeof (*pSig)) != sizeof (*pSig)) ||
         (pSig->Signature[0] != 'N') ||
         (pSig->Signature[1] != 'B') ||
         (!isdigit(pSig->Signature[2])) ||
         (!isdigit(pSig->Signature[3]))) {
        return sheNoSymbols;
    }

    switch (*(LONG UNALIGNED *)(pSig->Signature)) {
        case sigNB05:
        case sigNB06:
        case sigNB07:
            return sheMustRelink;
        case sigNB08:
            return sheNotPacked;
        case sigNB09:
            return sheConvertTIs;
        case sigNB10:
            SYReadFar(hfile, (LPB)ppdb, sizeof(PDB_INFO));
        case sigNB11:
            return sheNone;
        default:
            return sheFutureSymbols;
    }
}


//  OLMkSegDir - MakeSegment directory
//
//  Entry
//
//  Returns non-zero for success

BOOL
OLMkSegDir(
    WORD  csgd,
    LPSGD *lplpsgd,
    LPSGE *lplpsge,
    LPEXG lpexg
    )
{
    LPSGD lpsgd;
    LPSGE lpsge = NULL;
    int  *lpisge;
    int   csgc = 0;
    int   isge = 0;
    int   isgd = 0;
    DWORD iMod;

    if (!(lpsgd = (LPSGD) MHAlloc (csgd * sizeof (SGD)))) {
        return FALSE;
    }

    if (!(lpisge = (int *) MHAlloc (csgd * sizeof (int)))) {
        MHFree(lpsgd);
        return FALSE;
    }

    memset(lpsgd,  0, csgd * sizeof(SGD));
    memset(lpisge, 0, csgd * sizeof(int));

    // Count the number of contributers per segment

    for (iMod = 1; iMod <= lpexg->cMod; iMod++) {
        LPMDS lpmds = &lpexg->rgMod[iMod];
        int cseg = lpmds->csgc;
        int iseg = 0;
        int isegT = 0;

        for (iseg = 0; iseg < cseg; iseg++) {
            isegT = lpmds->lpsgc [ iseg ].seg;
            if (isegT != 0) {
                lpsgd [ isegT - 1 ].csge += 1;
                csgc += 1;
            }
        }
    }

    // Allocate subtable for each all segments

    lpsge = (LPSGE) MHAlloc (csgc * sizeof (SGE));

    if (!lpsge) {
        MHFree (lpsgd);
        MHFree (lpisge);
        return FALSE;
    }

    // Set up sgd's with pointers into appropriate places in the sge table

    isge = 0;
    for (isgd = 0; isgd < (int) csgd; isgd++) {
        lpsgd[ isgd ].lpsge = lpsge + isge;
        isge += lpsgd[ isgd ].csge;
    }

    // Fill in the sge table

    for (iMod = 1; iMod <= lpexg->cMod; iMod += 1) {
        LPMDS lpmds = &lpexg->rgMod[iMod];
        int cseg = lpmds->csgc;
        int iseg = 0;

        for (iseg = 0; iseg < cseg; iseg++) {
            int isgeT = lpmds->lpsgc[ iseg ].seg - 1;

            if (isgeT != -1) {
                lpsgd[ isgeT ].lpsge[ lpisge[ isgeT ]].sgc =
                    lpmds->lpsgc[ iseg ];
                lpsgd[ isgeT ].lpsge[ lpisge[ isgeT ]].hmod =
                    (HMOD)lpmds;
                lpisge[ isgeT ] += 1;
            }
        }
    }

    MHFree (lpisge);

    *lplpsge = lpsge;
    *lplpsgd = lpsgd;

    return TRUE;
}

//  OLMkModule - make module entries for module
//
//  Entry   lpexg  - Supplies the pointer to the EXG structure for current exe
//          hexg   - Supplies the handle EXG structure
//
//  Returns non-zero for success


int
OLMkModule(
    LPEXG   lpexg,
    HEXG    hexg
    )
{
    LSZ     lszModName;
    LPMDS   lpmds;
    LPB     lpbName;
    WORD    cbName;
    WORD    i;
    OMFModule *pMod;

    // Point to the OMFModule table.  This structure describes the name and
    // segments for the current Module being processed.  There is a one to one
    // correspondance of modules to object files.

    pMod = (OMFModule *) (lpexg->lpbData + lpdssCur->lfo);

    // Point to the name field in the module table.  This location is variable
    // and is dependent on the number of contributuer segments for the module.

    lpbName = ((LPB)pMod) +
      offsetof (OMFModule, SegInfo[0]) +
      (sizeof (OMFSegDesc) * pMod->cSeg);
    cbName = *((LPB)lpbName)++;
    lszModName = (LPCH) MHAlloc (cbName + 1);
    memmove(lszModName, lpbName, cbName);
    *(lszModName + cbName) = 0;

    lpmds = &lpexg->rgMod[lpdssCur->iMod];

    lpmds->imds   = lpdssCur->iMod;
    lpmds->hexg   = hexg;
    lpmds->name   = lszModName;

    // step thru making the module entries
    //
    // NOTENOTE -- This can most likely be optimized as the data
    //          is just being copied from the debug data.

    lpmds->csgc = pMod->cSeg;
    lpmds->lpsgc = (LPSGC)MHAlloc ( pMod->cSeg * sizeof ( SGC ) );

    for ( i = 0; i < pMod->cSeg; i++ ) {
        if ( pMod->SegInfo[i].Seg > csegExe ) {
            csegExe = pMod->SegInfo[i].Seg;
        }
        lpmds->lpsgc[i].seg = pMod->SegInfo[i].Seg;
        lpmds->lpsgc[i].off = pMod->SegInfo[i].Off;
        lpmds->lpsgc[i].cb  = pMod->SegInfo[i].cbSeg;
    }

    return TRUE;
}

SHE
NB10LoadModules(
    LPEXG   lpexg,
    ULONG*  pcMods,
    HEXG    hexg
    )
{
    Mod* pmod = NULL;
    ULONG   cMod = 0;
    IMOD    imod;

    // First count up the number of Mods

    while (DBIQueryNextMod(lpexg->pdbi, pmod, &pmod) && pmod) {
        if (!ModQueryImod(pmod, &imod)) {
            return sheCorruptOmf;
        }
        if (imod > *pcMods) {
            cMod = imod;
        }
    }

    *pcMods = cMod;

    // Got the count.  Allocate rgMod.

    lpexg->rgMod = (LPMDS) MHAlloc((cMod+2) * sizeof(MDS));
    if (lpexg->rgMod == NULL) {
        return sheOutOfMemory;
    }
    memset(lpexg->rgMod, 0, sizeof(MDS)*(cMod+2));
    lpexg->rgMod[cMod+1].imds = (WORD) -1;

    // Now fill in the blanks.

    pmod = NULL;

    for (; cMod; cMod--) {
        LPMDS   lpmds;
        LPCH    lpchName;
        CB      cbName;

        DBIQueryNextMod(lpexg->pdbi, pmod, &pmod);

        if (!ModQueryImod(pmod, &imod)) {
            return sheCorruptOmf;
        }

        lpmds = &lpexg->rgMod[imod];

        lpmds->imds = imod;
        lpmds->pmod = pmod;
        lpmds->hexg = hexg;

        if (!ModQueryName(pmod, NULL, &cbName)) {
            return sheCorruptOmf;
        }
        lpmds->name  = (LSZ) MHAlloc(cbName);
        if(!lpmds->name) {
            return sheOutOfMemory;
        }
        if(!ModQueryName(pmod, lpmds->name, &cbName)) {
            return sheCorruptOmf;
        }
        if(!ModSetPvClient(pmod, lpmds)) {
            return sheCorruptOmf;
        }
    }
    return sheNone;
}


BOOL
OLLoadHashTable(
    LPB     lpbData,
    ULONG   cbTable,
    LPSHT   lpsht,
    BOOL    fDWordChains
    )
{
    WORD    ccib   = 0;
    LPUL    rgib   = NULL;
    LPUL    rgcib  = NULL;
    ULONG   cbHeader = 0;
    LPB     lpHashStart = lpbData;

    memset(lpsht, 0, sizeof(SHT));

    ccib = *(WORD *)lpbData;        // First, get the hash bucket counts
    lpbData += 4;                   // the 2 byte hash count and 2 bytes padding
    rgib = (LPUL) lpbData;

    lpbData += ccib * sizeof(ULONG);

    rgcib = (LPUL) lpbData;

    lpbData += ccib * sizeof(ULONG);

    // Subtract off what we've processed already.

    cbTable     -= (ULONG)(lpbData - lpHashStart);

    lpsht->ccib  = ccib;
    lpsht->rgib  = rgib;
    lpsht->rgcib = rgcib;
    lpsht->lpalm = BuildALM(FALSE,
                            btAlign,
                            lpbData,
                            cbTable,
                            cbAlign);

    if (lpsht->lpalm == NULL) {
        return FALSE;
    }

    return TRUE;
}


BOOL
OLLoadHashSubSec(
    LPGST       lpgst,
    LPB         lpbData,
    WidenTi *   pwti
    )
{
    LPB        lpbTbl = NULL;
    OMFSymHash hash;
    ULONG      cbSymbol;
    BOOL       fRet = TRUE;
    LPSHT      lpshtName = &lpgst->shtName;
    LPSHT      lpshtAddr = &lpgst->shtAddr;

    memset(lpshtAddr, 0, sizeof(SHT));
    memset(lpshtName, 0, sizeof(SHT));

    hash = *(OMFSymHash *)lpbData;

    lpbData += sizeof (OMFSymHash);

    cbSymbol = hash.cbSymbol;

    if (pwti) {
        ConvertGlobal16bitSyms(pwti, lpgst, lpbData, cbSymbol);
    } else {
        lpgst->lpalm = BuildALM(TRUE,
                                btAlign,
                                lpbData,
                                cbSymbol,
                                cbAlign);
    }

    if (lpgst->lpalm == NULL) {
        return FALSE;
    }

    lpbData += cbSymbol;

//    if (hash.symhash == 6 || hash.symhash == 10) {
    if (hash.symhash == 10) {
        fRet = OLLoadHashTable(lpbData,
                               hash.cbHSym,
                               &lpgst->shtName,
                               hash.symhash == 10);
        lpgst->shtName.HashIndex = hash.symhash;
    }

    lpbData += hash.cbHSym;

//    if (hash.addrhash == 8 || hash.addrhash == 12) {
    if (hash.addrhash == 12) {
        fRet = OLLoadHashTable(lpbData,
                               hash.cbHAddr,
                               &lpgst->shtAddr,
                               hash.addrhash == 12);
        lpgst->shtAddr.HashIndex = hash.addrhash;
    }

    return fRet;
}

//  OLLoadTypes - load compacted types table
//
//  Input:  lpexg - Pointer to exg we're working on.
//
//  Returns:    - An error code

SHE
OLLoadTypes(
    LPEXG lpexg
    )
{
    LPB         pTyp;
    LPB         pTypes;

    OMFTypeFlags flags;
    DWORD        cType  = 0;
    DWORD       *rgitd  = NULL;
    DWORD        ibType = 0;

    pTyp = pTypes = lpexg->lpbData + lpdssCur->lfo;

    flags = *(OMFTypeFlags *) pTypes;
    pTypes += sizeof(OMFTypeFlags);
    cType = *(ULONG *) pTypes;
    pTypes += sizeof(ULONG);

    if (!cType) {
        return sheNone;
    }

    // Point to the array of pointers to types

    rgitd = (DWORD *) pTypes;

    // Move past them

    pTypes += cType * sizeof(ULONG);

    // Read in the type index table

    ibType = (DWORD)(pTypes - pTyp);
    lpexg->lpalmTypes = BuildALM (FALSE,
                                  btAlign,
                                  pTypes,
                                  lpdssCur->cb - ibType,
                                  cbAlignType);

    if (lpexg->lpalmTypes == NULL) {
        return sheOutOfMemory;
    }

    lpexg->rgitd = rgitd;
    lpexg->citd  = cType;

    return sheNone;
}


//  OLLoadSym - load symbol information
//
//  error = OLLoadSym (pMod)
//
//  Entry   lpexg - Pointer to exg structure to use.
//
//  Returns sheNone if symbols loaded

SHE
OLLoadSym(
    LPEXG lpexg
    )
{
    // UNDONE: If we run into problems with a stale VC, we'll have to
    //  revert to reading the file on demand.  The expectation is that the
    //  mapped I/O code will just work.

    // lpexg->rgMod[lpdssCur->iMod].symbols = NULL;

    if (lpexg->pwti) {
        // we be converting 16-bit symbols to 32-bit versions.
        SHE                 sheRet = sheOutOfMemory;
        WidenTi *           pwti = lpexg->pwti;
        PMDS                pmod = &lpexg->rgMod[lpdssCur->iMod];
        SymConvertInfo &    sci = pmod->sci;
        PB                  pbSyms = lpexg->lpbData + lpdssCur->lfo;
        CB                  cbSyms = lpdssCur->cb;

        // Remember the signature!
        if (pwti->fQuerySymConvertInfo (sci, pbSyms, cbSyms, sizeof ULONG)) {
            sci.pbSyms = PB(MHAlloc (sci.cbSyms));
            sci.rgOffMap = POffMap(MHAlloc (sci.cSyms * sizeof OffMap));
            if (sci.pbSyms && sci.rgOffMap) {
                memset (sci.pbSyms, 0, sci.cbSyms);
                memset (sci.rgOffMap, 0, sci.cSyms * sizeof OffMap);
                if (pwti->fConvertSymbolBlock (sci, pbSyms, cbSyms, sizeof ULONG) ) {
                    pmod->symbols = sci.pbSyms;
                    *(ULONG*)pmod->symbols = CV_SIGNATURE_C11;
                    pmod->cbSymbols = sci.cbSyms;
                    // REVIEW: What about pmod->ulsym?  how is it used?
                    sheRet = sheNone;
                }
                else {
                    sheRet = sheCorruptOmf;
                }
            }
        }
        return sheRet;
    }
    else
    {
        lpexg->rgMod[lpdssCur->iMod].symbols = lpexg->lpbData + lpdssCur->lfo;
        lpexg->rgMod[lpdssCur->iMod].cbSymbols = lpdssCur->cb;
        lpexg->rgMod[lpdssCur->iMod].ulsym = lpdssCur->lfo;
        return sheNone;
    }
}


__inline SHE
OLLoadSrc(
    LPEXG lpexg
    )
{
    lpexg->rgMod[lpdssCur->iMod].hst = (HST) (lpexg->lpbData + lpdssCur->lfo);
    return sheNone;
}


__inline SHE
OLGlobalPubs(
    LPEXG   lpexg
    )
{
    SHE   she   = sheNone;

    if (!OLLoadHashSubSec (&lpexg->gstPublics,
                           lpexg->lpbData + lpdssCur->lfo,
                           lpexg->pwti)) {
        she = sheOutOfMemory;
    }

    return she;
}


__inline SHE
OLGlobalSym(
    LPEXG   lpexg
    )
{
    SHE   she   = sheNone;

    if (!OLLoadHashSubSec (&lpexg->gstGlobals,
                           lpexg->lpbData + lpdssCur->lfo,
                           lpexg->pwti)) {
        she = sheOutOfMemory;
    }

    return she;
}

SHE
OLLoadSegMap(
    LPEXG   lpexg
    )
{
    LPB lpb;
    SHE sheRet = sheNone;

    if(lpexg->pdbi) {
        CB      cb;

        // load from the pdb
        if(!DBIQuerySecMap(lpexg->pdbi, NULL, &cb) ||
           !(lpb = (LPB) MHAlloc (cb))) {
            sheRet = sheOutOfMemory;
        } else
        if(!DBIQuerySecMap(lpexg->pdbi, lpb, &cb)) {
            MHFree(lpb);
            lpb = NULL;
            sheRet = sheOutOfMemory;
        }
    } else {
        lpb = lpexg->lpbData + lpdssCur->lfo;
    }

    lpexg->lpgsi = lpb;

    return sheRet;
}

SHE
OLLoadNameIndex(
    LPEXG   lpexg
    )
{
    OMFFileIndex *  lpefi;
    WORD            cmod = 0;
    WORD            cfile = 0;
    CB              cb;

    if(lpexg->pdbi) {
        if(!DBIQueryFileInfo(lpexg->pdbi, 0, &cb)) {
            return sheNoSymbols;
        }
        else if(!(lpefi = (OMFFileIndex *) MHAlloc(cb))) {
            return sheOutOfMemory;
        }
        else if(!DBIQueryFileInfo(lpexg->pdbi, (PB)lpefi, &cb)) {
            MHFree(lpefi);
            return sheNoSymbols;
        }
    } else {
        lpefi = (OMFFileIndex *)(lpexg->lpbData + lpdssCur->lfo);
        cb = (CB)lpdssCur->cb;
    }

    cmod  = lpefi->cmodules;
    // Make sure we found as many sstModule entries as we should have.
    assert(cmod == lpexg->cMod);
//    lpexg->cmod      = cmod;
    cfile = lpefi->cfilerefs;

    lpexg->lpefi     = (LPB) lpefi;
    lpexg->rgiulFile = lpefi->modulelist;
    lpexg->rgculFile = &lpefi->modulelist [cmod];
    lpexg->rgichFile = (LPUL) &lpefi->modulelist [cmod * 2];

    lpexg->lpchFileNames = (LPCH) &lpefi->modulelist [cmod * 2 + cfile * 2];

    lpexg->cbFileNames =
        (ULONG)(cb - ((LPB)lpexg->lpchFileNames - (LPB)lpefi + 1));

    return sheNone;
}

SHE
OLStaticSym(
    LPEXG   lpexg
    )
{
    SHE   she   = sheNone;

    if (!OLLoadHashSubSec (&lpexg->gstStatics,
                           lpexg->lpbData + lpdssCur->lfo,
                           lpexg->pwti)) {
        she = sheOutOfMemory;
    }

    return she;

}


const SHE mpECToShe[] = {
    sheNone,            // EC_OK
    sheNoSymbols,       // EC_USAGE (invalid parameter of call order)
    sheOutOfMemory,     // EC_OUT_OF_MEMORY (-, out of RAM)
    sheNoSymbols,       // EC_FILE_SYSTEM (pdb name, can't write file, out of disk, etc)
    shePdbNotFound,     // EC_NOT_FOUND (PDB file not found)
    shePdbBadSig,       // EC_INVALID_SIG (PDB::OpenValidate() and its clients only)
    shePdbInvalidAge,   // EC_INVALID_AGE (PDB::OpenValidate() and its clients only)
    sheNoSymbols,       // EC_PRECOMP_REQUIRED (obj name, Mod::AddTypes only)
    sheNoSymbols,       // EC_OUT_OF_TI (pdb name, TPI::QueryTiForCVRecord() only)
    sheNoSymbols,       // EC_NOT_IMPLEMENTED
    sheNoSymbols,       // EC_V1_PDB (pdb name, PDB::Open() only)
    shePdbOldFormat,    // EC_FORMAT (accessing pdb with obsolete format)
    sheNoSymbols,       // EC_LIMIT,
    sheNoSymbols,       // EC_CORRUPT,             // cv info corrupt, recompile mod
    sheNoSymbols,       // EC_TI16,                // no 16-bit type interface present
    sheNoSymbols,       // EC_ACCESS_DENIED,       // "pdb name", PDB file read-only
};

// Get the name of the pdb file (OMF name) for the specified exe.  If the
// LoadPdb hasn't been called on this exe OR it's not NB10, this will return
// an empty string!  Note: There will only be an lszPdbName if there was
// an error loading the pdb

VOID LOADDS
SHPdbNameFromExe(
    LSZ lszExe,
    LSZ lszPdbName,
    UINT cbMax
    )
{
    HEXE    hexe;

    // Zero out the destination
    memset(lszPdbName, 0, cbMax);

    // Look up the exe
    if (hexe = SHGethExeFromName(lszExe)) {
        HEXG    hexg = ((LPEXE)LLLock(hexe))->hexg;
        LPEXG   lpexg = (LPEXG)LLLock(hexg);

        // Only copy if there's a pdbname
        if (lpexg->lszPdbName) {
            _tcsncpy(lszPdbName, lpexg->lszPdbName, cbMax);
        }

        // Clean up
        LLUnlock(hexg);
        LLUnlock(hexe);
    }
}


BOOL
GetDefaultKeyName(
    LPCTSTR KeySuffix,
    LPTSTR  KeyBuffer,
    DWORD   nLength
    )
{
    DWORD   lnLength = nLength;

    if (!GetRegistryRoot (KeyBuffer, &lnLength))
        return FALSE;

    if (KeyBuffer [lnLength - 1] != '\\') {
        KeyBuffer [lnLength++] = '\\';
        KeyBuffer [lnLength] = '\000';
    }

    assert (*KeySuffix != '\\');
    _tcscpy (&KeyBuffer [lnLength], KeySuffix);

    return TRUE;
}

PDB *
LocatePdb(
    char *szPDB,
    ULONG PdbAge,
    ULONG PdbSignature,
    char *SymbolPath,
    char *szImageExt,
    BOOL fValidatePdb
    )
/*++
Description:

Arguments:
    fValidatePdb -  TRUE - call PDBOpenValidate
                    FALSE - call PDBOpen
--*/
{
    PDB *pdb = NULL;
    EC  ec;
    char szError[cbErrMax] = "";
    char szPDBSansPath[_MAX_FNAME];
    char szPDBExt[_MAX_EXT];
    char szPDBLocal[_MAX_PATH];
    char *SemiColon;
    BOOL  fPass1;
	BOOL  symsrv = TRUE;
    char  szPDBName[_MAX_PATH];


    // SymbolPath is a semicolon delimited path (reference path first)

    strcpy (szPDBLocal, szPDB);
    _splitpath(szPDBLocal, NULL, NULL, szPDBSansPath, szPDBExt);

    do {
        SemiColon = strchr(SymbolPath, ';');

        if (SemiColon) {
            *SemiColon = '\0';
        }

        fPass1 = TRUE;
        if (SymbolPath) {
do_again:
            if (!_strnicmp(SymbolPath, "SYMSRV*", 7)) {
                
                *szPDBLocal = 0;
                sprintf(szPDBName, "%s%s", szPDBSansPath, ".pdb");
                if (symsrv) {
                    GetSymbolFileFromServer(SymbolPath, 
                                            szPDBName, 
                                            PdbSignature,
                                            PdbAge,
                                            0,
                                            szPDBLocal);
                    symsrv = FALSE;
                }
            } else {
				ExpandEnvironmentStringsA(SymbolPath, szPDBLocal, sizeof(szPDBLocal));
				strcat(szPDBLocal, "\\");
					if (fPass1) {
						strcat(szPDBLocal, szImageExt);
						strcat(szPDBLocal, "\\");
					}
				strcat(szPDBLocal, szPDBSansPath);
				strcat(szPDBLocal, szPDBExt);
			}
            if (fValidatePdb) {
                PDBOpenValidate(szPDBLocal, 
                                NULL, 
                                "r",  
                                PdbSignature, 
                                PdbAge, 
                                &ec, 
                                szError, 
                                &pdb
                                );
            } else {
                PDBOpen(szPDBLocal, 
                        "r", 
                        PdbSignature, 
                        &ec, 
                        szError, 
                        &pdb
                        );
            }
            
            // UNDONE: Add DEBUG Diagnostics

            if (pdb) {
                break;
            } else {
                if (fPass1) {
                    fPass1=FALSE;
                    goto do_again;
                }
            }
        }

        if (SemiColon) {
            *SemiColon = ';';
             SemiColon++;
			 symsrv = TRUE;
        
        }

        SymbolPath = SemiColon;
    } while (SemiColon);

    if (!pdb) {
        strcpy(szPDBLocal, szPDB);
        if (fValidatePdb) {
            PDBOpenValidate(szPDBLocal, 
                            NULL, 
                            "r", 
                            PdbSignature, 
                            PdbAge, 
                            &ec, 
                            szError, 
                            &pdb
                            );
        } else {
            PDBOpen(szPDBLocal, 
                    "r", 
                    PdbSignature, 
                    &ec, 
                    szError, 
                    &pdb
                    );
        }
        // UNDONE: Add DEBUG Diagnostics
    }

    if (pdb) {
        // Store the name of the PDB we actually opened for later reference.
        strcpy(szPDB, szPDBLocal);
    }

    return pdb;
}


SHE
LoadPdb(
    LPEXG     lpexg,
    PDB_INFO *ppdb,
    UOFFSET   ImageBase,
    BOOL      fValidatePdb
    )
/*++
Description:

Arguments:
    fValidatePdb - TRUE - Open and validate the PDB using the images age and signature
                   FALSE - Just open the PDB
--*/
{
    EC ec;
    char szRefPath[_MAX_PATH];
    char szImageExt[_MAX_PATH];
    char szPDBOut[cbErrMax];
    ULONG       cbRet;

    assert(lpexg);

    // figure out the home directory of the EXE/DLL or DBG file - pass that along to
    // OpenValidate this will direct to dbi to search for it in that directory.

    _fullpath(szRefPath, lpexg->lszDebug ? lpexg->lszDebug : lpexg->lszName, _MAX_PATH);
    char *pcEndOfPath = _tcsrchr(szRefPath, '\\');
    *pcEndOfPath = '\0';        // null terminate it
    *szPDBOut = '\0';

    _splitpath(lpexg->lszName, NULL, NULL, NULL, szImageExt);


    if (!SYGetProfileString(szPdbDirs,
                            szSearchPath,
                            sizeof(szSearchPath),
                            &cbRet)) {
        szSearchPath[0] = 0;
    }

    PCHAR pszPdbSearchPath = (PCHAR)MHAlloc(strlen(szRefPath) + strlen(szSearchPath) + 2);
    if (!pszPdbSearchPath) {
        return sheOutOfMemory;
    }

    strcpy(pszPdbSearchPath, szRefPath);
    strcat(pszPdbSearchPath, ";");
    strcat(pszPdbSearchPath, szSearchPath);

    lpexg->ppdb = LocatePdb(ppdb->sz, 
                            ppdb->age, 
                            ppdb->sig, 
                            pszPdbSearchPath, 
                            &szImageExt[1],
                            fValidatePdb
                            );

    if (!lpexg->ppdb) {
        return sheNoSymbols;
    }

    MHFree(pszPdbSearchPath);

    // Store the name of the pdb in lszDebug.

    char *szPdb = PDBQueryPDBName(lpexg->ppdb, (char *)szPDBOut);
    assert(szPdb);

    // Save the name of the PDB

    if (lpexg->lszDebug) {
        MHFree(lpexg->lszDebug);
        lpexg->lszDebug = 0;
    }

    if (!(lpexg->lszDebug = (LSZ)MHAlloc(_tcslen(szPDBOut) + 1))) {
        return sheOutOfMemory;
    }

    _tcscpy(lpexg->lszDebug, szPDBOut);

    if (!PDBOpenTpi(lpexg->ppdb, pdbRead, &(lpexg->ptpi))) {
        ec = PDBQueryLastError(lpexg->ppdb, NULL);
        return mpECToShe[ec];
    }

    if (!PDBOpenDBIEx(lpexg->ppdb, pdbRead, lpexg->lszName, &(lpexg->pdbi), pSYFindDebugInfoFile)) {
        ec = PDBQueryLastError(lpexg->ppdb, NULL);
        return mpECToShe[ ec ];
    }

    if (!STABOpen(&(lpexg->pstabUDTSym)))
        return sheOutOfMemory;

    // Read in the omap.

    int     DebugCount;
    void   *DebugData;
    Dbg    *pDbg;

    if (DBIOpenDbg(lpexg->pdbi, dbgtypeOmapFromSrc, &pDbg)) {
        DebugCount = DbgQuerySize(pDbg);
        if (DebugCount) {
            DebugData = MHAlloc( DebugCount * sizeof(OMAP) );
            if (!DebugData)
                return sheOutOfMemory;

            if (DbgQueryNext((Dbg *) pDbg, DebugCount, DebugData)) {
                lpexg->debugData.lpOmapFrom = (LPOMAP) DebugData;
                lpexg->debugData.cOmapFrom = DebugCount;
            }
        }
        DbgClose(pDbg);
    }

    if (DBIOpenDbg(lpexg->pdbi, dbgtypeOmapToSrc, &pDbg)) {
        DebugCount = DbgQuerySize(pDbg);
        if (DebugCount) {
            DebugData = MHAlloc( DebugCount * sizeof(OMAP) );
            if (!DebugData)
                return sheOutOfMemory;

            if (DbgQueryNext((Dbg *) pDbg, DebugCount, DebugData)) {
                lpexg->debugData.lpOmapTo = (LPOMAP)DebugData;
                lpexg->debugData.cOmapTo = DebugCount;
            }
        }
        DbgClose(pDbg);
    }

    // Read in the fpo (if it exists)

    if (!lpexg->debugData.lpFpo) {
        if (DBIOpenDbg(lpexg->pdbi, dbgtypeFPO, &pDbg)) {
            DebugCount = DbgQuerySize(pDbg);
            if (DebugCount) {
                DebugData = MHAlloc( DebugCount * sizeof(FPO_DATA) );
                if (!DebugData)
                    return sheOutOfMemory;

                if (DbgQueryNext((Dbg *) pDbg, DebugCount, DebugData)) {
                    lpexg->debugData.lpFpo = (PFPO_DATA)DebugData;
                    lpexg->debugData.cRtf = DebugCount;
                }
            }
            DbgClose(pDbg);
        }
    }

    if (!lpexg->debugData.lpRtf) {
        if (DBIOpenDbg(lpexg->pdbi, dbgtypeException, &pDbg)) {
            DebugCount = DbgQuerySize(pDbg);
            if (DebugCount) {

                DWORD dwSizRTFEntry;

                switch (lpexg->machine) {
                    case IMAGE_FILE_MACHINE_ALPHA:
                        dwSizRTFEntry = sizeof(IMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY);
                        break;

                    case IMAGE_FILE_MACHINE_ALPHA64:
                        dwSizRTFEntry = sizeof(IMAGE_AXP64_RUNTIME_FUNCTION_ENTRY);
                        break;

                    case IMAGE_FILE_MACHINE_IA64:
                        dwSizRTFEntry = sizeof(IMAGE_IA64_RUNTIME_FUNCTION_ENTRY);
                        break;
                        
                    default:
                        assert(!"Unsupported platform");
                        break;
                }

                DebugData = MHAlloc( DebugCount * dwSizRTFEntry);
                if (!DebugData)
                    return sheOutOfMemory;

                if (DbgQueryNext((Dbg *) pDbg, DebugCount, DebugData)) {
                    lpexg->debugData.lpRtf = CreateRuntimeFunctionTable(DebugData, DebugCount, ImageBase, dwSizRTFEntry);
                    lpexg->debugData.cRtf  = DebugCount;
                }
                MHFree(DebugData);
            }
            DbgClose(pDbg);
        }
    }

    return sheNone;
}

// Routine Description:
//
//  This routine is used to validate that the debug information
//  in a file matches the debug information requested
//
// Arguments:
//
//  hFile       - Supplies the file handle to be validated
//  lpv         - Supplies a pointer to the information to used in vaidation
//
// Return Value:
//
//  TRUE if matches and FALSE otherwise

SHE
OLValidate(
    HANDLE       hFile,
    PVLDCHK      pVldChk
    )
{
    IMAGE_DOS_HEADER    exeHdr;
    IMAGE_NT_HEADERS    peHdr;
    int                 fPeExe = FALSE;
    int                 fPeDbg = FALSE;

    // Read in a dos exe header

    if ((SYSeek(hFile, 0, SEEK_SET) != 0) ||
        (SYReadFar( hFile, (LPB) &exeHdr, sizeof(exeHdr)) != sizeof(exeHdr))) {
        return sheNoSymbols;
    }

    // See if it is a dos exe hdr

    if (exeHdr.e_magic == IMAGE_DOS_SIGNATURE) {
        if ((SYSeek(hFile, exeHdr.e_lfanew, SEEK_SET) == exeHdr.e_lfanew) &&
            (SYReadFar(hFile, (LPB) &peHdr, sizeof(peHdr)) == sizeof(peHdr))) {
            if (peHdr.Signature == IMAGE_NT_SIGNATURE) {
                fPeExe = TRUE;
            }
        }
    } else if (exeHdr.e_magic == IMAGE_SEPARATE_DEBUG_SIGNATURE) {
        fPeDbg = TRUE;
    }

    if (fPeExe) {
        if (pVldChk->ImgTimeDateStamp == 0) {
            // If the timestamp has not been initialized do so now.
            pVldChk->ImgTimeDateStamp = peHdr.FileHeader.TimeDateStamp;
        }

        if (peHdr.FileHeader.Characteristics & IMAGE_FILE_DEBUG_STRIPPED) {
            return sheNoSymbols;
        }

        if (pVldChk->ImgTimeDateStamp == 0xffffffff) {

            pVldChk->SymTimeDateStamp = peHdr.FileHeader.TimeDateStamp;

            return sheCouldntReadImageHeader;

        } else if (peHdr.FileHeader.TimeDateStamp != pVldChk->ImgTimeDateStamp) {

            pVldChk->SymTimeDateStamp = peHdr.FileHeader.TimeDateStamp;

            return sheBadTimeStamp;
        }
    } else if (fPeDbg) {
        IMAGE_SEPARATE_DEBUG_HEADER sepHdr;

        if ((SYSeek(hFile, 0, SEEK_SET) != 0) ||
            (SYReadFar(hFile, (LPB) &sepHdr, sizeof(sepHdr)) != sizeof(sepHdr))) {
            return sheNoSymbols;
        }

        pVldChk->SymTimeDateStamp = sepHdr.TimeDateStamp;
        pVldChk->SymCheckSum = sepHdr.CheckSum;

        if (pVldChk->ImgTimeDateStamp == 0xffffffff && pVldChk->ImgCheckSum == 0xffffffff) {

            //
            // The timestamp is frequently unavailable, so don't return an error unless
            // both the timestamp and checksum are unavailable.
            //
            return sheCouldntReadImageHeader;
        }

        if (pVldChk->ImgTimeDateStamp != 0xffffffff &&
            sepHdr.TimeDateStamp != pVldChk->ImgTimeDateStamp) {

            return sheBadTimeStamp;
        }

        if (pVldChk->ImgCheckSum != 0xffffffff &&
            sepHdr.CheckSum != pVldChk->ImgCheckSum) {

            return sheBadChecksum;
        }
    } else {
        //
        // When debugging a ROM image, this hack will find what we are
        // looking for.  It doesn't validate correctly, so don't be
        // surprised if we get in trouble when the symbols aren't good.
        //

        char rgch[4];

        if ((SYSeek(hFile, -8, SEEK_END) == -1) ||
            (SYReadFar(hFile, (LPB)rgch, sizeof(rgch)) != sizeof(rgch))) {
            return sheNoSymbols;
        }

        if ((rgch[0] != 'N') || (rgch[1] != 'B')) {
            return sheNoSymbols;
        }
    }

    return sheNone;
}


BOOL
OLUnloadOmf(
    LPEXG lpexg
    )
{
    ULONG i;
    // Cleanup the Module table;
    for (i = 0; i < lpexg->cMod; i++) {
        KillMdsNode(&lpexg->rgMod[i]);
    }

    if (lpexg->rgMod) {
        MHFree(lpexg->rgMod);
        lpexg->rgMod = NULL;
        lpexg->cMod = 0;
    }

    // module map info
    if (lpexg->lpsgd) {
        MHFree(lpexg->lpsgd);
        lpexg->lpsgd = NULL;
        lpexg->csgd = 0;
    }

    //
    if (lpexg->lpsge) {
        MHFree(lpexg->lpsge);
        lpexg->lpsge = NULL;
    }

    if (lpexg->lpbData) {
        // Depending on how we got the data, free it.

        if (lpexg->pvSymMappedBase) {
            // Mapped view of file.
            UnmapViewOfFile(lpexg->pvSymMappedBase);
            lpexg->pvSymMappedBase = NULL;
        } else {
            if (lpexg->fSymConverted) {
                // Converted from coff/sym file
                LocalFree(lpexg->lpbData);
            } else {
                // Read the blob in from disk
                MHFree(lpexg->lpbData);
            }
        }

        lpexg->lpbData = NULL;
    }

    // OSDebug 4 FPO info
    if (lpexg->debugData.lpRtf) {
        MHFree(lpexg->debugData.lpRtf);
        lpexg->debugData.lpRtf = NULL;
    }

    if (lpexg->debugData.lpOmapFrom) {
        MHFree(lpexg->debugData.lpOmapFrom);
        lpexg->debugData.lpOmapFrom = NULL;
    }

    if (lpexg->debugData.lpOmapTo) {
        MHFree(lpexg->debugData.lpOmapTo);
        lpexg->debugData.lpOmapTo = NULL;
    }

    if (lpexg->debugData.lpSecStart) {
        MHFree(lpexg->debugData.lpSecStart);
        lpexg->debugData.lpSecStart = NULL;
    }

    // Segment map info
    if (lpexg->lpgsi) {
        if (lpexg->ppdb) {
            MHFree (lpexg->lpgsi);
        }
        lpexg->lpgsi = NULL;
    }

    // Source Module information
    if (lpexg->lpefi) {
        if (lpexg->ppdb) {
            MHFree(lpexg->lpefi);
        }
        lpexg->lpefi = NULL;
    }

    // Type Info array

    lpexg->citd = 0;
    lpexg->rgitd = NULL;

    // Publics, Globals, and Statics

    KillGst(&lpexg->gstPublics);
    KillGst(&lpexg->gstGlobals);
    KillGst(&lpexg->gstStatics);

    // If there's PDB info, clean up and close

    if (lpexg->ppdb) {
        if (lpexg->pgsiPubs) {
            if (!GSIClose(lpexg->pgsiPubs)) {
                assert(FALSE);
            }
            lpexg->pgsiPubs = 0;
        }

        if (lpexg->pgsiGlobs) {
            if (!GSIClose(lpexg->pgsiGlobs)) {
                assert(FALSE);
            }
            lpexg->pgsiGlobs = 0;
        }

        if (lpexg->pdbi) {
            if (!DBIClose(lpexg->pdbi)) {
                assert(FALSE);
            }
            lpexg->pdbi = 0;
        }

        if (lpexg->ptpi) {
            if (!TypesClose(lpexg->ptpi)) {
                assert(FALSE);
            }
            lpexg->ptpi = 0;
        }

        if (lpexg->pstabUDTSym) {
            STABClose(lpexg->pstabUDTSym);
            lpexg->pstabUDTSym = 0;
        }

        if (!PDBClose(lpexg->ppdb)) {
            assert(FALSE);
        }

        lpexg->ppdb = 0;
    }

    lpexg->fOmfLoaded = 0;

    return TRUE;
}

cassert(offsetof(OffMap,offOld) == 0);

int __cdecl
sgnCompareOffsFromOffMap(
    const void *  pOff1,
    const void *  pOff2
    )
{
    ULONG   off1 = POffMap(pOff1)->offOld;
    ULONG   off2 = POffMap(pOff2)->offOld;

    if (off1 < off2)
        return -1;
    if (off1 > off2)
        return 1;
    return 0;
}

void
FixupHash(
    SymConvertInfo &    sci,
    SHT &               sht
    )
{
    // for every offset in the hash, we need to fixup the offset which
    // references the old 16-bit pool with the associated new one for the
    // new pool of 32-bit types.
    assert(sht.HashIndex);
    assert(sci.cSyms);
    assert(sci.rgOffMap);

    unsigned    iBucketMax = sht.ccib;

    for (unsigned iBucket = 0; iBucket < iBucketMax; iBucket++) {
        unsigned    offChain = sht.rgib[iBucket];
        unsigned    iulpMax = sht.rgcib[iBucket];
        for (unsigned iulp = 0; iulp < iulpMax; iulp++, offChain += sizeof ULP ) {
            LPULP   pulp = LPULP(LpvFromAlmLfo(sht.lpalm, offChain));

            POffMap poffmap = POffMap(
                bsearch(
                    &pulp->ib,
                    sci.rgOffMap,
                    sci.cSyms,
                    sizeof OffMap,
                    sgnCompareOffsFromOffMap
                    )
                );
            // we should always find it
            //assert(poffmap);
            if (poffmap) {
                pulp->ib = poffmap->offNew;
            }
        }
    }
}

//
// this routine will do all the post processing for the inter-relationships
// that exist between the module symbols and the global syms.  The global
// syms include procref and dataref records that include offsets into the
// module symbols.  We have to fix up those offsets to refer to the new
// offsets.  Each of these mappings is stored in the MDS structure and can
// be released after this operation.
//
// the hash offsets for the globals, publics, and statics are also fixed up here
//
void
FixupGst(
    LPEXG   pexg,
    GST &   gst,
    BOOL    fFixupRefSyms
    )
{
    assert(pexg->pwti);

    if (!gst.lpalm)
        return;

    // first off, we check to see if we need to and can fixup the refsyms
    // that may be present.
    if (fFixupRefSyms) {
        for (
            SYMPTR  psym = SYMPTR(gst.lpalm->pbData);
            psym;
            psym = GetNextSym(psym, gst.lpalm)
        ) {
            unsigned    rectyp = psym->rectyp;
            if (rectyp == S_PROCREF || rectyp == S_LPROCREF || rectyp == S_DATAREF) {
                // fix up the ibSym from the module's array of offset mappings
                REFSYM &            refsym = *(REFSYM *)psym;
                SymConvertInfo &    sci = pexg->rgMod[refsym.imod].sci;

                POffMap poffmap = POffMap(
                    bsearch(
                        &refsym.ibSym,
                        sci.rgOffMap,
                        sci.cSyms,
                        sizeof OffMap,
                        sgnCompareOffsFromOffMap
                        )
                    );
                // we should always find it.
                assert(poffmap);
                if (poffmap) {
                    refsym.ibSym = poffmap->offNew;
                }
            }
        }
    }

    // next, we check our hash tables and fix up all of the offsets there.
    if (gst.shtName.HashIndex) {
        // fixup name hash
        FixupHash(gst.sci, gst.shtName);
    }
    if (gst.shtAddr.HashIndex) {
        // fixup address hash
        FixupHash(gst.sci, gst.shtAddr);
    }
}


SHE
SheFixupConvertedSyms(
    LPEXG   pexg
    )
{
    // for each of the symbol blocks, iterate over all symbols,
    // and if they are REFSYMs of some sort, we go to the appropriate
    // module's sci.rgoffmap to find what new offset we need to plug into
    // the REFSYM.ibSym field.

    FixupGst(pexg, pexg->gstGlobals, TRUE);
    FixupGst(pexg, pexg->gstStatics, TRUE);
    FixupGst(pexg, pexg->gstPublics, FALSE);

    // we can safely get rid of all of our offmap buffers.

    // first, the module ones.
    unsigned    imod = 0;
    unsigned    imodMax = pexg->cMod;

    while (imod < imodMax) {
        MDS &   mds = pexg->rgMod[imod];
        if (mds.sci.rgOffMap) {
            MHFree(mds.sci.rgOffMap);
            mds.sci.rgOffMap = 0;
        }
        imod++;
    }

    // now, the gst versions
    if (pexg->gstGlobals.sci.rgOffMap) {
        MHFree(pexg->gstGlobals.sci.rgOffMap);
        pexg->gstGlobals.sci.rgOffMap = 0;
    }

    if (pexg->gstStatics.sci.rgOffMap) {
        MHFree(pexg->gstStatics.sci.rgOffMap);
        pexg->gstStatics.sci.rgOffMap = 0;
    }

    if (pexg->gstPublics.sci.rgOffMap) {
        MHFree(pexg->gstPublics.sci.rgOffMap);
        pexg->gstPublics.sci.rgOffMap = 0;
    }

    return sheNone;
}

void
ConvertGlobal16bitSyms(
    WidenTi *           pwti,
    LPGST               pgst,
    PB                  pbSymSrc,
    ULONG               cbSymSrc
    )
{
    SymConvertInfo &    sci = pgst->sci;

    memset ( &sci, 0, sizeof sci );
    if (pwti->fQuerySymConvertInfo(sci, pbSymSrc, cbSymSrc)) {
        // allocate the needed memory
        sci.pbSyms = PB(MHAlloc(sci.cbSyms));
        sci.rgOffMap = POffMap(MHAlloc(sci.cSyms * sizeof OffMap));
        if (sci.pbSyms && sci.rgOffMap) {
            memset(sci.pbSyms, 0, sci.cbSyms);
            memset(sci.rgOffMap, 0, sci.cSyms * sizeof OffMap);
            if (pwti->fConvertSymbolBlock(sci, pbSymSrc, cbSymSrc)) {
                // all cool, set up the ALM.
                pgst->lpalm = BuildALM(
                    FALSE,
                    0,
                    sci.pbSyms,
                    sci.cbSyms,
                    cbAlign);
            }
        }
    }

}


////    LazyLoader
//
//  Description:
//      This routine is used to do lazy loads and unloads of debug
//      information.

DWORD
LazyLoader(
    LPVOID
    )
{
    BOOL        fNoChanges;
    FILETIME    ftNow;
    LONGLONG    llNow;
    HEXG        hexg;
    LPEXG       pexg;

    //
    //  We loop forever until told to die
    //

    for (; TRUE; ) {
        //
        //  Wait for someone to tell us that we some work to do, or
        //      a suffient amount of time has passed that we should go
        //      out and check for defered unloads
        //
        //  Current time out on lazy unloads is 10 minutes
        //

        if (WaitForSingleObject(HevntLazyLoad, 1000*60*10) == WAIT_OBJECT_0) {
            //
            //  If we have been told to die -- die
            //

            if (FKillLazyLoad) {
                break;
            }

            //
            //  We were just told that a dll has been loaded and we should
            //  check symbols -- wait for a minute just incase the symbols
            //  are immeadiately demand loaded
            //

            Sleep(10*1000);
        }

        //
        //  Walk the entire list of exe's looking for modules to be loaded
        //      and modules to be unloaded.
        //

        GetSystemTimeAsFileTime(&ftNow);
        memcpy(&llNow, &ftNow, sizeof(LONGLONG));

        do {
            hexg = LLFind(HlliExgExe, 0, &llNow, 2);

            if (hexg != NULL) {
                pexg = (LPEXG) LLLock(hexg);

                //
                //  Check for defer load modules
                //

                if (pexg->fOmfDefered) {
                    LLUnlock(hexg);
                    LoadSymbols(hexg, TRUE);
                }

                //
                //  Check for defered unload of symbols
                else if (pexg->llUnload != 0) {
                    LLUnlock(hexg);
                    LLDelete(HlliExgExe, hexg);
                }
            }
        } while (hexg != NULL);
    }
    return 1;
}

BOOL
StartLazyLoader(
    void
    )
{
    //
    //  Setup the items used for lazy load of symbols
    //

    InitializeCriticalSection(&CsExeModify);
    HevntLazyLoad = CreateEvent(NULL, FALSE, FALSE, NULL);

    //HthdLazyLoad = CreateThread(NULL, 0, LazyLoader, NULL, 0, NULL);
//    SetThreadPriority(HthdLazyLoad, THREAD_PRIORITY_IDLE);

    return TRUE;
}


VOID
StopLazyLoader(
    void
    )
{
    //
    //  Tell the lazy loader it is suppose to die and then start the thread
    //  going again
    //

    FKillLazyLoad = TRUE;
    SetEvent(HevntLazyLoad);

    //
    //  Wait for the lazy loaded to die
    //

    //WaitForSingleObject(HthdLazyLoad, INFINITE);

    //
    //  Close and zero the handles
    //

    CloseHandle(HevntLazyLoad);
    HevntLazyLoad = NULL;
    //CloseHandle(HthdLazyLoad);
    HthdLazyLoad = NULL;

    DeleteCriticalSection(&CsExeModify);

    return;
}
