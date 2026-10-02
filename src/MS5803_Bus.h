/**
 * @file MS5803_Bus.h
 * @brief The I2C bus an MS5803 is read over, so that the part is read the same
 * way wherever it is fitted.
 */
#ifndef MS5803_Bus_h
#define MS5803_Bus_h

#include <Arduino.h>
#include <Wire.h>

/**
 * @brief The seven bus calls reading an MS5803 needs.
 * @details A logger reads the part over the hardware TwoWire bus. A board whose
 * hardware TWI is the peripheral side cannot: the Walrus answers its logger on
 * TWI and reaches its own MS5803 over a software bus, and its firmware used to
 * carry a second copy of this library's constants and compensation for that
 * reason alone. Pass the bus instead, and one implementation serves both.
 *
 * The methods match the Wire API, so an adapter is a line each. Neither
 * TwoWire nor SlowSoftWire can be used directly: both derive from Stream, and
 * none of the four transaction calls is on Stream or virtual.
 */
class MS5803_Bus
{
public:
    virtual ~MS5803_Bus() {}
    virtual void begin() = 0;
    virtual void beginTransmission(uint8_t address) = 0;
    virtual size_t write(uint8_t data) = 0;
    virtual uint8_t endTransmission() = 0;
    virtual uint8_t requestFrom(uint8_t address, uint8_t quantity) = 0;
    virtual int available() = 0;
    virtual int read() = 0;
};

/// The hardware TwoWire bus, which is what a logger or a plain sketch uses.
class MS5803_WireBus : public MS5803_Bus
{
public:
    void begin() override { Wire.begin(); }
    void beginTransmission(uint8_t address) override { Wire.beginTransmission((int)address); }
    size_t write(uint8_t data) override { return Wire.write(data); }
    uint8_t endTransmission() override { return Wire.endTransmission(); }
    uint8_t requestFrom(uint8_t address, uint8_t quantity) override
    {
        return Wire.requestFrom((int)address, (int)quantity);
    }
    int available() override { return Wire.available(); }
    int read() override { return Wire.read(); }
};

/// The default bus, used by the constructors that do not name one.
extern MS5803_WireBus MS5803_wire;

#endif
