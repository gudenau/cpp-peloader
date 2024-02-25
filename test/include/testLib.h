#ifndef PELOADER_TESTLIB_H
#define PELOADER_TESTLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <peloader.h>

PE_FUNC const char* testFunc();

PE_FUNC void* testCallback(void* (*callback)());

#ifdef __cplusplus
}
#endif

#endif //PELOADER_TESTLIB_H
