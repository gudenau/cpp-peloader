package net.gudenau.peloader.impl;

import net.gudenau.peloader.PeFile;
import org.jetbrains.annotations.NotNull;

import java.io.IOException;
import java.lang.foreign.Arena;
import java.nio.file.FileSystems;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.util.Objects;

import static java.lang.foreign.ValueLayout.ADDRESS;

public final class PeLoaderImpl {
    private PeLoaderImpl() {
        throw new AssertionError();
    }

    @NotNull
    public static PeFile open(@NotNull Path path) throws IOException {
        Objects.requireNonNull(path, "path was null");

        if(path.getFileSystem() != FileSystems.getDefault()) {
            throw new IllegalArgumentException("path must be of the default filesystem");
        }
        var realPath = path.toRealPath(LinkOption.NOFOLLOW_LINKS);

        try(var arena = Arena.ofConfined()) {
            var pointer = arena.allocate(ADDRESS);
            var result = Natives.peloader_open(realPath.toString(), pointer);
            if(result < 0) {
                throw new IOException("Failed to open PE file (" + result + "): " + path);
            }

            return new PeFileImpl(pointer.get(ADDRESS, 0));
        }
    }
}
