#ifndef PELOADER_TEST_DLL_H
#define PELOADER_TEST_DLL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

const char* testFunc();
void* testCallback(void* (*callback)());
size_t importTest(const char* string);

#ifdef __cplusplus
}
#endif

#endif //PELOADER_TEST_DLL_H
