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

    _bus = &MS5803_wire;
    setModel(MaxPressure);
}

MS5803::MS5803(MS5803_Bus& bus, uint8_t address, int MaxPressure)
// The same part, read over a bus the caller supplies.
{
    _address = address;
    _bus = &bus;
    setModel(MaxPressure);
}

void MS5803::setModel(int MaxPressure)
// The calibration constants for the part that is fitted, from the bar figure in
// its order code. A wrong number here gives a confidently wrong pressure and
// raises no fault, so extras/test/compensation_check.py compares every one of
// them with the datasheets. Run it after any edit.
{
    _modelKnown = true;   // cleared again by the default case below
    switch(MaxPressure)
    {
        case (1):   // MS5803-01BA
            OffShift = 16;
            OffDtShift = 7;
            SensShift = 15;
            SensDtShift = 8;
            MbarDivisor = 100;
            T2MultCold = 1;
            T2ShiftCold = 31;
            Off2MultCold = 3;
            Off2ShiftCold = 0;
            Sens2MultCold = 7;
            Sens2ShiftCold = 3;
            Off2MultVeryCold = 0;
            Sens2MultVeryCold = 2;
            T2MultHot = 0;
            T2ShiftHot = 0;
            Off2MultHot = 0;
            Sens2MultVeryHot = 1;
            PShift = 15;
            break;

        case (2):   // MS5803-02BA
            OffShift = 17;
            OffDtShift = 6;
            SensShift = 16;
            SensDtShift = 7;
            MbarDivisor = 100;
            T2MultCold = 1;
            T2ShiftCold = 31;
            Off2MultCold = 61;
            Off2ShiftCold = 4;
            Sens2MultCold = 2;
            Sens2ShiftCold = 0;
            Off2MultVeryCold = 20;
            Sens2MultVeryCold = 12;
            T2MultHot = 0;
            T2ShiftHot = 0;
            Off2MultHot = 0;
            Sens2MultVeryHot = 0;
            PShift = 15;
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

        case (5):   // MS5803-05BA
            OffShift = 18;
            OffDtShift = 5;
            SensShift = 17;
            SensDtShift = 7;
            MbarDivisor = 100;
            T2MultCold = 3;
            T2ShiftCold = 33;
            Off2MultCold = 3;
            Off2ShiftCold = 3;
            Sens2MultCold = 7;
            Sens2ShiftCold = 3;
            Off2MultVeryCold = 0;
            Sens2MultVeryCold = 3;
            T2MultHot = 0;
            T2ShiftHot = 0;
            Off2MultHot = 0;
            Sens2MultVeryHot = 0;
            PShift = 15;
            break;

        case (7):   // MS5803-07BA
            OffShift = 18;
            OffDtShift = 5;
            SensShift = 17;
            SensDtShift = 6;
            MbarDivisor = 100;
            T2MultCold = 3;
            T2ShiftCold = 33;
            Off2MultCold = 3;
            Off2ShiftCold = 3;
            Sens2MultCold = 7;
            Sens2ShiftCold = 3;
            Off2MultVeryCold = 0;
            Sens2MultVeryCold = 3;
            T2MultHot = 0;
            T2ShiftHot = 0;
            Off2MultHot = 0;
            Sens2MultVeryHot = 0;
            PShift = 15;
            break;

        case (14):   // MS5803-14BA
            OffShift = 16;
            OffDtShift = 7;
            SensShift = 15;
            SensDtShift = 8;
            MbarDivisor = 10;
            T2MultCold = 3;
            T2ShiftCold = 33;
            Off2MultCold = 3;
            Off2ShiftCold = 1;
            Sens2MultCold = 5;
            Sens2ShiftCold = 3;
            Off2MultVeryCold = 7;
            Sens2MultVeryCold = 4;
            T2MultHot = 7;
            T2ShiftHot = 37;
            Off2MultHot = 1;
            Sens2MultVeryHot = 0;
            PShift = 15;
            break;

        case (30):   // MS5803-30BA
            OffShift = 16;
            OffDtShift = 7;
            SensShift = 15;
            SensDtShift = 8;
            MbarDivisor = 10;
            T2MultCold = 3;
            T2ShiftCold = 33;
            Off2MultCold = 3;
            Off2ShiftCold = 1;
            Sens2MultCold = 5;
            Sens2ShiftCold = 3;
            Off2MultVeryCold = 7;
            Sens2MultVeryCold = 4;
            T2MultHot = 7;
            T2ShiftHot = 37;
            Off2MultHot = 1;
            Sens2MultVeryHot = 0;
            PShift = 13;
            break;
    }
}

void MS5803::reset(void)
// Reset device I2C
{
   sendCommand(CMD_RESET);
   sensorWait(3);
}


uint8_t MS5803::begin()
// Initialize library for subsequent pressure measurements
{
    _bus->begin();  // the bus initializer, whichever bus this is

    uint8_t i;
    uint8_t highByte = 0;
    uint8_t lowByte = 0;

    for(i = 0; i <= 7; i++){
        sendCommand(CMD_PROM + (i * 2));
        _bus->requestFrom(_address, 2);
        highByte = _bus->read();
        lowByte = _bus->read();
        coefficient[i] = (highByte << 8)|lowByte;
    // Uncomment below for debugging output.
       // Serial.print("C");
       // Serial.print(i);
       // Serial.print("= ");
       // Serial.println(coefficient[i]);
    }

    _bus->beginTransmission(_address);
    return _bus->endTransmission(); //Return sucess or failue of I2C connection
}


uint8_t MS5803::begin(uint8_t address, int MaxPressure)
// Initialize library for subsequent pressure measurements
{
    // Reset address and pressure if given in begin
    _address = address; //set interface used for communication

    setModel(MaxPressure);

    return begin();
}


