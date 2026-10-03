/**
 * @file MS5803.h
 * @brief Arduino library for the MS5803 series of pressure transducers: 01BA,
 * 02BA, 05BA, 07BA, 14BA and 30BA.
 * @details Four types, in the order a reader meets them.
 *   - the part's addresses, commands and conversion options;
 *   - MS5803_Bus, the seven bus calls reading the part needs, and the
 *     MS5803_WireBus that wraps the hardware bus;
 *   - MS5803_Compensation, the constant table and the datasheet's arithmetic,
 *     which a board reading the part over its own bus uses on its own;
 *   - MS5803, the part on a bus, which holds one of those and delegates.
 * Most callers want MS5803 and can ignore the rest.
 */
#ifndef MS5803_h
#define MS5803_h

#include <Arduino.h>
#include <Wire.h>

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

enum temperature_units
{
    CELSIUS,
    FAHRENHEIT,
};

// Define measurement type.

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
    // No virtual destructor on purpose. A bus is a static object held for the
    // program's life and is never deleted through this pointer, and declaring
    // one pulls operator delete, and with it malloc and free, into the link:
    // 586 bytes of an ATtiny1634's flash for a destructor nobody calls.
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

// The bar figure in a part's order code, which is what selects its
// compensation constants: MS5803-01BA, -02BA, -05BA, -07BA, -14BA, -30BA.
// A part is not assumed. Until one is declared, nothing is compensated and
// modelKnown() says so, because the alternative is converting with another
// part's constants and returning a confidently wrong pressure.
#define MS5803_MODEL_UNDECLARED 0

/**
 * @brief Turn an MS5803's two raw conversions into a pressure and a
 * temperature, for the part that is fitted.
 * @details This is the half of an MS5803 that has nothing to do with a bus,
 * and it is separate for a reason worth stating. A board whose hardware TWI
 * answers its own controller must reach its MS5803 over a software bus with
 * its own timing and its own fault handling: the Walrus firmware does, and it
 * used to carry a second copy of every constant below and every line of the
 * arithmetic. It holds one of these instead, and reads the part itself.
 *
 * Give it a part with setModel() and that part's PROM with setCoefficients(),
 * then hand it each pair of conversions. A caller that wants the library to do
 * the reading as well uses MS5803, which holds one of these.
 *
 * The constants are the datasheet's. Every variant is converted by the same
 * equations; only these numbers differ, and each is read from that variant's
 * "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER TEMPERATURE
 * COMPENSATION" pages. A wrong number here gives a confidently wrong pressure
 * and raises no fault, so extras/test/compensation_check.py compares every one
 * of them with the six datasheets. Run it after any edit.
 */
class MS5803_Compensation
{
public:
    /// @param MaxPressure the bar figure in the order code: 1, 2, 5, 7, 14, 30.
    MS5803_Compensation(int MaxPressure = MS5803_MODEL_UNDECLARED) { setModel(MaxPressure); }

    /// @brief The constants for a part's bar figure. A figure with no such part is refused.
    void setModel(int MaxPressure);

    /// @brief The part's eight PROM words, in the datasheet's order.
    void setCoefficients(const uint16_t* c);

    /// @brief Compensate one pair of conversions. D1 is pressure, D2 temperature.
    void compensate(uint32_t D1, uint32_t D2);

    /// @brief Whether a part this library carries constants for has been declared.
    bool modelKnown() const { return _modelKnown; }

    /// @brief The compensated pressure as the datasheet's integer; divide by mbarDivisor() for mbar.
    int32_t pressureActual() const { return _pressure_actual; }

    /// @brief The compensated temperature in hundredths of a degree Celsius.
    int32_t temperatureActual() const { return _temperature_actual; }

    /// @brief The divisor taking pressureActual() to mbar, from the fitted part.
    uint8_t mbarDivisor() const { return _mbarDivisor; }

    /// @brief The last conversions given to compensate(): D1, the pressure count.
    uint32_t pressureADC() const { return _pressure_adc; }

    /// @brief The last conversions given to compensate(): D2, the temperature count.
    uint32_t temperatureADC() const { return _temperature_adc; }

private:
    uint16_t coefficient[8];    // the part's PROM, from setCoefficients()
    bool _modelKnown = false;   // set by setModel(); false for a bar figure it does not know
    uint32_t _pressure_adc = 0;      // D1, as given to compensate()
    uint32_t _temperature_adc = 0;   // D2, as given to compensate()
    int32_t _temperature_actual = 0; // hundredths of a degree Celsius
    int32_t _pressure_actual = 0;    // the datasheet's integer, before MbarDivisor

    // The constants that differ between variants; setModel() gives their values.
    uint8_t _offShift;           // OFF  = C2 << OffShift + (C4 * dT) >> OffDtShift
    uint8_t _offDtShift;
    uint8_t _sensShift;          // SENS = C1 << SensShift + (C3 * dT) >> SensDtShift
    uint8_t _sensDtShift;
    uint8_t _mbarDivisor;        // the converted value divided by this is mbar
    uint8_t _t2MultCold;         // below 20 C
    uint8_t _t2ShiftCold;
    uint8_t _off2MultCold;
    uint8_t _off2ShiftCold;
    uint8_t _sens2MultCold;
    uint8_t _sens2ShiftCold;
    uint8_t _off2MultVeryCold;   // below -15 C
    uint8_t _sens2MultVeryCold;
    uint8_t _t2MultHot;          // 20 C and above
    uint8_t _t2ShiftHot;
    uint8_t _off2MultHot;
    uint8_t _sens2MultVeryHot;   // above 45 C, the 01BA alone
    uint8_t _pShift;             // P = (D1 * SENS / 2^21 - OFF) / 2^PShift
};

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
