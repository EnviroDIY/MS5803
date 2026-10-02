/******************************************************************************
MS5803_I2C.cpp
Library for MS5803 pressure sensor.
Bobby Schulz @ Northern Widget LLC
6/26/2014
https://github.com/sparkfun/MS5803-14BA_Breakout

The MS5803 is a media isolated temperature and pressure sensor made by
Measurment Specialties which can be used to measure either water pressure
and depth, or baramatric (atmospheric) pressure, and altitude along with that

"Instruments register only through things they're designed to register.
Space still contains infinite unknowns."
-Mr. Spock

Distributed as-is; no warranty is given.
******************************************************************************/

#include <Wire.h> // Wire library is used for I2C
#include "MS5803.h"

// NOT IMPLEMENTED, and it applies to the 07BA alone: that part carries a third
// compensation stage above 1100 mbar, and its PROM word 7 is not only the CRC.
// Bits 0-3 are the checksum, bits 4-9 are C7 and bits 10-15 are C8, and
// ENG_DS_MS5803-07BA_B3 Figure 18 then gives
//
//   P3 = P + [(C7 - 2^5) * 2^2 - ((C8 - 2^5) * (T - 20)) / 2^4] * (P - 1100) / 4900
//
// for P above 1100 mbar, with P3 = P below it. A 07BA read through this library
// is therefore uncompensated above 1100 mbar. No 07BA is fitted to any Northern
// Widget board, and the datasheet does not say whether P and T enter that
// formula in mbar and degrees C or in the hundredths the calculation page uses,
// so it is recorded here rather than guessed at. The other five variants' word 7
// is the CRC alone.


MS5803_WireBus MS5803_wire;   // the default bus, declared in MS5803_Bus.h

MS5803::MS5803(uint8_t address, int MaxPressure)
// Base library type I2C
{
    _address = address; //set interface used for communication

    _bus = NULL;   // the default bus, resolved by bus() only if it is used
    _comp.setModel(MaxPressure);
}

MS5803::MS5803(MS5803_Bus& bus, uint8_t address, int MaxPressure)
// The same part, read over a bus the caller supplies.
{
    _address = address;
    _bus = &bus;
    _comp.setModel(MaxPressure);
}

MS5803_Bus& MS5803::bus()
// The bus given to the constructor, or the hardware one when none was.
{
    if(_bus) return *_bus;
    return MS5803_wire;
}

void MS5803::reset(void)
// Reset device I2C
{
   sendCommand(CMD_RESET);
   sensorWait(3);
}

bool MS5803::begin()
// Reset the part, read its PROM coefficients, and report whether it answered.
{
    bus().begin();  // the bus initializer, whichever bus this is

    uint8_t i;
    uint8_t highByte = 0;
    uint8_t lowByte = 0;
    uint16_t prom[8];

    for(i = 0; i <= 7; i++){
        sendCommand(CMD_PROM + (i * 2));
        bus().requestFrom(_address, 2);
        highByte = bus().read();
        lowByte = bus().read();
        prom[i] = (highByte << 8)|lowByte;
    // Uncomment below for debugging output.
       // Serial.print("C");
       // Serial.print(i);
       // Serial.print("= ");
       // Serial.println(prom[i]);
    }
    _comp.setCoefficients(prom);

    bus().beginTransmission(_address);
    // endTransmission() answers 0 on success, so invert it: true means the part
    // acknowledged, which is what every other NW library's begin() means.
    return bus().endTransmission() == 0;
}


bool MS5803::begin(uint8_t address, int MaxPressure)
// Initialize library for subsequent pressure measurements
{
    // Reset address and pressure if given in begin
    _address = address; //set interface used for communication

    _comp.setModel(MaxPressure);

    return begin();
}


float MS5803::getTemperature(temperature_units units, precision _precision)
// Return a temperature reading in either F or C, or NAN for a part whose bar
// figure this library does not know. A caller wanting the reading anyway takes
// getTemperatureADC(), which is what the Walrus does: ask modelKnown(), then
// name the column D2 rather than a temperature.
{
    getMeasurements(_precision);
    if(!_comp.modelKnown()) return NAN;
    float temperature_reported;
    // If Fahrenheit is selected return the temperature converted to F
    if(units == FAHRENHEIT){
        temperature_reported = _comp.temperatureActual() / 100.0f;
        temperature_reported = (((temperature_reported) * 9) / 5) + 32;
        return temperature_reported;
        }

    // If Celsius is selected return the temperature converted to C
    else {
        temperature_reported = _comp.temperatureActual() / 100.0f;
        return temperature_reported;
    }
}


float MS5803::getPressure(precision _precision)
// Return a pressure reading units Pa, or NAN for a part whose bar figure this
// library does not know. A caller wanting the reading anyway takes
// getPressureADC(), which is what the Walrus does: ask modelKnown(), then name
// the column D1 rather than a pressure.
{
    getMeasurements(_precision);
    if(!_comp.modelKnown()) return NAN;
    float pressure_reported;
    pressure_reported = _comp.pressureActual();
    // Divided as a double, which is what float(ConvCoef[4])/100.0 was before
    // this constant carried the divisor itself: a float division here would
    // round once where that rounded twice, and the harness compares bytes.
    pressure_reported = pressure_reported / double(_comp.mbarDivisor());
    return pressure_reported;
}


void MS5803::getMeasurements(precision _precision)
// Read the two conversions off the part, then compensate them.
{
    //Retrieve ADC result
    int32_t temperature_raw = getADCconversion(TEMPERATURE, _precision);
    int32_t pressure_raw = getADCconversion(PRESSURE, _precision);
    _comp.compensate((uint32_t)pressure_raw, (uint32_t)temperature_raw);
}

uint32_t MS5803::getADCconversion(measurement _measurement, precision _precision)
// Retrieve ADC measurement from the device.
// Select measurement type and precision
// TODO:  Set up for asynchronous conversion? - ie, w/o waits
{
    uint32_t result;
    uint8_t highByte = 0;
    uint8_t midByte = 0;
    uint8_t lowByte = 0;

    sendCommand(CMD_ADC_CONV + _measurement + _precision);
    // Wait for conversion to complete
    sensorWait(1); //general delay
    switch( _precision )
    {
        case ADC_256 : sensorWait(1); break;
        case ADC_512 : sensorWait(3); break;
        case ADC_1024: sensorWait(4); break;
        case ADC_2048: sensorWait(6); break;
        case ADC_4096: sensorWait(10); break;
    }

    sendCommand(CMD_ADC_READ);
    bus().requestFrom(_address, 3);

    while(bus().available())
    {
        highByte = bus().read();
        midByte = bus().read();
        lowByte = bus().read();
    }

    result = ((uint32_t)highByte << 16) + ((uint32_t)midByte << 8) + lowByte;

    return result;
}


void MS5803::sendCommand(uint8_t command)
{
    bus().beginTransmission(_address);
    bus().write(command);
    bus().endTransmission();
}


void MS5803::sensorWait(uint8_t time)
// Delay function.  This can be modified to work outside of Arduino based MCU's
{
    delay(time);
};
