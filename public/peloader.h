#ifndef PELOADER_PELOADER_H
#define PELOADER_PELOADER_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * All functions that the PE file imports and exports need to be tagged with this. The ABI that Microsoft uses is
 * different than the one Linux uses, this tells the compiler to use the correct one.
 */
#define PE_FUNC __attribute__((ms_abi))

/**
 * An opaque structure for the PE file. Required for all PE operations.
 */
typedef struct PeFile PeFile;

/**
 * A representation of an address that a PE file needs to import or has exported.
 *
 * @param name The name of the address or NULL if absent
 * @param address The pointer to the address
 * @param ordinal The ordinal to the address or -1 if absent
 */
typedef struct {
    const char* name;
    void* address;
    int ordinal;
} PeSymbol;

/**
 * Opens a PE file at the given path. This reads the sections into memory, finds any imports and exports it has and
 * processes the relocations to make it work correctly.
 *
 * @param path The path of the PE file to open
 * @param result The opened PE file
 * @return 0 on success, <0 on error
 */
int peloader_open(const char* path, PeFile** result);

/**
 * The current version of the options structure.
 */
#define PELOADER_OPTIONS_VERSION (1)

/**
 * The different ways to open a PE file.
 */
typedef enum {
    /**
     * Open a PE file on disk, must provide a non-null path.
     */
    PELOADER_OPEN_FILE = 0,

    /**
     * Open a PE file from memory, must provide a buffer and a length.
     */
    PELOADER_OPEN_MEMORY = 1,
} PeLoaderOpenMode;

/**
 * A callback that is invoked when a PE file loaded from memory is closed, used to free the provided memory buffer.
 */
typedef void (*PeFileFreeCallback)(const void* buffer, void* user);

/**
 * The options for peloader_openEx
 */
typedef struct {
    /**
     * The version of the structure.
     */
    int version;

    /**
     * The method to open the PE file.
     */
    PeLoaderOpenMode mode;

    /**
     * The options that change based on the open mode.
     */
    union {
        /**
         * Path mode only: the path to the file to open.
         */
        const char* path;
        /**
         * Memory mode only
         */
        struct {
            /**
             * The pointer to the PE file in memory.
             */
            const void* buffer;

            /**
             * The length of the PE buffer.
             */
            size_t length;

            /**
             * The optional callback to free the buffer when it is no longer needed.
             */
            PeFileFreeCallback callback;

            /**
             * The user data to pass to the callback.
             */
            void* user;
        };
    } file;
} PeLoaderOpen;

/**
 * Opens a PE file with the given options. The documentation for the structure explains the options.
 *
 * @param options The options of the PE file to open
 * @param result The opened PE file
 * @return 0 on success, <0 on error
 */
int peloader_openEx(const PeLoaderOpen* options, PeFile** result);

/**
 * Closes an opened PE file and sets the pointer to NULL.
 */
void peloader_close(PeFile** file);

/**
 * Binds an imported symbol to the given PE file.
 *
 * @param file The file to bind a address to
 * @param module The name of the module the address is imported from
 * @param symbol The address to import
 * @return 0 on success, <0 on error
 */
int peloader_import(PeFile* file, const char* module, const PeSymbol* symbol);

/**
 * Gets an exported symbol from the PE file. The name or ordinal are read from the passed symbol.
 *
 * @param file The file that exported the symbol
 * @param symbol The symbol to get
 * @return 0 on success, <0 on error
 */
int peloader_export(PeFile* file, PeSymbol* symbol);

/**
 * Gets a list of modules that the PE file imported. If names is NULL this only gets the count of modules imported.
 *
 * @param file The PE file to query
 * @param names An array of strings or NULL
 * @return the count of imported modules, <0 on error
 */
int peloader_modules(PeFile* file, const char** names);

/**
 * Gets a list of imported symbols that are imported from a module. If names is NULL this only gets the count of
 * symbols.
 *
 * @param file The PE file to query
 * @param module The name of the module to query
 * @param symbols An array of symbols or NULL
 * @return the count of imported symbols, <0 on error
 */
int peloader_imports(PeFile* file, const char* module, PeSymbol* symbols);

/**
 * Gets a list of exported symbols from a PE file. If names is NULL this only gets the count of symbols.
 *
 * @param file The PE file to query
 * @param symbols An array of symbols or NULL
 * @return the count of exported symbols, <0 on error
 */
int peloader_exports(PeFile* file, PeSymbol* symbols);

#ifdef __cplusplus
}
#endif

#endif //PELOADER_PELOADER_H
