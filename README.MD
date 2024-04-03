# PeLoader
A minimalistic Windows executable loader that works on any AMD64 target. Created with the intention to allow programmers
to use libraries that where compiled for Windows AMD64 on Linux AMD64.

---
### Example:
An example target DLL header might look like:
```C
#include <string.h>

const char* getString();
void setupCallback(void (*callback)(void*), void* user);
size_t stringLength(const char* string);
```
where `stringLength` imports `strlen`.

You could then use this library as follows:
```C++
#include <cstring>

#include <peloader.h>

// It is critical to mark this with PE_FUNC, different platforms have different calling conventions!
static PE_FUNC size_t winStrlen(const char* string) {
    return strlen(string);
}

int main(int argc, char** argv) {
    // Open the DLL
    PeFile* file;
    auto result = peloader_open("SomeAwesomeLib.dll", &file);
    if(result < 0) return result;
    
    // Bind the strlen import to msvcrt.dll, missing imports have a default handler that aborts.
    PeSymbol function = {
        .name = "strlen",
        .address = reinterpret_cast<void*>(winStrlen),
        .ordinal = -1
    };
    peloader_import(file, "msvcrt.dll", &function);
    
    // Get the exported functions, it is critical that you use PE_FUNC here because calling conventions vary between
    // platforms!
    function.name = "getString";
    peloader_export(file, &function);
    auto getString = reinterpret_cast<const char* (PE_FUNC *)()>(function.address);
    
    function.name = "setupCallback";
    peloader_export(file, &function);
    auto setupCallback = reinterpret_cast<const void (PE_FUNC *)(void(*)(void*), void*)>(function.address);
    
    function.name = "stringLength";
    peloader_export(file, &function);
    auto stringLength = reinterpret_cast<const size_t (PE_FUNC *)(const char*)>(function.address);
    
    // Now to call them like normal function pointers:
    printf("getString: %s\n", getString());
    setupCallback([](void* user) -> void {
        printf("My callback!\n");
    }, nullptr);
    auto someString = "Some string";
    printf(
        "Length of \"%s\": %ld, lib length: %ld\n",
        someString,
        strlen(someString),
        stringLength(someString)
    );
    
    // After this call `file` will be `nullptr` and all the exported symbols will not be valid.
    peloader_close(&file);
    
    return 0;
}
```

---

### Bindings:
- Java 22+: Uses the new Foreign memory and function APIs
