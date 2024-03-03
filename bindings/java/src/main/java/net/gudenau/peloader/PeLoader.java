package net.gudenau.peloader;

import net.gudenau.peloader.impl.Natives;
import net.gudenau.peloader.impl.PeLoaderImpl;
import org.jetbrains.annotations.NotNull;

import java.io.IOException;
import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;

/**
 * The starting point of this library.
 */
public final class PeLoader {
    private PeLoader() {
        throw new AssertionError();
    }

    /**
     * Opens a PE file. The PE file must be on the default {@link java.nio.file.FileSystem FileSystem}.
     *
     * @param path The path to the PE file to open
     * @return The opened PE file
     * @throws IOException If there was a problem opening the file
     */
    @NotNull
    public static PeFile open(@NotNull Path path) throws IOException {
        return PeLoaderImpl.open(path);
    }

    /**
     * Upcalls a method using the WINAPI calling convention. You must use this in order to use callbacks instead of
     * {@link Linker Linker's} upcall because the calling conventions are different.
     *
     * @param target The upcall target
     * @param function The callback descriptor
     * @param arena The arena to allocate memory from
     * @param options The link options
     * @return The WINAPI stub pointer
     */
    @NotNull
    public static MemorySegment upcall(
        @NotNull MethodHandle target,
        @NotNull FunctionDescriptor function,
        @NotNull Arena arena,
        @NotNull Linker.Option @NotNull ... options
    ) {
        return Natives.WIN_LINKER.upcallStub(target, function, arena, options);
    }
}
