#ifndef PELOADER_IO_H
#define PELOADER_IO_H

#include <cstddef>
#include <cstdlib>
#include <cstdio>

extern "C" {
#include <sys/types.h>
}

#include "peloader.h"

typedef enum {
    TYPE_FILE,
    TYPE_BUFFER,
    TYPE_CLOSED,
} PeFileType;

typedef struct {
    PeFileType fileType;
    union {
        int fileHandle;
        struct {
            const void* pointer;
            size_t length;
            size_t offset;
            void* user;
            PeFileFreeCallback cleanup;
        } memFile;
    };
} File;

static inline size_t bufferRemaining(File* file) {
    if(file->fileType != TYPE_BUFFER) {
        fprintf(stderr, "bufferRemaining fileType not TYPE_BUFFER");
        abort();
    }
    return file->memFile.length - file->memFile.offset;
}

static inline void* bufferPointer(File* file) {
    if(file->fileType != TYPE_BUFFER) {
        fprintf(stderr, "bufferPointer fileType not TYPE_BUFFER");
        abort();
    }
    if(bufferRemaining(file) <= 0) {
        fprintf(stderr, "bufferPointer underflow");
        abort();
    }

    return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(file->memFile.pointer) + file->memFile.offset);
}

int openFile(File* file, const char* path);
int openMemory(File* file, const void* pointer, size_t length, PeFileFreeCallback callback, void* user);
int closeFile(File* file);

off64_t fileSeek(File* file, size_t offset);

ssize_t readPartially(File* file, void* buffer, size_t length);
int readFully(File* file, void* buffer, size_t length);
int readFully(File* file, size_t offset, void* buffer, size_t length);

template <typename T> static inline int readFully(File* file, T& buffer) {
    return readFully(file, &buffer, sizeof(buffer));
}

#endif //PELOADER_IO_H
