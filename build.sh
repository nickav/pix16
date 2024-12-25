#!/bin/bash
set -e # exit on error

# Config
project_root="$(cd "$(dirname "$0")" && pwd -P)"
exe_name="pix16_debug"

pushd $project_root
    build_hash=$(more "$project_root/.git/refs/heads/main")

    mkdir -p build

    pushd build
        flags="-std=c++11 -Wno-deprecated-declarations -Wno-int-to-void-pointer-cast -Wno-writable-strings -Wno-dangling-else -Wno-switch -Wno-undefined-internal -Wno-logical-op-parentheses"
        libs="$(pkg-config --libs sdl2)"
        ~/bin/ntime clang++ -DDEBUG=1 -DBUILD_HASH=$build_hash -I ../src -I ../src/third_party $flags $libs ../src/pix16_sdl2.cpp -o $exe_name

        ./$exe_name
    popd
popd