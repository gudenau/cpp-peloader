#include <cerrno>
#include <cstdint>

extern "C" {
#include <unistd.h>
}

#include "io.h"

/**
 * Reads from a file until the file ends or length bytes are read.
 *
 * @param handle The file to read from
 * @param buffer The buffer to read into
 * @param length The max amount of bytes to read
 * @return The amount of bytes read on success or a negative number on error
 */
ssize_t readPartially(int handle, void* buffer, size_t length) {
    auto pointer = reinterpret_cast<intptr_t>(buffer);
    auto start = pointer;
    auto end = static_cast<intptr_t>(pointer) + length;

    while(pointer < end) {
        auto transferred = read(handle, reinterpret_cast<void*>(pointer), end - pointer);
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
 * @param handle The file to read from
 * @param buffer The buffer to read into
 * @param length The number of bytes to read
 * @return 0 on success or <0 on error
 */
int readFully(int handle, void* buffer, size_t length) {
    auto result = readPartially(handle, buffer, length);
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
 * @param handle The file to read from
 * @param offset The offset into the file to read from
 * @param buffer The buffer to read into
 * @param length The number of bytes to read
 * @return 0 on success or <0 on error
 */
int readFully(int handle, size_t offset, void* buffer, size_t length) {
    auto result = lseek(handle, offset, SEEK_SET);
    if(result == (off_t) -1) return -errno;

    return readFully(handle, buffer, length);
}
