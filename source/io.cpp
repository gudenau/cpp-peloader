#include <cerrno>
#include <cstdint>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}

#include "io.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))

/**
 * Opens a file from disk.
 *
 * @param file The file handle
 * @param path The path to the file on disk
 * @return 0 on success, <0 on error
 */
int openFile(File* file, const char* path) {
    file->fileType = TYPE_FILE;
    file->fileHandle = open(path, O_NOFOLLOW);
    if(file->fileHandle == -1) {
        return -errno;
    }
    return 0;
}

/**
 * "Opens" a file from memory.
 *
 * @param file The file handle
 * @param pointer The file buffer
 * @param length The length of the buffer
 * @param callback The callback to invoke on close
 * @param user The user data for the callback
 * @return 0 on success, <0 on error
 */
int openMemory(File* file, const void* pointer, size_t length, PeFileFreeCallback callback, void* user) {
    if(pointer == nullptr || length == 0) {
        return -EINVAL;
    }

    file->fileType = TYPE_BUFFER;
    file->memFile.pointer = pointer;
    file->memFile.length = length;
    file->memFile.offset = 0;
    file->memFile.cleanup = callback;
    file->memFile.user = user;
    return 0;
}

/**
 * Closes a File.
 *
 * @param file The file to close
 * @return 0 on success, <0 on error.
 */
int closeFile(File* file) {
    switch(file->fileType) {
        case TYPE_FILE: {
            int result = 0;
            if(close(file->fileHandle) != 0) {
                result = -errno;
            }
            file->fileType = TYPE_CLOSED;
            return result;
        } break;

        case TYPE_BUFFER: {
            if(file->memFile.cleanup != nullptr) {
                file->memFile.cleanup(file->memFile.pointer, file->memFile.user);
            }
            file->fileType = TYPE_CLOSED;
            return 0;
        } break;

        case TYPE_CLOSED: {
            return 0;
        } break;

        default: {
            fprintf(stderr, "close unexpected fileType: %d\n", file->fileType);
            abort();
        } break;
    }
}

/**
 * Seeks to a specific point in an open file.
 *
 * @param file The file to seek
 * @param offset The offset to seek to
 * @return The seeked position or -1 on error
 */
off64_t fileSeek(File* file, size_t offset) {
    switch(file->fileType) {
        case TYPE_FILE: {
            auto result = lseek64(file->fileHandle, offset, SEEK_SET);
            if(result == (off_t) -1) return -errno;
            return result;
        } break;

        case TYPE_BUFFER: {
            if(offset > file->memFile.length) {
                errno = EIO;
                return -1;
            }
            file->memFile.offset = offset;
            return (off64_t) offset;
        } break;

        case TYPE_CLOSED: {
            errno = EIO;
            return -1;
        } break;

        default: {
            fprintf(stderr, "readFully unexpected fileType: %d\n", file->fileType);
            abort();
        } break;
    }
}

/**
 * Reads from a file until the file ends or length bytes are read.
 *
 * @param file The file to read from
 * @param buffer The buffer to read into
 * @param length The max amount of bytes to read
 * @return The amount of bytes read on success or a negative number on error
 */
ssize_t readPartially(File* file, void* buffer, size_t length) {
    auto pointer = reinterpret_cast<intptr_t>(buffer);
    auto start = pointer;
    auto end = static_cast<intptr_t>(pointer) + length;

    while(pointer < end) {
        ssize_t transferred;
        switch(file->fileType) {
            case TYPE_FILE: {
                transferred = read(file->fileHandle, reinterpret_cast<void*>(pointer), end - pointer);
            } break;

            case TYPE_BUFFER: {
                transferred = MIN(end - pointer, bufferRemaining(file));
                memcpy(reinterpret_cast<void*>(pointer), bufferPointer(file), transferred);
            } break;

            case TYPE_CLOSED: {
                return -EIO;
            } break;

            default: {
                fprintf(stderr, "readPartially unexpected fileType: %d\n", file->fileType);
                abort();
            } break;
        }
        if(transferred == 0) {
            break;
        }else if(transferred < 0) {
            return -errno;
        }
        pointer += transferred;
    }

    return pointer - start;
}

/**
 * Reads length bytes from a file and returns an error code if the file is too small.
 *
 * @param file The file to read from
 * @param buffer The buffer to read into
 * @param length The number of bytes to read
 * @return 0 on success or <0 on error
 */
int readFully(File* file, void* buffer, size_t length) {
    auto result = readPartially(file, buffer, length);
    if(result < 0) {
        return (int) result;
    } else if(result != length) {
        return -EIO;
    } else {
        return 0;
    }
}

/**
 * Reads length bytes from a file starting at offset from the beginning of said file. Equivalent to seeking before
 * calling readFully(handle, buffer, length).
 *
 * @param file The file to read from
 * @param offset The offset into the file to read from
 * @param buffer The buffer to read into
 * @param length The number of bytes to read
 * @return 0 on success or <0 on error
 */
int readFully(File* file, size_t offset, void* buffer, size_t length) {
    if(fileSeek(file, offset) != offset) {
        return -errno;
    }

    return readFully(file, buffer, length);
}
