#ifndef PELOADER_INTERNAL_H
#define PELOADER_INTERNAL_H

#include "pefile.h"

typedef struct {
    const char* name;
    void** address;
    int ordinal;
} PeImportedFunction;

typedef struct {
    const char* name;
    int functionCount;
    PeImportedFunction* functions;
} PeImportModule;

typedef struct {
    const char* name;
    void* address;
    int ordinal;
} PeExportedFunction;

struct PeFile {
    //TODO Make the fileHandle and other stuff temporary, they are not needed once the PE file is loaded.
    int fileHandle;

    struct {
        PeOptionalHeaderStd std;
        PeOptionalHeaderWin win;
    } headers;

    PeDataDir dataDirs[16];

    void* sectionAllocation;
    size_t sectionAllocationSize;

    PeSection* sections;
    PeImportModule* imports;
    PeExportedFunction* exports;

    int sectionCount;
    int importCount;
    int exportCount;
};

#endif //PELOADER_INTERNAL_H
