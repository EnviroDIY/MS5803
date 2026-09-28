// Minimal Arduino stub for the host harness. The MS5803 library needs almost
// nothing from Arduino.h: integer types, delay(), and the math pow() that
// MS5803.cpp calls. NW_Core's shared stubs are not used here, because they
// model a Schema 1 register file and the MS5803 is a command-response part.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>   // the Arduino core brings string.h in through Arduino.h

inline uint32_t& _millis_counter() { static uint32_t t = 0; return t; }
inline uint32_t millis() { return _millis_counter(); }
inline void delay(uint32_t ms) { _millis_counter() += ms; }
