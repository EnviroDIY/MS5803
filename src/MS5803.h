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
#include "MS5803_Protocol.h"
#include "MS5803_Bus.h"
#include "MS5803_Compensation.h"

// Define units for conversions.
enum temperature_units
{
    CELSIUS,
    FAHRENHEIT,
};

// Define measurement type.

class MS5803
{
public:
    MS5803(uint8_t address = 0x77, int MaxPressure = MS5803_MODEL_UNDECLARED);  // hardware TwoWire
    // The same part on another bus: a board whose hardware TWI answers its own
    // logger reaches its MS5803 over a software bus. The bus must outlive this.
    MS5803(MS5803_Bus& bus, uint8_t address = 0x77, int MaxPressure = MS5803_MODEL_UNDECLARED);
    void reset(void);     //Reset device
    /**
     * @brief Start the part: reset it, read its PROM coefficients, and declare
     * which part it is.
     * @param address 0x76 or 0x77, by the CSB pin.
     * @param MaxPressure the bar figure in the order code: 1, 2, 5, 7, 14, 30.
     * @return true when the part acknowledged on the bus.
     */
    bool begin(uint8_t address, int MaxPressure);

    /// @brief Start the part at the address and model already given. @return true when it acknowledged.
    bool begin();

    // Return calculated temperature from sensor
    float getTemperature(temperature_units units, precision _precision);
    // Return calculated pressure from sensor
    float getPressure(precision _precision);

    /**
     * @brief Whether a part this library carries constants for has been
     * declared, by the constructor or by begin().
     * @details When it has not, getPressure() and getTemperature() return NAN
     * rather than converting with another part's constants, while
     * getPressureADC() and getTemperatureADC() still hand back the raw
     * conversions: the reading is kept and can be compensated afterwards, and
     * the units are no longer mbar and C. Ask this before naming a column, as
     * the Walrus library does.
     */
    bool modelKnown() const { return _comp.modelKnown(); }

    /// @brief Declare which part is fitted, by the bar figure in its order code.
    void setModel(int MaxPressure) { _comp.setModel(MaxPressure); }

    /// @brief The last raw conversion of pressure, D1, whatever the model.
    uint32_t getPressureADC() const { return _comp.pressureADC(); }

    /// @brief The last raw conversion of temperature, D2, whatever the model.
    uint32_t getTemperatureADC() const { return _comp.temperatureADC(); }

    /**
     * @brief The part's constants and the datasheet's arithmetic, on their own.
     * @details A board that reads the part over its own bus holds an
     * MS5803_Compensation directly and never needs this class. This accessor is
     * for a caller that has both and wants the integers: Schema 1 registers
     * carry pressure in microbar and temperature in hundredths of a degree, and
     * taking the float back to an integer would round a second time.
     */
    const MS5803_Compensation& compensation() const { return _comp; }

private:
    MS5803_Compensation _comp;  // the part's constants and the datasheet's arithmetic
    MS5803_Bus* _bus;           // the bus this part is read over, or null for the default
    // Resolved here rather than in the constructor so that a board which only
    // compensates - it reads the part over its own bus - never references
    // MS5803_wire, and the link drops the hardware-bus adapter with it.
    MS5803_Bus& bus();

    uint8_t _address;         // Variable used to store I2C device address.

    // The calibration constants for the part that is fitted. Every variant is
    // converted by the same equations in getMeasurements(); only these numbers
    // differ, and setModel() gives them their values. Each is read from that
    // variant's "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER
    // TEMPERATURE COMPENSATION" pages.

    void getMeasurements(precision _precision);   // read the part, then compensate

    void sendCommand(uint8_t command);    // General I2C send command function
    uint32_t getADCconversion(measurement _measurement, precision _precision);    // Retrieve ADC result

    void sensorWait(uint8_t time); // General delay function see: delay()
};

#endif
