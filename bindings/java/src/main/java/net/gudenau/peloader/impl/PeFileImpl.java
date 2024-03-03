package net.gudenau.peloader.impl;

import net.gudenau.peloader.PeFile;
import net.gudenau.peloader.PeSymbol;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.invoke.MethodHandle;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.SequencedCollection;
import java.util.function.Function;

import static java.lang.foreign.ValueLayout.ADDRESS;

public record PeFileImpl(
    @NotNull MemorySegment pointer
) implements PeFile {
    @Override
    public void bindImport(@NotNull String moduleName, @NotNull PeSymbol symbol) {
        var result = Natives.peloader_import(pointer, moduleName, symbol.segment());
        if(result < 0) {
            throw new UnsatisfiedLinkError("Failed to bind import " + symbol.identifier());
        }
    }

    @Override
    public void export(@NotNull PeSymbol symbol) {
        var result = Natives.peloader_export(pointer, symbol.segment());
        if(result < 0 || symbol.address().equals(MemorySegment.NULL)) {
            throw new UnsatisfiedLinkError("Failed to bind export " + symbol.identifier());
        }
    }

    @Nullable
    private static <T> SequencedCollection<@NotNull T> getList(
        @NotNull Function<MemorySegment, Integer> getter,
        @NotNull MemoryLayout layout,
        @NotNull Function<MemorySegment, T> factory,
        @NotNull Arena arena
    ) {
        var result = getter.apply(MemorySegment.NULL);
        if(result < 0) {
            return null;
        } else if(result == 0) {
            return List.of();
        }
        var count = result;

        var pointer = arena.allocate(layout, count);
        result = getter.apply(pointer);
        if(result < 0) {
            return null;
        }

        var values = new ArrayList<T>(count);
        var layoutSize = layout.byteSize();
        for(int i = 0; i < count; i++) {
            values.add(factory.apply(pointer.asSlice(layoutSize * i, layoutSize)));
        }
        return Collections.unmodifiableSequencedCollection(values);
    }

    @Override
    @NotNull
    public SequencedCollection<@NotNull String> modules() {
        var result = Natives.peloader_modules(pointer, MemorySegment.NULL);
        if(result < 0) {
            throw new RuntimeException("Failed to get modules");
        } else if(result == 0) {
            return List.of();
        }
        var count = result;

        try(var arena = Arena.ofConfined()) {
            var namesPointer = arena.allocate(ADDRESS, count);
            result = Natives.peloader_modules(pointer, namesPointer);
            if(result < 0) {
                throw new RuntimeException("Failed to get modules");
            }

            var names = new ArrayList<String>(count);
            for(int i = 0; i < count; i++) {
                names.add(namesPointer.getAtIndex(ADDRESS, i).reinterpret(Long.MAX_VALUE).getString(0));
            }
            return Collections.unmodifiableSequencedCollection(names);
        }
    }

    @Override
    @NotNull
    public SequencedCollection<@NotNull PeSymbol> imports(@NotNull String module, @NotNull Arena arena) {
        var result = getList(
            (segment) -> Natives.peloader_imports(pointer, module, segment),
            Natives.PE_SYMBOL_LAYOUT,
            PeSymbol::new,
            arena
        );
        if(result == null) {
            throw new RuntimeException("Failed to get imports for " + module);
        }
        return result;
    }

    @Override
    @NotNull
    public SequencedCollection<@NotNull PeSymbol> exports(@NotNull Arena arena) {
        var result = getList(
            (segment) -> Natives.peloader_exports(pointer, segment),
            Natives.PE_SYMBOL_LAYOUT,
            PeSymbol::new,
            arena
        );
        if(result == null) {
            throw new RuntimeException("Failed to get exports");
        }
        return result;
    }

    @Override
    public void close() {
        Natives.peloader_close(pointer);
    }
}
