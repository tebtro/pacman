#!/bin/bash

mkdir -p ../run_tree
pushd ../run_tree

clang --debug -Wno-null-dereference -Wno-writable-strings -Wno-undefined-internal ../src/sdl2_pacman.cpp -o linux_pacman -g -I/usr/include/SDL2 -DBUILD_INTERNAL=1 -DBUILD_SLOW=1 -DBUILD_LINUX=1 -D_REENTRANT -lSDL2 -lm -fno-exceptions


popd