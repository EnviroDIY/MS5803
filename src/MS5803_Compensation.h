/**
 * @file MS5803_Compensation.h
 * @brief The MS5803's calibration constants and the datasheet's compensation,
 * apart from any bus.
 */
#ifndef MS5803_Compensation_h
#define MS5803_Compensation_h

#include <Arduino.h>

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
    uint8_t mbarDivisor() const { return MbarDivisor; }

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
};

#endif
