#ifndef PELOADER_IO_H
#define PELOADER_IO_H

#include <cstddef>

extern "C" {
#include <sys/types.h>
}

ssize_t readPartially(int handle, void* buffer, size_t length);
int readFully(int handle, void* buffer, size_t length);
int readFully(int handle, size_t offset, void* buffer, size_t length);

template <typename T> static inline int readFully(int handle, T& buffer) {
    return readFully(handle, &buffer, sizeof(buffer));
}

#endif //PELOADER_IO_H
