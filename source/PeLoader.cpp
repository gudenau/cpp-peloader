/*
TODO:
 - Faster symbol lookups (binary search, hash map?)
 - Non-AMD64 support
 - Non-Linux support
 */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
}

#include "internal.h"
#include "io.h"
#include "pefile.h"

#include "peloader.h"

/**
 * Returns the smaller of the two values.
 *
 * @param a Value A
 * @param b Value B
 * @return The smaller of A and B
 */
static inline size_t min(size_t a, size_t b) {
    return a > b ? b : a;
}

/**
 * Returns the larger of the two values.
 *
 * @param a Value A
 * @param b Value B
 * @return The larger of A and B
 */
static inline size_t max(size_t a, size_t b) {
    return a < b ? b : a;
}

/**
 * Cleans up all resources associated with the provided PeFile.
 *
 * @param file The file to cleanup
 */
static void cleanup(PeFile* file) {
    closeFile(&file->file);

    if(file->importCount != 0) {
        for(int i = 0; i < file->importCount; i++) {
            auto module = &file->imports[i];
            delete[] module->functions;
        }

        delete[] file->imports;
    }

    delete file->exports;

    if(file->sectionAllocation != nullptr) {
        munmap(file->sectionAllocation, file->sectionAllocationSize);
    }
    delete[] file->sections;

    delete file;
}

/**
 * A generic way to read some data from a file into memory. It will only read up to *remaining bytes from the file, even
 * if the buffer object is larger. On successful read remaining will be updated to reflect how many bytes where
 * transferred.
 *
 * @tparam T The type of the buffer
 * @param file The file to read from
 * @param buffer The buffer to read to
 * @param remaining A pointer to the amount of bytes to read
 * @return 0 on success, <0 on error
 */
template <typename T> static int readOptionalHeader(PeFile* file, T& buffer, uint16_t* remaining) {
    auto transferred = readPartially(&file->file, &buffer, min(*remaining, sizeof(buffer)));
    if(transferred < 0) return (int) transferred;
    *remaining -= transferred;
    return 0;
}

/**
 * Resolves an RVA from a PE file to the section that contains it.
 *
 * @param file The file that contains the RVA
 * @param rva The RVA to resolve
 * @return The section if found, nullptr if missing
 */
static PeSection* resolveRvaSection(PeFile* file, uint32_t rva) {
    for(int i = 0; i < file->sectionCount; i++) {
        auto current = &file->sections[i];
        if(current->header.virtualAddress <= rva && current->header.virtualAddress + current->size > rva) {
            return current;
        }
    }

    return nullptr;
}

/**
 * Resolves a RVA in a given segment to a virtual address.
 *
 * @tparam T The type of the pointer
 * @param section The section that owns the RVA
 * @param rva The RVA to resolve
 * @return The pointer to virtual memory
 */
template <typename T> static T* resolveRva(PeSection* section, uint32_t rva) {
    return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(section->pointer) + rva - section->header.virtualAddress);
}

/**
 * Resolves an RVA from a PeFile to a virtual address.
 *
 * @tparam T The type of the pointer
 * @param file The file that owns the RVA
 * @param rva The RVA to resolve
 * @return The resolved pointer, or nullptr if not found
 */
template <typename T> static T* resolveRva(PeFile* file, uint32_t rva) {
    if(rva == 0) {
        return nullptr;
    }

    PeSection* section = resolveRvaSection(file, rva);

    if(section == nullptr) {
        return nullptr;
    }

    return resolveRva<T>(section, rva);
}

/**
 * Resolves an RVA from a PE data directory to a virtual address.
 *
 * @tparam T The type of the pointer
 * @param file The file that owns the data directory
 * @param dir The data directory to resolve
 * @return The pointer to the data directory, or nullptr if not found
 */
template <typename T> static T* resolveRva(PeFile* file, PeDataDir& dir) {
    if(dir.size == 0 && dir.virtualAddress == 0) {
        return nullptr;
    }

    return resolveRva<T>(file, dir.virtualAddress);
}

/**
 * Parses the headers from a PE file.
 *
 * @param file The PE file to parse
 * @param peHeader The parsed headers
 * @return 0 on success, <0 on failure
 */
