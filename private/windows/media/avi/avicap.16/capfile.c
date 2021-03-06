/****************************************************************************
 *
 *   capfile.c
 * 
 *   AVI file writing module.
 *
 *   Microsoft Video for Windows Sample Capture Class
 *
 *   Copyright (c) 1992, 1993 Microsoft Corporation.  All Rights Reserved.
 *
 *    You have a royalty-free right to use, modify, reproduce and 
 *    distribute the Sample Files (and/or any modified version) in 
 *    any way you find useful, provided that you agree that 
 *    Microsoft has no warranty obligations or liability for any 
 *    Sample Application Files which are modified. 
 *
 ***************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <msvideo.h>
#include <avifmt.h>
#include <drawdib.h>
#include "avicap.h"
#include "avicapi.h"


/*----------------------------------------------------------------------+
| fileCapFileIsAVI() - Returns TRUE if the capture file is a valid AVI |
|                                   |
+----------------------------------------------------------------------*/
BOOL FAR PASCAL fileCapFileIsAVI (LPSTR lpsz)
{
    BOOL        fReturn = TRUE;
    HMMIO       hmmioSource = NULL;
    MMCKINFO    ckRIFF;
    
    // Does the file exist?
    hmmioSource = mmioOpen(lpsz, NULL, MMIO_READ);
    if (!hmmioSource)
        return FALSE;
    
    // Is there an AVI RIFF chunk?
    // !!! Don't do a FINDRIFF for an AVI chunk or it'll take several minutes to
    // !!! come back from checking a really big file
    fReturn = (mmioDescend(hmmioSource, &ckRIFF, NULL, 0) == 0) &&
		(ckRIFF.ckid == FOURCC_RIFF) &&
		(ckRIFF.fccType == formtypeAVI);

    if (hmmioSource)
        mmioClose(hmmioSource, 0);

    return fReturn;
}

/*----------------------------------------------------------------------+
| fileSaveCopy() - save a copy of the current capture file.     |
|                                   |
+----------------------------------------------------------------------*/
BOOL FAR PASCAL fileSaveCopy(LPCAPSTREAM lpcs)
{
#define INITFILE_BUFF_SIZE  (1024L * 256L)
    BOOL        fReturn = TRUE;
    char        achCaption[80]; // caption on Open File dialog
    
    HMMIO       hmmioSource = NULL, hmmioDest = NULL;
    LONG        lFileSize, lFileSizeTotal, lTemp;
    HANDLE      hMem = NULL;
    LPSTR       lpstr = NULL;
    LONG        lBuffSize = INITFILE_BUFF_SIZE;
    MMCKINFO    ckRIFF;
    HCURSOR     hOldCursor;

    
    /* grab a big buffer to xfer the file in, start the */
    /* buffer size at 32K and hope we get that much.    */
TRYAGAIN:    
    hMem = GlobalAlloc(GMEM_MOVEABLE, lBuffSize);
    if (!hMem){
        /* we don't have this much mem, go for half that */
        lBuffSize /= 2;
        if (lBuffSize)
            goto TRYAGAIN;
        else {
            fReturn = FALSE;
            goto SAVECOPYOUT;
        }
    }

    /* let's go and create the destination file */
    hmmioDest = mmioOpen(lpcs->achSaveAsFile, NULL, MMIO_CREATE|MMIO_WRITE);
    if (!hmmioDest){
        /* we've got an error of some kind here, let's bail out */
        /* on this one.                     */
        errorUpdateError (lpcs, IDS_CAP_CANTOPEN, (LPSTR)lpcs->achSaveAsFile);
        fReturn = FALSE;
        goto SAVECOPYOUT;
    }
    
    /* open up the source file and find the size */
    hmmioSource = mmioOpen(lpcs->achFile, NULL, MMIO_READ);
    if (!hmmioSource){
        /* we are totally hosed here, the source file can't even */
        /* be opened up, error out.              */
        errorUpdateError (lpcs, IDS_CAP_CANTOPEN, (LPSTR)lpcs->achFile);
        fReturn = FALSE;
        goto SAVECOPYOUT;
    }
    
    /* go down to the RIFF chunk and find out the size of this  */
    /* thing.  If there is no RIFF chunk then we can safely */
    /* assume that the file is of 0 length.         */
    ckRIFF.fccType = formtypeAVI;
    if (mmioDescend(hmmioSource, &ckRIFF, NULL, MMIO_FINDRIFF) != 0){
        /* we are done, this file has no RIFF chunk so it's size */
        /* is 0 bytes.  Just close up and leave.         */
        goto SAVECOPYOUT;
    } else {
        /* there is a RIFF chunk, get the size of the file and  */
        /* get back to the start of the file.           */
        lFileSizeTotal = lFileSize = ckRIFF.cksize + 8;
        mmioAscend(hmmioSource, &ckRIFF, 0);
        mmioSeek(hmmioSource, 0L, SEEK_SET);
    }
    
    /* Before trying to write, seek to the end of the destination  */
    /* file and write one byte.  This both preallocates the file,  */
    /* and confirms enough disk is available for the copy, without */
    /* going through the trial and error of writing each byte.     */

    mmioSeek( hmmioDest, lFileSizeTotal - 1, SEEK_SET );
    mmioWrite( hmmioDest, (HPSTR) achCaption, 1L );
    if (mmioSeek (hmmioDest, 0, SEEK_END) < lFileSizeTotal) {

        /* Notify user with message that disk may be full. */
        errorUpdateError (lpcs, IDS_CAP_WRITEERROR, (LPSTR)lpcs->achSaveAsFile);
    
        /* close the file and delete it */
        mmioClose(hmmioDest, 0);
        mmioOpen(lpcs->achSaveAsFile, NULL, MMIO_DELETE);
        hmmioDest = NULL;
        fReturn = FALSE;
        goto SAVECOPYOUT;
    }
    
    mmioSeek (hmmioDest, 0L, SEEK_SET); // Back to the beginning

    UpdateWindow(lpcs->hwnd);             // Make everything pretty 

    hOldCursor = SetCursor( lpcs->hWaitCursor );

    /* lock our buffer and start xfering data */
    lpstr = GlobalLock(hMem);

    while (lFileSize > 0) {

        if (lFileSize < lBuffSize)
            lBuffSize = lFileSize;
        mmioRead(hmmioSource, (HPSTR)lpstr, lBuffSize);
        if (mmioWrite(hmmioDest, (HPSTR)lpstr, lBuffSize) <= 0) {
            /* we got a write error on the file, error on it */
            errorUpdateError (lpcs, IDS_CAP_WRITEERROR, (LPSTR)lpcs->achSaveAsFile);
        
            /* close the file and delete it */
            mmioClose(hmmioDest, 0);
            mmioOpen(lpcs->achSaveAsFile, NULL, MMIO_DELETE);
            hmmioDest = NULL;
            fReturn = FALSE;
            goto SAVECOPYOUT0;
        }

        // Let the user hit escape to get out
        if (GetAsyncKeyState(VK_ESCAPE) & 0x0001) {
            /* close the file and delete it */
            mmioClose(hmmioDest, 0);
            mmioOpen(lpcs->achSaveAsFile, NULL, MMIO_DELETE);
            hmmioDest = NULL;
            goto SAVECOPYOUT0;
        }

        lFileSize -= lBuffSize;

        // lTemp is percentage complete
        lTemp = muldiv32 (lFileSizeTotal - lFileSize, 100L, lFileSizeTotal);
        statusUpdateStatus (lpcs, IDS_CAP_SAVEASPERCENT, lTemp);

        Yield();
    } // endwhile more bytes to copy
SAVECOPYOUT0:
    SetCursor( hOldCursor );
    

SAVECOPYOUT:
    /* close files, free up mem, restore cursor and get out */
    if (hmmioSource) mmioClose(hmmioSource, 0);
    if (hmmioDest){
        mmioSeek(hmmioDest, 0L, SEEK_END);
        mmioClose(hmmioDest, 0);    
    }
    if (hMem) {
        GlobalUnlock(hMem);
        GlobalFree(hMem);
    }
    statusUpdateStatus (lpcs, NULL);    
    return fReturn;
}


