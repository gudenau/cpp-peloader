package net.gudenau.peloader.impl;

import org.jetbrains.annotations.NotNull;
import sun.misc.Unsafe;

import java.lang.foreign.Linker;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.AccessibleObject;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.function.Consumer;

final class Hacks {
    private Hacks() {
        throw new AssertionError();
    }

    @NotNull
    private static Unsafe getUnsafe() {
        var exceptions = new ArrayList<Throwable>();

        for(var field : Unsafe.class.getDeclaredFields()) {
            if(field.getType() != Unsafe.class || !Modifier.isStatic(field.getModifiers())) {
                continue;
            }

            try {
                field.setAccessible(true);
                if(field.get(null) instanceof Unsafe unsafe) {
                    return unsafe;
                }
            } catch(Throwable e) {
                exceptions.add(e);
            }
        }

        var exception = new RuntimeException("Failed to get Unsafe");
        exceptions.forEach(exception::addSuppressed);
        throw exception;
    }

    @NotNull
    private static Consumer<AccessibleObject> getAccessible(@NotNull Unsafe unsafe) {
        AccessibleObject object;
        try {
            object = (AccessibleObject) unsafe.allocateInstance(AccessibleObject.class);
        } catch(InstantiationException e) {
            throw new RuntimeException("Failed to find override offset", e);
        }

        long cookie = -1;
        for(long i = 8; i < 32; i++) {
            object.setAccessible(false);
            if(unsafe.getBoolean(object, i)) {
                continue;
            }

            object.setAccessible(true);
            if(unsafe.getBoolean(object, i)) {
                cookie = i;
                break;
            }
        }
        if(cookie == -1) {
            throw new RuntimeException();
        }

        long finalCookie = cookie;
        return (obj) -> unsafe.putBoolean(obj, finalCookie, true);
    }

    static Linker getLinker() {
        var unsafe = getUnsafe();
        var accessible = getAccessible(unsafe);

        MethodHandles.Lookup lookup;
        try {
            var constructor = MethodHandles.Lookup.class.getDeclaredConstructor(Class.class, Class.class, int.class);
            accessible.accept(constructor);
            lookup = constructor.newInstance(Object.class, null, -1);
        } catch(ReflectiveOperationException e) {
            throw new RuntimeException("Failed to create lookup", e);
        }

        try {
            var Windowsx64Linker = lookup.findClass("jdk.internal.foreign.abi.x64.windows.Windowsx64Linker");
            var getInstance = lookup.findStatic(Windowsx64Linker, "getInstance", MethodType.methodType(Windowsx64Linker));
            return (Linker) getInstance.invoke();
        } catch(Throwable e) {
            throw new RuntimeException("Failed to get linker", e);
        }
    }
}
