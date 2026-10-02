/**
 * @file MS5803_Protocol.h
 * @brief The MS5803's I2C addresses, commands and conversion options.
 */
#ifndef MS5803_Protocol_h
#define MS5803_Protocol_h

#include <Arduino.h>

/**
 * @brief What to send the part and where to send it.
 * @details Separate from both halves of the library because both want it and
 * neither owns it. MS5803 uses these to read the part over a bus it holds, and
 * a board that reads the part itself uses them over its own bus: the Walrus
 * firmware did, and defined its own copy of all four commands with the same
 * values and no guard, so changing one would have left the other stale.
 */

// The two addresses the CSB pin selects.
#define ADDRESS_LOW 0x77
#define ADDRESS_HIGH 0x76

// Commands.
#define CMD_RESET 0x1E      // reset command
#define CMD_ADC_READ 0x00   // ADC read command
#define CMD_ADC_CONV 0x40   // ADC conversion command
#define CMD_PROM 0xA0       // Coefficient location

/// Which conversion to ask for, added to CMD_ADC_CONV.
enum measurement
{
    PRESSURE    = 0x00,
    TEMPERATURE = 0x10
};

/// The oversampling ratio, added to CMD_ADC_CONV: more bits, longer wait.
enum precision
{
    ADC_256  = 0x00,
    ADC_512  = 0x02,
    ADC_1024 = 0x04,
    ADC_2048 = 0x06,
    ADC_4096 = 0x08
};

#endif
