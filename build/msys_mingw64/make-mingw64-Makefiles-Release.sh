#!/bin/sh
cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=./toolChain.cmake -DCMAKE_SH="CMAKE_SH-NOTFOUND" ../../src
