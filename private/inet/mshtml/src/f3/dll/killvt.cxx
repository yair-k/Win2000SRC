// Forms3
// killvtbl.cxx
//
// If an abstract C++ does not directly or indirectly call a virtual
// function on itself from its constructor or destructor, then the
// vtables for the class are not used. The space used by these
// useless vtables can be very large.
//
// This tool remedies this problem by generating an object file with
// zero size COMDAT records for the vtables listed in an input file.
// If the object file generated by this tool is linked before all other
// files, the linker will happily link in the zero size vtables.
//
// The format for the vtable names is "classname" for single inheritance
// and "classname.baseclassname" for multiple inheritance.
//
// Sample input:
//
// CControl
// CControl.IOleObject
// CControl.IPersistStreamInit
// CControl.IViewObject2
// IOleObject
// IPersistStreamInit
// IViewObject2

#pragma warning(disable:4201)
#pragma warning(disable:4214)
#pragma warning(disable:4244)
#pragma warning(disable:4514)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <time.h>

char * g_apchVTable[1024];
int    g_cClass;
FILE * g_pfileIn;
FILE * g_pfileOut;

#if defined(_M_IX86)
    #define IMAGE_FILE_MACHINE IMAGE_FILE_MACHINE_I386
#elif defined(_M_ALPHA)
    #define IMAGE_FILE_MACHINE IMAGE_FILE_MACHINE_ALPHA
#elif defined(_M_PPC)
    #define IMAGE_FILE_MACHINE IMAGE_FILE_MACHINE_POWERPC
#elif defined(_M_MRX000)
    #define IMAGE_FILE_MACHINE IMAGE_FILE_MACHINE_R4000
#else
#error Machine type unknown
#endif

void
ReadInputFile()
{
    char achClass[128];
    char achVTable[128];
    char *pch;

    while (fscanf(g_pfileIn, " %s", achClass) != EOF)
    {
        pch = strchr(achClass, '.');
        if (pch)
        {
            *pch = NULL;
            sprintf(achVTable, "??_7%s@@6B%s@@@", achClass, pch + 1);
        }
        else
        {
            sprintf(achVTable, "??_7%s@@6B@", achClass);
        }
        g_apchVTable[g_cClass++] = _strdup(achVTable);
    }
}

void
WriteOutputFile()
{
    int                     i;
    IMAGE_FILE_HEADER       file;
    IMAGE_SECTION_HEADER    scn;
    IMAGE_SYMBOL            symSection;
    IMAGE_AUX_SYMBOL        auxSection;
    IMAGE_SYMBOL            symName;

    // file header

    memset(&file, 0, sizeof(file));

    file.Machine = IMAGE_FILE_MACHINE;
    file.NumberOfSections = g_cClass;
    file.TimeDateStamp = time(NULL);
    file.PointerToSymbolTable =
            IMAGE_SIZEOF_FILE_HEADER +
            IMAGE_SIZEOF_SECTION_HEADER * g_cClass;
    file.NumberOfSymbols = 3 * g_cClass;
    file.Characteristics = 0;

    fwrite(&file, IMAGE_SIZEOF_FILE_HEADER, 1, g_pfileOut);

    // section headers

    memset(&scn, 0, sizeof(scn));
    strcpy((char *)scn.Name, ".rdata");
    scn.Characteristics =
            IMAGE_SCN_CNT_INITIALIZED_DATA |
            IMAGE_SCN_LNK_COMDAT |
            IMAGE_SCN_ALIGN_8BYTES |
            IMAGE_SCN_MEM_READ;

    for (i = 0; i < g_cClass; i++)
    {
        fwrite(&scn, IMAGE_SIZEOF_SECTION_HEADER, 1, g_pfileOut);
    }

    // symbol table

    memset(&symSection, 0, sizeof(symSection));
    strcpy((char *)symSection.N.ShortName, ".rdata");
    symSection.Type = IMAGE_SYM_TYPE_NULL;
    symSection.StorageClass = IMAGE_SYM_CLASS_STATIC;
    symSection.NumberOfAuxSymbols = 1;

    memset(&auxSection, 0, sizeof(auxSection));
    auxSection.Section.Selection = IMAGE_COMDAT_SELECT_ANY;

    memset(&symName, 0, sizeof(symName));
    symName.N.Name.Short = 0;
    symName.N.Name.Long = 4;
    symName.Type = IMAGE_SYM_TYPE_NULL;
    symName.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;

    for (i = 0; i < g_cClass; i++)
    {
        symSection.SectionNumber = i + 1;
        auxSection.Section.Number = i + 1;
        symName.SectionNumber = i + 1;

        fwrite(&symSection, IMAGE_SIZEOF_SYMBOL, 1, g_pfileOut);
        fwrite(&auxSection, IMAGE_SIZEOF_SYMBOL, 1, g_pfileOut);
        fwrite(&symName, IMAGE_SIZEOF_SYMBOL, 1, g_pfileOut);

        symName.N.Name.Long += strlen(g_apchVTable[i]) + 1;
    }

    // string table

    fwrite(&symName.N.Name.Long, 4, 1, g_pfileOut);
    for (i = 0; i < g_cClass; i++)
    {
        fwrite(g_apchVTable[i], strlen(g_apchVTable[i]) + 1, 1, g_pfileOut);
    }
}

int __cdecl
main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s input.txt output.obj\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-") == 0)
    {
        g_pfileIn = stdin;
    }
    else
    {
        g_pfileIn = fopen(argv[1], "r");
        if (!g_pfileIn)
        {
            perror(argv[1]);
            return 1;
        }
    }

    g_pfileOut = fopen(argv[2], "wb");
    if (!g_pfileOut)
    {
        perror(argv[2]);
        return 1;
    }

    ReadInputFile();
    WriteOutputFile();

    fclose(g_pfileIn);  // might be closing stdin, but who cares!
    fclose(g_pfileOut);

    return 0;
}
