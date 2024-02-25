#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>

#include "test.h"

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    return TRUE;
}

const char* testFunc() {
    return "This string is inside of the DLL.";
}

void* testCallback(void* (*callback)()) {
    return callback();
}

size_t importTest(const char* string) {
    return strlen(string);
}
