package net.gudenau.peloader.impl;

import net.gudenau.peloader.PeLoader;
import net.gudenau.peloader.PeSymbol;
import org.jetbrains.annotations.NotNull;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.nio.file.Path;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

public class Test {
    // From https://graphics.stanford.edu/~seander/bithacks.html#HasLessInWord
    private static boolean hasNull(long value) {
        return ((value - 0x0101010101010101L) & ~value & 0x8080808080808080L) != 0;
    }

    private static long strlen(@NotNull MemorySegment string) {
        string = string.reinterpret(Long.MAX_VALUE);
        long length = 0;
        while(!hasNull(string.get(JAVA_LONG, length))) {
            length += 8;
        }
        while(string.get(ValueLayout.JAVA_BYTE, length) != 0) {
            length++;
        }
        return length;
    }

    public static void main(String[] args) throws Throwable {
        var lookup = MethodHandles.lookup().in(Test.class);

        try(
            var arena = Arena.ofConfined();
            var file = PeLoader.open(Path.of(args[0]))
        ) {
            System.out.println("Imports:");
            file.modules().forEach((name) -> {
                System.out.println("  " + name + ':');
                try(var localArena = Arena.ofConfined()) {
                    file.imports(name, localArena).forEach((symbol) -> {
                        System.out.print("    ");
                        if(symbol.name() != null) {
                            System.out.println(symbol.name());
                        } else {
                            System.out.println(symbol.ordinal());
                        }
                    });
                }
            });

            System.out.println("Exports: ");
            file.exports(arena).forEach((export) -> System.out.println("  " + export.identifier()));

            file.bindImport("msvcrt.dll", PeSymbol.upcall(
                "strlen",
                -1,
                lookup.findStatic(Test.class, "strlen", MethodType.methodType(long.class, MemorySegment.class)),
                FunctionDescriptor.of(JAVA_LONG, ADDRESS),
                arena
            ));

            var testFunc = file.export(
                PeSymbol.ofName("testFunc", arena),
                FunctionDescriptor.of(ADDRESS)
            );
            System.out.println("testFunc: " + ((MemorySegment) testFunc.invokeExact()).reinterpret(Long.MAX_VALUE).getString(0));

            var testCallback = file.export(
                PeSymbol.ofName("testCallback", arena),
                FunctionDescriptor.of(ADDRESS, ADDRESS)
            );
            @FunctionalInterface
            interface Callback {
                @NotNull
                MemorySegment invoke();
            }
            var callback = PeLoader.upcall(
                MethodHandles.lookup().bind(
                    ((Callback)() -> arena.allocateFrom("This is from a callback")),
                    "invoke",
                    MethodType.methodType(MemorySegment.class)
                ),
                FunctionDescriptor.of(ADDRESS),
                arena
            );
            System.out.println(
                "testCallback: " +
                ((MemorySegment) testCallback.invokeExact(callback)).reinterpret(Long.MAX_VALUE).getString(0)
            );

            var importTest = file.export(
                PeSymbol.ofName("importTest", arena),
                FunctionDescriptor.of(JAVA_LONG, ADDRESS)
            );
            var length = (long) importTest.invokeExact(arena.allocateFrom("This is a test from Java"));
            System.out.println("\"This is a test from Java\" length: " + length);
        }
    }
}