float MS5803::getTemperature(temperature_units units, precision _precision)
// Return a temperature reading in either F or C, or NAN for a part whose bar
// figure this library does not know. A caller wanting the reading anyway takes
// getTemperatureADC(), which is what the Walrus does: ask modelKnown(), then
// name the column D2 rather than a temperature.
{
    getMeasurements(_precision);
    if(!_modelKnown) return NAN;
    float temperature_reported;
    // If Fahrenheit is selected return the temperature converted to F
    if(units == FAHRENHEIT){
        temperature_reported = _temperature_actual / 100.0f;
        temperature_reported = (((temperature_reported) * 9) / 5) + 32;
        return temperature_reported;
        }

    // If Celsius is selected return the temperature converted to C
    else {
        temperature_reported = _temperature_actual / 100.0f;
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
    if(!_modelKnown) return NAN;
    float pressure_reported;
    pressure_reported = _pressure_actual;
    // Divided as a double, which is what float(ConvCoef[4])/100.0 was before
    // this constant carried the divisor itself: a float division here would
    // round once where that rounded twice, and the harness compares bytes.
    pressure_reported = pressure_reported / double(MbarDivisor);
    return pressure_reported;
}


void MS5803::getMeasurements(precision _precision)
// Gets resuts from ADC and stores them into internal variables
{
    //Retrieve ADC result
    int32_t temperature_raw = getADCconversion(TEMPERATURE, _precision);
    int32_t pressure_raw = getADCconversion(PRESSURE, _precision);
    _temperature_adc = temperature_raw;
    _pressure_adc = pressure_raw;

    // A part whose bar figure this library does not know stops here: the raw
    // conversions stand, and getPressure() and getTemperature() hand those back
    // instead. Compensating would mean shifting by another part's amounts, which
    // is how this used to return a confidently wrong pressure. The Walrus device
    // does the same thing with no model on Page 1, and its header names the
    // columns D1 and D2 when it does.
    if(!_modelKnown) return;

    //Create Variables for calculations
    int32_t temp_calc;
    int32_t pressure_calc;

    int32_t dT;

    //Now that we have a raw temperature, let's compute our actual.
    dT = temperature_raw - ((int32_t)coefficient[5] << 8);
    temp_calc = (((int64_t)dT * coefficient[6]) >> 23) + 2000;

    // TODO TESTING  _temperature_actual = temp_calc;

    //Now we have our first order Temperature, let's calculate the second order.
    int64_t T2, OFF2, SENS2, OFF, SENS; //working variables

    //Squared terms in 64 bits: 61 * (-40C - 20C)^2 overflows an int32 on the 02BA,
    //and the divisors are powers of two, so a shift is exact where pow() was not.
    int64_t TempSq = (int64_t)(temp_calc - 2000) * (temp_calc - 2000);

    if (temp_calc < 2000)
    // If temp_calc is below 20.0C
        //LOW TEMP
    {
        T2 = T2MultCold* (((int64_t)dT * dT) >> T2ShiftCold);
        OFF2 = Off2MultCold * TempSq / ((int64_t)1 << Off2ShiftCold);
        SENS2 = Sens2MultCold * TempSq / ((int64_t)1 << Sens2ShiftCold);

        if(temp_calc < -1500)
        // If temp_calc is below -15.0C
            //VERY LOW TEMP
        {
            int64_t ColdSq = (int64_t)(temp_calc + 1500) * (temp_calc + 1500);
            OFF2 = OFF2 + Off2MultVeryCold * ColdSq;
            SENS2 = SENS2 + Sens2MultVeryCold * ColdSq;
        }
    }
    else
    // If temp_calc is above 20.0C
        //HIGH TEMP
    {
        T2 = T2MultHot * ((int64_t)dT * dT) / ((int64_t)1 << T2ShiftHot);
        OFF2 = Off2MultHot * TempSq / 16;
        SENS2 = 0;

        //Entry 17 is 1 on the 01BA, whose flow chart alone carries this term,
        //and 0 on every other variant, where it subtracts nothing. A constant
        //rather than a test on the model.
        if(temp_calc > 4500) SENS2 = SENS2 - Sens2MultVeryHot * ((int64_t)(temp_calc - 4500) * (temp_calc - 4500))/8;
    }

    // Now bring it all together to apply offsets


    OFF = ((int64_t)coefficient[2] << OffShift) + (((coefficient[4] * (int64_t)dT)) >> OffDtShift);
    SENS = ((int64_t)coefficient[1] << SensShift) + (((coefficient[3] * (int64_t)dT)) >> SensDtShift);

    temp_calc = temp_calc - T2;
    OFF = OFF - OFF2;
    SENS = SENS - SENS2;

    // Now lets calculate the pressure
    pressure_calc = (((SENS * pressure_raw) / 2097152 ) - OFF) / ((int64_t)1 << PShift);

    _temperature_actual = temp_calc ;
    _pressure_actual = pressure_calc ; // 10;// pressure_calc;
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
    _bus->requestFrom(_address, 3);

    while(_bus->available())
    {
        highByte = _bus->read();
        midByte = _bus->read();
        lowByte = _bus->read();
    }

    result = ((uint32_t)highByte << 16) + ((uint32_t)midByte << 8) + lowByte;

    return result;
}


void MS5803::sendCommand(uint8_t command)
{
    _bus->beginTransmission(_address);
    _bus->write(command);
    _bus->endTransmission();
}


void MS5803::sensorWait(uint8_t time)
// Delay function.  This can be modified to work outside of Arduino based MCU's
{
    delay(time);
};