static int parsePeHeaders(PeFile* file, PeHeader& peHeader) {
    // There is potential for the optional header to be truncated. So if it is we only read what we can
    uint16_t remainingHeader = peHeader.sizeOfOptionalHeader - sizeof(uint16_t);

    if(remainingHeader) {
        auto result = readOptionalHeader(file, file->headers.std, &remainingHeader);
        if(result < 0) return result;
    }

    if(remainingHeader) {
        auto result = readOptionalHeader(file, file->headers.win, &remainingHeader);
        if(result < 0) return result;
    }

    for(int i = 0; i < 16 && remainingHeader; i++) {
        auto result = readOptionalHeader(file, file->dataDirs[i], &remainingHeader);
        if(result < 0) return result;
    }

    // The sections are right after the PE header, no pointers or anything
    auto sections = new PeSection[peHeader.numberOfSections];
    for(int i = 0; i < peHeader.numberOfSections; i++) {
        auto result = readFully(&file->file, sections[i].header);
        if(result < 0) {
            delete[] sections;
            return result;
        }
    }
    file->sectionCount = peHeader.numberOfSections;
    file->sections = sections;

    return 0;
}

/**
 * Allocates a block of contiguous memory and reads the sections into it.
 *
 * @param file The PE file to read the sections from
 * @return 0 on success, <0 on ereror
 */
