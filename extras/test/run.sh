#!/bin/sh
# Host harness for the MS5803 library: compile its sources against the local
# stubs and run every datasheet's worked example through it. The compensation is
# its own translation unit, because a board that reads the part over its own bus
# uses it without the bus half (see MS5803_Compensation.h).
cd "$(dirname "$0")" || exit 1
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -I. -I../../src \
    -o ms5803_test ms5803_test.cpp ../../src/MS5803.cpp ../../src/MS5803_Compensation.cpp || exit 1
exec ./ms5803_test
