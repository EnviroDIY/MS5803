/**
 * @file MS5803_Compensation.cpp
 * @brief See MS5803_Compensation.h.
 */
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

void MS5803_Compensation::setModel(int MaxPressure)
// The calibration constants for the part that is fitted, from the bar figure in
// its order code. A wrong number here gives a confidently wrong pressure and
// raises no fault, so extras/test/compensation_check.py compares every one of
// them with the datasheets. Run it after any edit.
{
  _modelKnown = true;  // cleared again by the default case below
  switch (MaxPressure) {
    case (1):  // MS5803-01BA
      _offShift = 16;
      _offDtShift = 7;
      _sensShift = 15;
      _sensDtShift = 8;
      _mbarDivisor = 100;
      _t2MultCold = 1;
      _t2ShiftCold = 31;
      _off2MultCold = 3;
      _off2ShiftCold = 0;
      _sens2MultCold = 7;
      _sens2ShiftCold = 3;
      _off2MultVeryCold = 0;
      _sens2MultVeryCold = 2;
      _t2MultHot = 0;
      _t2ShiftHot = 0;
      _off2MultHot = 0;
      _sens2MultVeryHot = 1;
      _pShift = 15;
      break;

    case (2):  // MS5803-02BA
      _offShift = 17;
      _offDtShift = 6;
      _sensShift = 16;
      _sensDtShift = 7;
      _mbarDivisor = 100;
      _t2MultCold = 1;
      _t2ShiftCold = 31;
      _off2MultCold = 61;
      _off2ShiftCold = 4;
      _sens2MultCold = 2;
      _sens2ShiftCold = 0;
      _off2MultVeryCold = 20;
      _sens2MultVeryCold = 12;
      _t2MultHot = 0;
      _t2ShiftHot = 0;
      _off2MultHot = 0;
      _sens2MultVeryHot = 0;
      _pShift = 15;
      break;

    // A bar figure this library does not know no longer takes the 05BA's
    // constants, which is what it used to do: that returned a confidently
    // wrong pressure and raised nothing (Andy, 2026-10-02, "MS5803 should
    // have no silent default"). The constants are left untouched here, and
    // the reading stops after the raw conversions, which getPressure() and
    // getTemperature() hand back instead. Nothing is thrown away, and a
    // caller can compensate afterwards once the part is identified.
    default:
      _modelKnown = false;
      break;

    case (5):  // MS5803-05BA
      _offShift = 18;
      _offDtShift = 5;
      _sensShift = 17;
      _sensDtShift = 7;
      _mbarDivisor = 100;
      _t2MultCold = 3;
      _t2ShiftCold = 33;
      _off2MultCold = 3;
      _off2ShiftCold = 3;
      _sens2MultCold = 7;
      _sens2ShiftCold = 3;
      _off2MultVeryCold = 0;
      _sens2MultVeryCold = 3;
      _t2MultHot = 0;
      _t2ShiftHot = 0;
      _off2MultHot = 0;
      _sens2MultVeryHot = 0;
      _pShift = 15;
      break;

    case (7):  // MS5803-07BA
      _offShift = 18;
      _offDtShift = 5;
      _sensShift = 17;
      _sensDtShift = 6;
      _mbarDivisor = 100;
      _t2MultCold = 3;
      _t2ShiftCold = 33;
      _off2MultCold = 3;
      _off2ShiftCold = 3;
      _sens2MultCold = 7;
      _sens2ShiftCold = 3;
      _off2MultVeryCold = 0;
      _sens2MultVeryCold = 3;
      _t2MultHot = 0;
      _t2ShiftHot = 0;
      _off2MultHot = 0;
      _sens2MultVeryHot = 0;
      _pShift = 15;
      break;

    case (14):  // MS5803-14BA
      _offShift = 16;
      _offDtShift = 7;
      _sensShift = 15;
      _sensDtShift = 8;
      _mbarDivisor = 10;
      _t2MultCold = 3;
      _t2ShiftCold = 33;
      _off2MultCold = 3;
      _off2ShiftCold = 1;
      _sens2MultCold = 5;
      _sens2ShiftCold = 3;
      _off2MultVeryCold = 7;
      _sens2MultVeryCold = 4;
      _t2MultHot = 7;
      _t2ShiftHot = 37;
      _off2MultHot = 1;
      _sens2MultVeryHot = 0;
      _pShift = 15;
      break;

    case (30):  // MS5803-30BA
      _offShift = 16;
      _offDtShift = 7;
      _sensShift = 15;
      _sensDtShift = 8;
      _mbarDivisor = 10;
      _t2MultCold = 3;
      _t2ShiftCold = 33;
      _off2MultCold = 3;
      _off2ShiftCold = 1;
      _sens2MultCold = 5;
      _sens2ShiftCold = 3;
      _off2MultVeryCold = 7;
      _sens2MultVeryCold = 4;
      _t2MultHot = 7;
      _t2ShiftHot = 37;
      _off2MultHot = 1;
      _sens2MultVeryHot = 0;
      _pShift = 13;
      break;
  }
}