static int readSegments(PeFile* file) {
    // The lowest and highest addresses of the sections without the PE image base applied.
    size_t baselessStart = SIZE_MAX;
    size_t baselessEnd = 0;

    // Find the min and max addresses that the sections will occupy.
    for(int i = 0; i < file->sectionCount; i++) {
        auto section = &file->sections[i];

        if(section->header.virtualSize == 0) {
            continue;
        }
        section->size = section->header.virtualSize;

        baselessStart = min(baselessStart, section->header.virtualAddress);
        baselessEnd = max(baselessEnd, section->header.virtualAddress + section->header.virtualSize);
    }

    // Round up to the nearest page
    baselessEnd = (baselessEnd + 0x1000) & ~0xFFF;

    size_t allocationSize = baselessEnd - baselessStart;
    void* allocation = mmap(
        nullptr,
        allocationSize,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if(allocation == MAP_FAILED) {
        return -errno;
    }

    auto pointer = reinterpret_cast<intptr_t>(allocation);

    for(int i = 0; i < file->sectionCount; i++) {
        auto section = &file->sections[i];
        if(section->size == 0) {
            continue;
        }

        // Since the memory we have is contiguous we can do simple math to drop the sections in
        auto sectionOffset = section->header.virtualAddress - baselessStart;
        auto sectionPointer = reinterpret_cast<void*>(sectionOffset + pointer);
        section->pointer = sectionPointer;

        // There are sections that only exist in memory (like BSS)
        if(section->header.pointerToRawData != 0) {
            auto result = readFully(
                &file->file,
                section->header.pointerToRawData,
                sectionPointer,
                min(section->header.sizeOfRawData, section->size)
            );
            if(result < 0) return result;
        }
    }

    // Need this for later
    file->sectionAllocationSize = allocationSize;

    return 0;
}

/**
 * A basic error handler for an unbound import, we don't mandate that all imports are bound before usage of a symbol.
 */
static PE_FUNC void unboundImport() {
    fprintf(stderr, "An unbound import was called!\n");
    abort();
}

/**
 * Parses the imports from a PE file.
 *
 * @param file The file to parse
 * @return 0 on success, <0 on error
 */
static int parseImports(PeFile* file) {
    auto dataDir = &file->dataDirs[IMPORT_TABLE_DIR];
    auto descriptors = resolveRva<PeImportDescriptor>(file, dataDir->virtualAddress);
    if(descriptors == nullptr) {
        return 0;
    }

    // Find the count since we don't have a reliable way to calculate this.
    int count = 0;
    while(descriptors[count].nameRva != 0) {
        count++;
    }

    auto modules = new PeImportModule[count];

    for(int i = 0; i < count; i++) {
        modules[i].name = resolveRva<char>(file, descriptors[i].nameRva);

        auto importTable = resolveRva<uint64_t>(file, descriptors[i].importTable);
        if(importTable == nullptr) {
            modules[i].functionCount = 0;
            modules[i].functions = nullptr;
            continue;
        }

        int importCount = 0;
        while(importTable[importCount] != 0) {
            importCount++;
        }

        auto functions = new PeImportedFunction[importCount];
        for(int o = 0; o < importCount; o++) {
            auto importEntry = importTable[o];
            auto function = &functions[o];

            // TODO Make this work on 32 bit arches
            // If the top bit is set this is an ordinal
            if(importEntry & 0x8000000000000000L) {
                auto ordinal = (uint16_t) (importEntry & 0x000000000000FFFFL);
                function->name = nullptr;
                function->ordinal = ordinal;
            } else {
                auto peImport = resolveRva<PeImportHintNameTable>(file, (int) importEntry);
                function->name = peImport->name;
                function->ordinal = -1;
            }
            function->address = reinterpret_cast<void**>(&importTable[o]);
            *function->address = reinterpret_cast<void*>(unboundImport);
        }

        modules[i].functionCount = importCount;
        modules[i].functions = functions;
    }

    file->importCount = count;
    file->imports = modules;

    return 0;
}

/**
 * Parses the exports from a PE file.
 *
 * @param file The PE file to parse
 * @return 0 on success, -1 on error
 */
static int parseExports(PeFile* file) {
    auto dataDir = &file->dataDirs[EXPORT_TABLE_DIR];
    auto descriptor = resolveRva<PeExportDescriptor>(file, dataDir->virtualAddress);
    if(descriptor == nullptr) {
        return 0;
    }

    int count = (int) descriptor->addressTableEntries;
    if(count < 0) {
        return -EINVAL;
    }

    //TODO This is technically wrong, but I don't want to fix it right now.
    auto ordinalBase = (int) descriptor->ordinalBase;
    auto addresses = resolveRva<uint32_t>(file, descriptor->exportAddressTableRva);
    auto names = resolveRva<uint32_t>(file, descriptor->namePointerRva);
    auto ordinals = resolveRva<uint16_t>(file, descriptor->ordinalTableRva);

    auto exports = new PeExportedFunction[count];

    for(int i = 0; i < count; i++) {
        int ordinal = ordinalBase + ordinals[i];
        const char* name = nullptr;
        if(i < descriptor->numberOfNamePointers) {
            name = resolveRva<char>(file, names[i]);
        }
        auto address = resolveRva<void>(file, addresses[i]);

        auto current = &exports[i];
        current->name = name;
        current->ordinal = ordinal;
        current->address = address;
    }

    file->exportCount = count;
    file->exports = exports;

    return 0;
}

/**
 * Process the segment relocations in a PE file. This accounts for differences in where the file is loaded and where the
 * compiler assumed it would be loaded. Essentially this just adds the difference in the two addresses to all of the
 * listed addresses that need updating.
 *
 * @param file The PE file to relocate
 * @return 0 on success, -1 on error
 */
static int relocateFile(PeFile* file) {
    auto relocations = resolveRva<void>(file, file->dataDirs[BASE_RELOCATION_TABLE_DIR]);
    if(relocations == nullptr) return 0;

    /*
    The format of this table is kind of odd, it's headers mixed with data.
     */
    auto pointer = reinterpret_cast<intptr_t>(relocations);
    auto end = pointer + file->dataDirs[BASE_RELOCATION_TABLE_DIR].size;

    while(pointer < end) {
        auto addressRva = *reinterpret_cast<uint32_t*>(pointer);
        auto size = *reinterpret_cast<uint32_t*>(pointer + sizeof(addressRva));

        if(addressRva == 0 || size == 0) {
            break;
        }

        auto currentSection = resolveRvaSection(file, addressRva);
        if(currentSection == nullptr) {
            return -EINVAL;
        }

        auto sectionBase = (intptr_t) file->headers.win.imageBase + currentSection->header.virtualAddress;
        auto sectionOffset = reinterpret_cast<intptr_t>(currentSection->pointer) - sectionBase;

        int offset = sizeof(addressRva) + sizeof(size);
        while(offset < size) {
            auto raw = *reinterpret_cast<uint16_t*>(pointer + offset);
            offset += sizeof(raw);

            // Top 4 bits are the type, bottom 12 are the offset of this table
            auto type = raw >> 12;
            auto offset = raw & 0x0FFF;

            //TODO Implement the other relocations
            switch(type) {
                // Everyone needs a NOP, it's for padding
                case 0: break;

                // A 64 bit raw offset, nothing fancy.
                case 10: {
                    *resolveRva<uint64_t>(currentSection, offset + addressRva) += sectionOffset;
                } break;

                default: {
                    fprintf(stderr, "peloader: unknown relocation type: %X (%d)\n", type);
                    fflush(stderr);
                    abort();
                }
            }
        }

        // The size in the header also counts the header.
        pointer += size;
    }

    return 0;
}

#define IMAGE_SCN_MEM_EXECUTE   (0x20000000)
#define IMAGE_SCN_MEM_READ      (0x40000000)
#define IMAGE_SCN_MEM_WRITE     (0x80000000)

/**
 * Applies the permissions of the sections to memory. Takes everything from RW to the correct flags.
 *
 * @param file The file to apply memory permissions to
 * @return 0 on success, <0 on error
 */
static int applySegmentPerms(PeFile* file) {
    for(int i = 0; i < file->sectionCount; i++) {
        auto section = &file->sections[i];

        auto characteristics = section->header.characteristics;
        int perms = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 ? PROT_EXEC : 0;
        perms |= (characteristics & IMAGE_SCN_MEM_READ) != 0 ? PROT_READ : 0;
        perms |= (characteristics & IMAGE_SCN_MEM_WRITE) != 0 ? PROT_WRITE : 0;

        auto result = mprotect(section->pointer, section->size, perms);
        if(result != 0) return -errno;
    }

    return 0;
}

/**
 * Parses and loads a PE file into memory.
 *
 * @param file The file to read
 * @return 0 on success, <0 on error
 */
static int parsePeFile(PeFile* file) {
    {
        DosHeader dosHeader;
        auto result = readFully(&file->file, dosHeader);
        if(result < 0) return result;

        if(dosHeader.magic != DOS_MAGIC) {
            return -EINVAL;
        }

        if(fileSeek(&file->file, dosHeader.peOff) == (off_t) -1) {
            return -errno;
        }
    }

    PeHeader peHeader;
    auto result = readFully(&file->file, peHeader);
    if(result < 0) return result;

    if(peHeader.magic != PE_MAGIC || peHeader.sizeOfOptionalHeader < 2) {
        return -EINVAL;
    }

    uint16_t magic;
    result = readFully(&file->file, magic);
    if(result < 0) return result;

    //TODO This should support other arches (with ifdefs)
    if(magic == PE32_PLUS_MAGIC) {
        result = parsePeHeaders(file, peHeader);
    } else {
        result = EIO;
    }
    if(result < 0) return result;

    result = readSegments(file);
    if(result < 0) return result;

    result = parseImports(file);
    if(result < 0) return result;

    result = parseExports(file);
    if(result < 0) return result;

    result = relocateFile(file);
    if(result < 0) return result;

    result = applySegmentPerms(file);
    if(result < 0) return result;

    closeFile(&file->file);

    return result;
}

int peloader_open(const char* path, PeFile** result) {
    if(!path || !result) {
        return -EINVAL;
    }

    PeLoaderOpen options;
    options.version = PELOADER_OPTIONS_VERSION;
    options.mode = PELOADER_OPEN_FILE;
    options.file.path = path;
    return peloader_openEx(&options, result);
}

int peloader_openEx(const PeLoaderOpen* options, PeFile** result) {
    if(!options || !result) {
        return -EINVAL;
    }

    PeLoaderOpen optionsCopy;
    memcpy(&optionsCopy, options, sizeof(*options));
    if(optionsCopy.version != PELOADER_OPTIONS_VERSION) {
        return -EINVAL;
    }

    auto file = new PeFile;

    switch(optionsCopy.mode) {
        case PELOADER_OPEN_FILE: {
            if(optionsCopy.file.path == nullptr) {
                cleanup(file);
                return EINVAL;
            }

            if(openFile(&file->file, optionsCopy.file.path) != 0) {
                cleanup(file);
                return -errno;
            }
        } break;

        case PELOADER_OPEN_MEMORY: {
            if(optionsCopy.file.buffer == nullptr || optionsCopy.file.length == 0) {
                cleanup(file);
                return EINVAL;
            }

            if(openMemory(&file->file, optionsCopy.file.buffer, optionsCopy.file.length, optionsCopy.file.callback, optionsCopy.file.user) != 0) {
                cleanup(file);
                return -errno;
            }
        } break;

        default: {
            cleanup(file);
            return -EINVAL;
        } break;
    }

    auto res = parsePeFile(file);
    if(res) {
        cleanup(file);
        return res;
    }

    *result = file;

    return 0;
}

void peloader_close(PeFile** file) {
    if(file == nullptr) {
        return;
    }

    cleanup(*file);

    *file = nullptr;
}

/**
 * Gets the name of an object for find.
 *
 * @param pointer The pointer to the object
 * @return The name of the object
 */
typedef const char* (*NameFunc)(void* pointer);

/**
 * A super simple and stupid way to find a named item in an array. TODO replace this with something less bad.
 *
 * @tparam T The type of item to search for
 * @param size The size of the array in elements
 * @param array The array of items to search though
 * @param target The name of the desired object
 * @param nameFunc The name getter for the objects
 * @return A pointer to the found object, nullptr if not found
 */
template <typename T> T* find(size_t size, T* array, const char* target, NameFunc nameFunc) {
    for(size_t i = 0; i < size; i++) {
        auto name = nameFunc(&array[i]);
        if(name == nullptr) {
            continue;
        } else if(strcmp(target, name) == 0) {
            return &array[i];
        }
    }

    return nullptr;
}

/**
 * Finds an import module from a PE file.
 *
 * @param file The file to search
 * @param name The name of the import
 * @return A pointer to the import module, or nullptr if not found
 */
static PeImportModule* findImportModule(PeFile* file, const char* name) {
    return find<PeImportModule>(file->importCount, file->imports, name, [](void* current) -> const char* {
        auto currentModule = static_cast<PeImportModule*>(current);
        return currentModule->name;
    });
}

int peloader_import(PeFile* file, const char* module, const PeSymbol* symbol) {
    if(file == nullptr || module == nullptr || symbol == nullptr) {
        return -EINVAL;
    }

    auto importModule = findImportModule(file, module);
    if(importModule == nullptr) return -EINVAL;

    PeImportedFunction* imported = nullptr;
    // Ordinals should be faster, check those first (if present)
    if(symbol->ordinal != -1) {
        for(int i = 0; i < importModule->functionCount; i++) {
            if(symbol->ordinal == importModule->functions[i].ordinal) {
                imported = &importModule->functions[i];
                break;
            }
        }
    }
    if(imported == nullptr && symbol->name != nullptr) {
        for(int i = 0; i < importModule->functionCount; i++) {
            if(strcmp(symbol->name, importModule->functions[i].name) == 0) {
                imported = &importModule->functions[i];
                break;
            }
        }
    }
    if(imported == nullptr) {
        return -EINVAL;
    }

    *imported->address = symbol->address;

    return 0;
}

int peloader_export(PeFile* file, PeSymbol* symbol) {
    if(file == nullptr || symbol == nullptr) {
        return -EINVAL;
    }

    PeExportedFunction* exportedFunction = nullptr;
    // Ordinals should be faster, check those first (if present)
    if(symbol->ordinal != -1) {
        for(int i = 0; i < file->exportCount; i++) {
            if(symbol->ordinal == file->exports[i].ordinal) {
                exportedFunction = &file->exports[i];
                break;
            }
        }
    }
    if(exportedFunction == nullptr && symbol->name != nullptr) {
        for(int i = 0; i < file->exportCount; i++) {
            if(file->exports[i].name != nullptr && strcmp(symbol->name, file->exports[i].name) == 0) {
                exportedFunction = &file->exports[i];
                break;
            }
        }
    }
    if(exportedFunction == nullptr) {
        return -EINVAL;
    }

    symbol->address = exportedFunction->address;

    return 0;
}

int peloader_modules(PeFile* file, const char** names) {
    if(file == nullptr) {
        return -EINVAL;
    }

    auto count = file->importCount;
    if(names != nullptr) {
        for(int i = 0; i < count; i++) {
            names[i] = file->imports[i].name;
        }
    }
    return count;
}

int peloader_imports(PeFile* file, const char* module, PeSymbol* symbols) {
    if(file == nullptr || module == nullptr) {
        return -EINVAL;
    }

    auto importModule = find<PeImportModule>(file->importCount, file->imports, module, [](void* current) -> const char* {
        auto currentModule = static_cast<PeImportModule*>(current);
        return currentModule->name;
    });
    if(importModule == nullptr) {
        return -EINVAL;
    }

    auto count = importModule->functionCount;
    if(symbols != nullptr) {
        for(int i = 0; i < count; i++) {
            auto inFunc = &importModule->functions[i];
            auto outFunc = &symbols[i];
            outFunc->name = inFunc->name;
            outFunc->ordinal = inFunc->ordinal;
            outFunc->address = nullptr;
        }
    }
    return count;
}

int peloader_exports(PeFile* file, PeSymbol* symbolss) {
    if(file == nullptr) {
        return -EINVAL;
    }

    auto count = file->exportCount;
    if(symbolss != nullptr) {
        for(int i = 0; i < count; i++) {
            auto currentIn = &file->exports[i];
            auto currentOut = &symbolss[i];
            currentOut->name = currentIn->name;
            currentOut->ordinal = currentIn->ordinal;
            currentOut->address = currentIn->address;
        }
    }

    return count;
}
