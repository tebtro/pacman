#!/bin/bash

mkdir -p ../run_tree
pushd ../run_tree


common_compiler_flags="-Wno-write-strings -Wno-unused-variable -Wno-sign-compare -Wno-null-dereference -Wno-missing-braces -fno-rtti -fno-exceptions -Wno-undefined-internal -DBUILD_INTERNAL=1 -DBUILD_SLOW=1 -DBUILD_LINUX=1"

# build 64-bit
clang --debug $common_compiler_flags ../src/sdl2_pacman.cpp -o linux_pacman.x86_64 -g `../libs/sdl2_x64/bin/sdl2-config --cflags --libs` -Wl,-rpath,'$ORIGIN/x86_64' -lm

# build 32-bit
#clang --debug $common_compiler_flags ../src/sdl2_pacman.cpp -o linux_pacman.x86 -g `../libs/sdl2_x32/bin/sdl2-config --cflags --libs` -Wl,-rpath,'$ORIGIN/x86' -lm

# copy SDL2 into output directory
mkdir -p x86_64
cp ../libs/sdl2_x64/lib/libSDL2-2.0.so.0 x86_64/
mkdir -p x86
cp ../libs/sdl2_x32/lib/libSDL2-2.0.so.0 x86/


popd
