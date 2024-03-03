package net.gudenau.peloader;

import net.gudenau.peloader.impl.Natives;
import org.jetbrains.annotations.Contract;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;
import java.util.Objects;

/**
 * A representation of a PE symbol.
 *
 * @param segment The memory segment of this symbol
 */
public record PeSymbol(@NotNull MemorySegment segment) {
    private static final MemoryLayout LAYOUT = Natives.PE_SYMBOL_LAYOUT;

    private static final VarHandle name = LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("name"));
    private static final VarHandle address = LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("address"));
    private static final VarHandle ordinal = LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("ordinal"));

    public PeSymbol(@NotNull SegmentAllocator allocator) {
        this(allocator.allocate(LAYOUT));
    }

    /**
     * The name of the symbol, may be null if only an ordinal is provided.
     *
     * @return The symbol name or null
     */
    @Nullable
    public String name() {
        var value = (MemorySegment) name.get(segment, 0);
        return MemorySegment.NULL.equals(value) ? null :
            value.reinterpret(Long.MAX_VALUE).getString(0);
    }

    /**
     * The address that this symbol points to.
     *
     * @return The symbol address
     */
    @NotNull
    public MemorySegment address() {
        return (MemorySegment) address.get(segment, 0);
    }

    /**
     * The ordinal of this symbol, may be -1 if not provided.
     *
     * @return The ordinal of this symbol
     */
    public int ordinal() {
        return (int) ordinal.get(segment, 0);
    }

    /**
     * Sets the name of this symbol.
     *
     * @param value The new name
     * @return `this`
     */
    @NotNull
    @Contract("_ -> this")
    public PeSymbol name(@NotNull MemorySegment value) {
        Objects.requireNonNull(value, "value can't be null");
        name.set(segment, 0, value);
        return this;
    }

    /**
     * Sets the name of this symbol.
     *
     * @param value The new name
     * @param arena The arena to allocate from
     * @return `this`
     */
    @NotNull
    @Contract("_, _ -> this")
    public PeSymbol name(@Nullable String value, @NotNull Arena arena) {
        Objects.requireNonNull(arena, "arena can't be null");
        return name(value == null ? MemorySegment.NULL : arena.allocateFrom(value));
    }

    /**
     * Sets the address of this symbol.
     *
     * @param value The new address
     * @return `this`
     */
    @NotNull
    @Contract("_ -> this")
    public PeSymbol address(@NotNull MemorySegment value) {
        Objects.requireNonNull(value, "value can't be null");
        address.set(segment, 0, value);
        return this;
    }

    /**
     * Sets the ordinal of this symbol.
     *
     * @param value The new ordinal
     * @return `this`
     */
    @NotNull
    @Contract("_ -> this")
    public PeSymbol ordinal(int value) {
        ordinal.set(segment, 0, value);
        return this;
    }

    /**
     * Sets all members of this symbol.
     *
     * @param name The new name of the symbol
     * @param address The new address of the symbol
     * @param ordinal The new ordinal of the symbol
     * @return `this`
     */
    @NotNull
    @Contract("_, _, _ -> this")
    public PeSymbol set(@NotNull MemorySegment name, @NotNull MemorySegment address, int ordinal) {
        return name(name)
            .address(address)
            .ordinal(ordinal);
    }

    /**
     * Creates a new symbol by upcalling a stub.
     *
     * @param name The name of the symbol
     * @param ordinal The ordinal of the symbol
     * @param target The target method of the upcall
     * @param descriptor The descriptor of the upcall
     * @param arena The arena to allocate from
     * @return The new symbol instance
     */
    @NotNull
    public static PeSymbol upcall(@Nullable String name, int ordinal, @NotNull MethodHandle target, @NotNull FunctionDescriptor descriptor, @NotNull Arena arena) {
        if(name == null && ordinal == -1) {
            throw new IllegalArgumentException("name or ordinal must be valid");
        }

        Objects.requireNonNull(target, "target can't be null");
        Objects.requireNonNull(descriptor, "descriptor can't be null");
        Objects.requireNonNull(arena, "arena can't be null");

        return new PeSymbol(arena).set(
            name == null ? MemorySegment.NULL : arena.allocateFrom(name),
            PeLoader.upcall(target, descriptor, arena),
            ordinal
        );
    }

    /**
     * Creates a new symbol with the specified name.
     *
     * @param name The symbol name
     * @param arena The arena to allocate from
     * @return The new symbol
     */
    @NotNull
    public static PeSymbol ofName(@NotNull String name, @NotNull Arena arena) {
        return new PeSymbol(arena).set(arena.allocateFrom(name), MemorySegment.NULL, -1);
    }

    /**
     * Creates a new symbol with the specified ordinal.
     *
     * @param ordinal The symbol ordinal
     * @param arena The arena to allocate from
     * @return The new symbol
     */
    @NotNull
    public static PeSymbol ofOrdinal(int ordinal, @NotNull Arena arena) {
        return new PeSymbol(arena).ordinal(ordinal);
    }

    /**
     * Returns the "identifier" of this symbol. If there is only a name the name will be returned, if there is only an
     * ordinal only the stringified ordinal will be returned, if both exist "$name ($ordinal)" will be returned.
     *
     * @return The identifier of the symbol
     */
    @NotNull
    public String identifier() {
        var name = name();
        var ordinal = ordinal();
        if(name == null) {
            return Integer.toString(ordinal);
        } else if(ordinal == -1) {
            return name;
        } else {
            return name + "(" + ordinal + ')';
        }
    }
}
