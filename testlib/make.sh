#!/bin/sh

x86_64-w64-mingw32-gcc -fPIC -Iinclude -shared main.cpp -o ./test.dll
