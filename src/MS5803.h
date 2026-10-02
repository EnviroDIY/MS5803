/******************************************************************************
MS5803_I2C.h
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

#ifndef MS5803_h
#define MS5803_h

#include <Arduino.h>
#include "MS5803_Bus.h"

// Define units for conversions.
enum temperature_units
{
    CELSIUS,
    FAHRENHEIT,
};

// Define measurement type.
enum measurement
{
    PRESSURE = 0x00,
    TEMPERATURE = 0x10
};

// Define constants for Conversion precision
enum precision
{
    ADC_256  = 0x00,
    ADC_512  = 0x02,
    ADC_1024 = 0x04,
    ADC_2048 = 0x06,
    ADC_4096 = 0x08
};

// Define address choices for the device (I2C mode)
// enum ms5803_addr
// {
//     ADDRESS_HIGH = 0x76,
//     ADDRESS_LOW  = 0x77
// };

//Commands
#define CMD_RESET 0x1E // reset command
#define CMD_ADC_READ 0x00 // ADC read command
#define CMD_ADC_CONV 0x40 // ADC conversion command

#define CMD_PROM 0xA0 // Coefficient location

#define ADDRESS_LOW 0x77
#define ADDRESS_HIGH 0x76

class MS5803
{
public:
    MS5803(uint8_t address = 0x77, int MaxPressure = 2);  // Constructor, hardware TwoWire
    // The same part on another bus: a board whose hardware TWI answers its own
    // logger reaches its MS5803 over a software bus. The bus must outlive this.
    MS5803(MS5803_Bus& bus, uint8_t address = 0x77, int MaxPressure = 2);
    void reset(void);     //Reset device
    uint8_t begin(uint8_t address, int MaxPressure); // Collect coefficients from sensor
    uint8_t begin(); // Collect coefficients from sensor

    // Return calculated temperature from sensor
    float getTemperature(temperature_units units, precision _precision);
    // Return calculated pressure from sensor
    float getPressure(precision _precision);

    // Whether the bar figure given to the constructor or to begin() names a
    // part this library carries constants for. When it does not, getPressure()
    // and getTemperature() hand back the raw D1 and D2 conversions rather than
    // converting with another part's constants: the reading is kept and can be
    // compensated afterwards, and the units are no longer mbar and C. Ask this
    // before naming a column, as the Walrus library does.
    bool modelKnown() const { return _modelKnown; }

    // The datasheet's compensation over conversions the caller already read,
    // for a board that reaches the part over its own bus: the Walrus firmware
    // does, and carried a second copy of the constants and the arithmetic for
    // that reason. Call setModel() first; the results are then available
    // through getPressure(), getTemperature(), pressureActual() and
    // temperatureActual().
    void compensate(uint32_t D1, uint32_t D2);

    // The part's eight PROM words, for a board that read them over its own
    // bus. begin() does this itself when the library owns the bus.
    void setCoefficients(const uint16_t* c);

    // The compensated values as the datasheet's integers, which is what a
    // device serving Schema 1 registers needs: pressure in the part's own
    // units before the mbar divisor, temperature in hundredths of a degree.
    int32_t pressureActual() const { return _pressure_actual; }
    int32_t temperatureActual() const { return _temperature_actual; }

    // The divisor taking pressureActual() to mbar, from the fitted part.
    uint8_t mbarDivisor() const { return MbarDivisor; }

    // The last raw conversions, whatever the model: D1 is pressure, D2 is
    // temperature, both 24-bit.
    uint32_t getPressureADC() const { return _pressure_adc; }
    uint32_t getTemperatureADC() const { return _temperature_adc; }

private:
    MS5803_Bus* _bus;           // the bus this part is read over; never null
    bool _modelKnown = false;   // set by setModel(); false for a bar figure it does not know
    uint32_t _pressure_adc = 0;      // D1, the last raw pressure conversion
    uint32_t _temperature_adc = 0;   // D2, the last raw temperature conversion
    int32_t _temperature_actual;
    int32_t _pressure_actual;

    uint8_t _address;         // Variable used to store I2C device address.
    uint16_t coefficient[8];// Coefficients;

    // The calibration constants for the part that is fitted. Every variant is
    // converted by the same equations in getMeasurements(); only these numbers
    // differ, and setModel() gives them their values. Each is read from that
    // variant's "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER
    // TEMPERATURE COMPENSATION" pages.
    uint8_t OffShift;           // OFF  = C2 << OffShift + (C4 * dT) >> OffDtShift
    uint8_t OffDtShift;
    uint8_t SensShift;          // SENS = C1 << SensShift + (C3 * dT) >> SensDtShift
    uint8_t SensDtShift;
    uint8_t MbarDivisor;        // the converted value divided by this is mbar
    uint8_t T2MultCold;         // below 20 C
    uint8_t T2ShiftCold;
    uint8_t Off2MultCold;
    uint8_t Off2ShiftCold;
    uint8_t Sens2MultCold;
    uint8_t Sens2ShiftCold;
    uint8_t Off2MultVeryCold;   // below -15 C
    uint8_t Sens2MultVeryCold;
    uint8_t T2MultHot;          // 20 C and above
    uint8_t T2ShiftHot;
    uint8_t Off2MultHot;
    uint8_t Sens2MultVeryHot;   // above 45 C, the 01BA alone
    uint8_t PShift;             // P = (D1 * SENS / 2^21 - OFF) / 2^PShift

    void getMeasurements(precision _precision);   // read the part, then compensate

    void setModel(int MaxPressure);   // the constants for a part's bar figure
    void sendCommand(uint8_t command);    // General I2C send command function
    uint32_t getADCconversion(measurement _measurement, precision _precision);    // Retrieve ADC result

    void sensorWait(uint8_t time); // General delay function see: delay()
};

#endif
