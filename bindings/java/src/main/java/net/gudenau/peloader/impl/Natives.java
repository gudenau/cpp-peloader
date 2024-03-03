package net.gudenau.peloader.impl;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.IOException;
import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;

import static java.lang.foreign.MemoryLayout.paddingLayout;
import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_INT;

public final class Natives {
    public static final Linker WIN_LINKER = Hacks.getLinker();
    public static final MemoryLayout PE_SYMBOL_LAYOUT = MemoryLayout.structLayout(
        ADDRESS.withName("name"),
        ADDRESS.withName("address"),
        JAVA_INT.withName("ordinal"),
        paddingLayout(4)
    );

    private Natives() {
        throw new AssertionError();
    }

    private static final MethodHandle peloader_open;
    public static int peloader_open(@NotNull String file, @NotNull MemorySegment result) {
        try(var arena = Arena.ofConfined()) {
            return (int) peloader_open.invokeExact(
                arena.allocateFrom(file),
                result
            );
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_open", e);
        }
    }

    private static final MethodHandle peloader_close;
    public static void peloader_close(@NotNull MemorySegment file) {
        try(var arena = Arena.ofConfined()) {
            var pointer = arena.allocate(ADDRESS);
            pointer.set(ADDRESS, 0, file);
            peloader_close.invokeExact(pointer);
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_close", e);
        }
    }

    private static final MethodHandle peloader_import;
    public static int peloader_import(@NotNull MemorySegment file, @NotNull String module, @NotNull MemorySegment symbol) {
        try(var arena = Arena.ofConfined()) {
            return (int) peloader_import.invokeExact(file, arena.allocateFrom(module), symbol);
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_import", e);
        }
    }

    private static final MethodHandle peloader_export;
    public static int peloader_export(@NotNull MemorySegment file, @NotNull MemorySegment symbol) {
        try {
            return (int) peloader_export.invokeExact(file, symbol);
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_export", e);
        }
    }

    private static final MethodHandle peloader_modules;
    public static int peloader_modules(@NotNull MemorySegment file, @NotNull MemorySegment names) {
        try {
            return (int) peloader_modules.invokeExact(file, names);
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_modules", e);
        }
    }

    private static final MethodHandle peloader_imports;
    public static int peloader_imports(@NotNull MemorySegment file, @NotNull String module, @NotNull MemorySegment symbols) {
        try(var arena = Arena.ofConfined()) {
            return (int) peloader_imports.invokeExact(file, arena.allocateFrom(module), symbols);
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_imports", e);
        }
    }

    private static final MethodHandle peloader_exports;
    public static int peloader_exports(@NotNull MemorySegment file, @NotNull MemorySegment symbols) {
        try {
            return (int) peloader_exports.invokeExact(file, symbols);
        } catch(Throwable e) {
            throw new RuntimeException("Failed to invoke peloader_exports", e);
        }
    }

    static {
        var os = switch(System.getProperty("os.name").toLowerCase()) {
            case "linux" -> "linux";
            default -> throw new RuntimeException("Unsupported OS: " + System.getProperty("os.name"));
        };
        var arch = switch(System.getProperty("os.arch").toLowerCase()) {
            case "amd64", "x64" -> "amd64";
            default -> throw new RuntimeException("Unsupported arch: " + System.getProperty("os.arch"));
        };

        var input = Natives.class.getResourceAsStream("/natives/" + os + '/' + arch + '/' + System.mapLibraryName("PeLoader"));
        if(input == null) {
            throw new RuntimeException("Unsupported OS/arch combo: " + System.getProperty("os.name") + "/" + System.getProperty("os.arch"));
        }

        var arena = Arena.ofAuto();
        var linker = Linker.nativeLinker();

        SymbolLookup lookup;
        try(input) {
            var outputPath = Files.createTempFile("PeLoader", ".so");
            try(var output = Files.newOutputStream(outputPath)) {
                input.transferTo(output);
            }
            lookup = SymbolLookup.libraryLookup(outputPath, arena);
            Files.deleteIfExists(outputPath);
        } catch(IOException e) {
            throw new RuntimeException("Failed to extract natives", e);
        }

        record Binder(@NotNull SymbolLookup lookup, @NotNull Linker linker) {
            @NotNull
            MethodHandle bind(@NotNull String name, @Nullable MemoryLayout result, @NotNull MemoryLayout @NotNull ... args) {
                var symbol = lookup.find("peloader_" + name)
                    .orElseThrow(() -> new UnsatisfiedLinkError(name));

                var descriptor = result == null ? FunctionDescriptor.ofVoid(args) :
                    FunctionDescriptor.of(result, args);

                return linker.downcallHandle(symbol, descriptor);
            }
        }

        Binder binder = new Binder(lookup, linker);
        peloader_open = binder.bind("open", JAVA_INT, ADDRESS, ADDRESS);
        peloader_close = binder.bind("close", null, ADDRESS);
        peloader_import = binder.bind("import", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
        peloader_export = binder.bind("export", JAVA_INT, ADDRESS, ADDRESS);
        peloader_modules = binder.bind("modules", JAVA_INT, ADDRESS, ADDRESS);
        peloader_imports = binder.bind("imports", JAVA_INT, ADDRESS, ADDRESS, ADDRESS);
        peloader_exports = binder.bind("exports", JAVA_INT, ADDRESS, ADDRESS);
    }
}