void MS5803_Compensation::setCoefficients(const uint16_t* c)
// The PROM words a board read for itself, in the datasheet's order.
{
  for (uint8_t i = 0; i < 8; i++) coefficient[i] = c[i];
}

void MS5803_Compensation::compensate(uint32_t D1, uint32_t D2)
// The datasheet's compensation, over conversions the caller already has. A
// board that reads the part itself calls this rather than carrying a second
// copy of the arithmetic: the Walrus firmware reaches its MS5803 over a
// software bus, and used to duplicate every constant and every line below.
{
  int32_t temperature_raw = (int32_t)D2;
  int32_t pressure_raw = (int32_t)D1;
  _temperature_adc = D2;
  _pressure_adc = D1;

  // A part whose bar figure this library does not know stops here: the raw
  // conversions stand, and getPressureADC() and getTemperatureADC() hand
  // those back. Compensating would mean shifting by another part's amounts,
  // which is how this used to return a confidently wrong pressure. The Walrus
  // device does the same with no model on Page 1, and its library names the
  // columns D1 and D2 when it does.
  if (!_modelKnown) return;

  //Create Variables for calculations
  int32_t temp_calc;
  int32_t pressure_calc;

  int32_t dT;

  //Now that we have a raw temperature, let's compute our actual.
  dT = temperature_raw - ((int32_t)coefficient[5] << 8);
  temp_calc = (((int64_t)dT * coefficient[6]) >> 23) + 2000;

  // TODO TESTING  _temperature_actual = temp_calc;

  //Now we have our first order Temperature, let's calculate the second order.
  int64_t T2, OFF2, SENS2, OFF, SENS;  //working variables

  //Squared terms in 64 bits: 61 * (-40C - 20C)^2 overflows an int32 on the 02BA,
  //and the divisors are powers of two, so a shift is exact where pow() was not.
  int64_t TempSq = (int64_t)(temp_calc - 2000) * (temp_calc - 2000);

  if (temp_calc < 2000)
  // If temp_calc is below 20.0C
  //LOW TEMP
  {
    T2 = _t2MultCold * (((int64_t)dT * dT) >> _t2ShiftCold);
    OFF2 = _off2MultCold * TempSq / ((int64_t)1 << _off2ShiftCold);
    SENS2 = _sens2MultCold * TempSq / ((int64_t)1 << _sens2ShiftCold);

    if (temp_calc < -1500)
    // If temp_calc is below -15.0C
    //VERY LOW TEMP
    {
      int64_t ColdSq = (int64_t)(temp_calc + 1500) * (temp_calc + 1500);
      OFF2 = OFF2 + _off2MultVeryCold * ColdSq;
      SENS2 = SENS2 + _sens2MultVeryCold * ColdSq;
    }
  } else
  // If temp_calc is above 20.0C
  //HIGH TEMP
  {
    T2 = _t2MultHot * ((int64_t)dT * dT) / ((int64_t)1 << _t2ShiftHot);
    OFF2 = _off2MultHot * TempSq / 16;
    SENS2 = 0;

    //Entry 17 is 1 on the 01BA, whose flow chart alone carries this term,
    //and 0 on every other variant, where it subtracts nothing. A constant
    //rather than a test on the model.
    if (temp_calc > 4500) SENS2 = SENS2 - _sens2MultVeryHot * ((int64_t)(temp_calc - 4500) * (temp_calc - 4500)) / 8;
  }

  // Now bring it all together to apply offsets


  OFF = ((int64_t)coefficient[2] << _offShift) + (((coefficient[4] * (int64_t)dT)) >> _offDtShift);
  SENS = ((int64_t)coefficient[1] << _sensShift) + (((coefficient[3] * (int64_t)dT)) >> _sensDtShift);

  temp_calc = temp_calc - T2;
  OFF = OFF - OFF2;
  SENS = SENS - SENS2;

  // Now lets calculate the pressure
  pressure_calc = (((SENS * pressure_raw) / 2097152) - OFF) / ((int64_t)1 << _pShift);

  _temperature_actual = temp_calc;
  _pressure_actual = pressure_calc;  // 10;// pressure_calc;
}
