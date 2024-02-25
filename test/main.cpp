#include <cerrno>
#include <cstdio>
#include <cstring>

#include <peloader.h>

PE_FUNC size_t winStrlen(const char* string) {
    return strlen(string);
}

int main(int argc, char** argv) {
    if(argc != 2) {
        return EINVAL;
    }

    PeFile* file;
    auto result = peloader_open(argv[1], &file);
    if(result < 0) return result;

    auto moduleCount = peloader_modules(file, nullptr);
    if(moduleCount < 0) {
        return moduleCount;
    } else if(moduleCount != 0) {
        auto moduleNames = new const char*[moduleCount];
        peloader_modules(file, moduleNames);

        printf("Imported module%s:\n", moduleCount == 1 ? "" : "s");
        for(int i = 0; i < moduleCount; i++) {
            printf("  %s\n", moduleNames[i]);

            int importCount = peloader_imports(file, moduleNames[i], nullptr);
            if(importCount < 0) {
                return importCount;
            } else if(importCount != 0) {
                auto imports = new PeSymbol[importCount];
                peloader_imports(file, moduleNames[i], imports);

                for(int o = 0; o < importCount; o++) {
                    auto imported = &imports[o];
                    if(imported->name != nullptr) {
                        printf("    name: %s\n", imported->name);
                    } else {
                        printf("    ordinal: %d\n", imported->ordinal);
                    }
                }

                delete[] imports;
            }
        }

        delete[] moduleNames;
    }

    auto exportCount = peloader_exports(file, nullptr);
    if(exportCount < 0) {
        return exportCount;
    } else if(exportCount != 0) {
        auto exports = new PeSymbol[exportCount];
        peloader_exports(file, exports);

        printf("Exports:\n");
        for(int i = 0; i < exportCount; i++) {
            auto current = &exports[i];
            printf("  %d:\n", i);
            printf("    ordinal: %d\n", current->ordinal);
            if(current->name != nullptr) {
                printf("    name: %s\n", current->name);
            }
            printf("    pointer: %p\n", current->address);
        }

        delete[] exports;
    }

    PeSymbol function = {
        .name = "strlen",
        .address = reinterpret_cast<void*>(winStrlen),
        .ordinal = -1
    };
    peloader_import(file, "msvcrt.dll", &function);

    function.name = "testFunc";
    peloader_export(file, &function);
    auto testFunc = reinterpret_cast<const char* (PE_FUNC *)()>(function.address);

    function.name = "testCallback";
    peloader_export(file, &function);
    auto testCallback = reinterpret_cast<void* (PE_FUNC *)(void* (PE_FUNC *)())>(function.address);

    function.name = "importTest";
    peloader_export(file, &function);
    auto importTest = reinterpret_cast<size_t (PE_FUNC *)(const char*)>(function.address);

    printf("testFunc: %s\n", testFunc());
    printf("testCallback: %s\n", (const char*) testCallback([]() PE_FUNC -> void* {
        return (void*) "This is from a callback";
    }));
    printf("importTest: %ld\n", importTest("string!"));

    peloader_close(&file);

    return 0;
}
