package net.gudenau.peloader;

import net.gudenau.peloader.impl.Natives;
import net.gudenau.peloader.impl.PeFileImpl;
import org.jetbrains.annotations.NotNull;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.invoke.MethodHandle;
import java.util.SequencedCollection;

/**
 * Methods for interacting with an open PE file.
 */
public sealed interface PeFile extends AutoCloseable permits PeFileImpl {
    /**
     * Binds a symbol to an import of the PE file.
     *
     * @param moduleName The name of the module, I.E. KERNEL32.dll
     * @param symbol The symbol to bind to the PE file
     * @throws UnsatisfiedLinkError If the symbol could not be bound
     */
    void bindImport(@NotNull String moduleName, @NotNull PeSymbol symbol);

    /**
     * Gets an exported symbol from the PE file.
     *
     * @param symbol The symbol to get
     */
    void export(@NotNull PeSymbol symbol);

    /**
     * Downcalls an exported symbol from the PE file.
     *
     * @param symbol The symbol to get
     * @param descriptor The descriptor of the function
     * @return The downcalled method
     */
    default MethodHandle export(@NotNull PeSymbol symbol, @NotNull FunctionDescriptor descriptor) {
        export(symbol);
        return Natives.WIN_LINKER.downcallHandle(symbol.address(), descriptor);
    }

    /**
     * Gets a list of imported modules from this PE file.
     *
     * @return The imported modules
     */
    @NotNull SequencedCollection<@NotNull String> modules();

    /**
     * Gets a list of imported symbols from this PE file.
     *
     * @param module The module that the symbols are imported from
     * @param arena The arena to allocate memory from
     * @return The imported symbols
     */
    @NotNull SequencedCollection<@NotNull PeSymbol> imports(@NotNull String module, @NotNull Arena arena);

    /**
     * Gets a list of exported symbols from this PE file.
     *
     * @param arena The arena to allocate memory from
     * @return The exported symbols
     */
    @NotNull SequencedCollection<@NotNull PeSymbol> exports(@NotNull Arena arena);

    @Override void close();
}
