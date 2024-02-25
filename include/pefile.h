#ifndef PELOADER_PEFILE_H
#define PELOADER_PEFILE_H

#include <cstdint>

// From https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

#define DOS_MAGIC   (0x5A4D)

typedef struct {
    uint16_t magic;
    uint16_t dontCare[29];
    uint32_t peOff;
} DosHeader;

static_assert(sizeof(DosHeader) == 64, "DosHeader is the wrong size");

#define PE_MAGIC    (0x00004550)

typedef struct {
    uint32_t magic;
    uint16_t machine;
    uint16_t numberOfSections;
    uint32_t timeDateStamp;
    uint32_t pointerToSymbolTable;
    uint32_t numberOfSymbols;
    uint16_t sizeOfOptionalHeader;
    uint16_t characteristics;
} PeHeader;

static_assert(sizeof(PeHeader) == 24, "PeHeader is the wrong size");

#define PE32_PLUS_MAGIC (0x020B)

typedef struct {
    uint8_t majorLinkerVersion;
    uint8_t minorLinkerVersion;
    uint32_t sizeOfCode;
    uint32_t sizeOfInitializedData;
    uint32_t sizeOfUninitializedData;
    uint32_t addressOfEntryPoint;
    uint32_t baseOfCode;
} __attribute__((packed)) PeOptionalHeaderStd;

static_assert(sizeof(PeOptionalHeaderStd) == 22, "PeOptionalHeaderStd is the wrong size");

typedef struct {
    uint64_t imageBase;
    uint32_t sectionAlignment;
    uint32_t fileAlignment;
    uint16_t majorOperatingSystemVersion;
    uint16_t minorOperatingSystemVersion;
    uint16_t majorImageVersion;
    uint16_t minorImageVersion;
    uint16_t minorSubsystemVersion;
    uint16_t majorSubsystemVersion;
    uint32_t win32VersionValue;
    uint32_t sizeOfImage;
    uint32_t sizeOfHeaders;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dllCharacteristics;
    uint64_t sizeOfStackReserve;
    uint64_t sizeOfStackCommit;
    uint64_t sizeOfHeapReserve;
    uint64_t sizeOfHeapCommit;
    uint32_t loaderFlags;
    uint32_t numberOfRvaAndSizes;
} PeOptionalHeaderWin;

static_assert(sizeof(PeOptionalHeaderWin) == 88, "PeOptionalHeaderWin is the wrong size");

#define EXPORT_TABLE_DIR            (0)
#define IMPORT_TABLE_DIR            (1)
#define RESOURCE_TABLE_DIR          (2)
#define EXCEPTION_TABLE_DIR         (3)
#define CERTIFICATE_TABLE_DIR       (4)
#define BASE_RELOCATION_TABLE_DIR   (5)
#define DEBUG_DIR                   (6)
#define ARCHITECTURE_DIR            (7)
#define GLOBAL_PTR_DIR              (8)
#define TLS_TABLE_DIR               (9)
#define LOAD_CONFIG_TABLE_DIR       (10)
#define BOUND_IMPORT_DIR            (11)
#define IAT_DIR                     (12)
#define DELAY_IMPORT_DESCRIPTOR_DIR (13)
#define CLR_RUNTIME_HEADER_DIR      (14)
#define RESERVED_DIR                (15)

typedef struct {
    uint32_t virtualAddress;
    uint32_t size;
} PeDataDir;

static_assert(sizeof(PeDataDir) == 8, "PeDataDir is the wrong size");

typedef struct {
    char name[8];
    uint32_t virtualSize;
    uint32_t virtualAddress;
    uint32_t sizeOfRawData;
    uint32_t pointerToRawData;
    uint32_t pointerToRelocations;
    uint32_t pointerToLineNumbers;
    uint16_t numberOfRelocations;
    uint16_t numberOfLineNumbers;
    uint32_t characteristics;
} PeSectionHeader;

static_assert(sizeof(PeSectionHeader) == 40, "PeSectionHeader is the wrong size");

typedef struct {
    PeSectionHeader header;
    void* pointer;
    size_t size;
} PeSection;

typedef struct {
    uint32_t lookupTable;
    uint32_t timeDateStamp;
    uint32_t forwarderChain;
    uint32_t nameRva;
    uint32_t importTable;
} PeImportDescriptor;

typedef struct {
    uint16_t index;
    char name[];
} PeImportHintNameTable;

static_assert(sizeof(PeImportDescriptor) == 20, "PeImportDescriptor is the wrong size");

typedef struct {
    uint32_t exportFlags;
    uint32_t timeDateStamp;
    uint16_t majorVersion;
    uint16_t minorVersion;
    uint32_t nameRva;
    uint32_t ordinalBase;
    uint32_t addressTableEntries;
    uint32_t numberOfNamePointers;
    uint32_t exportAddressTableRva;
    uint32_t namePointerRva;
    uint32_t ordinalTableRva;
} PeExportDescriptor;

static_assert(sizeof(PeExportDescriptor) == 40, "PeExportDescriptor is the wrong size");

#endif //PELOADER_PEFILE_H
