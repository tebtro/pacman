#!/bin/bash

mkdir -p ../run_tree
pushd ../run_tree

c++ ../src/sdl2_pacman.cpp -o linux_pacman -g -I/usr/include/SDL2 -D_REENTRANT -lSDL2


popd