/*--------------------------------------------------------------+
| fileAllocCapFile - allocate the capture file			|
|								|
+--------------------------------------------------------------*/
BOOL FAR PASCAL fileAllocCapFile(LPCAPSTREAM lpcs, DWORD dwNewSize)
{
    BOOL        fOK = FALSE;
    HMMIO       hmmio;
    WORD	w;
    HCURSOR     hOldCursor;

    lpcs->fCapFileExists = FALSE;
    hmmio = mmioOpen(lpcs->achFile, NULL, MMIO_WRITE);
    if( !hmmio ) {
	/* try and create */    
        hmmio = mmioOpen(lpcs-> achFile, NULL,
		MMIO_CREATE | MMIO_WRITE);
	if( !hmmio ) {
	    /* find out if the file was read only or we are just */
	    /* totally hosed up here.				 */
	    hmmio = mmioOpen(lpcs-> achFile, NULL, MMIO_READ);
	    if (hmmio){
		/* file was read only, error on it */
                errorUpdateError (lpcs, IDS_CAP_READONLYFILE, (LPSTR)lpcs-> achFile);
		mmioClose(hmmio, 0);
		return FALSE;
	    } else {
		/* even weirder error has occured here, give CANTOPEN */
                errorUpdateError (lpcs, IDS_CAP_CANTOPEN, (LPSTR) lpcs-> achFile);
		return FALSE;
	    }
	}
    }

    /* find the size */
    lpcs-> lCapFileSize = mmioSeek(hmmio, 0L, SEEK_END);

    if( dwNewSize == 0 )
        dwNewSize = 1;
	    	    
    lpcs-> lCapFileSize = dwNewSize;
    hOldCursor = SetCursor( lpcs-> hWaitCursor );

    // Delete the existing file so we can recreate to the correct size
    mmioClose(hmmio, 0);	// close the file before deleting 
    mmioOpen(lpcs-> achFile, NULL, MMIO_DELETE);

    /* now create a new file with that name */
    hmmio = mmioOpen(lpcs-> achFile, NULL, MMIO_CREATE | MMIO_WRITE);
    if( !hmmio ) {
        return FALSE;
    }
   
    /* Seek to end of new file */
    fOK = (mmioSeek( hmmio, dwNewSize - 1, SEEK_SET ) == (LONG) dwNewSize - 1);

    // write any garbage byte at the end of the file
    fOK &= (mmioWrite( hmmio, (HPSTR) &w, 1L ) == 1); 
    mmioClose( hmmio, 0 );

    SetCursor( hOldCursor );

    if (!fOK)
        errorUpdateError (lpcs, IDS_CAP_NODISKSPACE);

    return fOK;
}
    
