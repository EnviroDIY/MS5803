#!/bin/sh
# Host harness for the MS5803 library: compile src/MS5803.cpp against the local
# stubs and run every datasheet's worked example through it.
cd "$(dirname "$0")" || exit 1
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -I. -I../../src \
    -o ms5803_test ms5803_test.cpp ../../src/MS5803.cpp || exit 1
exec ./ms5803_test
