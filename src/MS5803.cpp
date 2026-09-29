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

// Conversion constants per variant, one row per model, taken from that
// variant's "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER
// TEMPERATURE COMPENSATION" pages. The entries are:
//
//   [0]  OFF  = C2 << [0]                [9]  SENS2 low-temperature multiplier
//   [1]         + (C4 * dT) >> [1]       [10] SENS2 low-temperature divisor exponent
//   [2]  SENS = C1 << [2]                [11] OFF2 very-low-temperature multiplier
//   [3]         + (C3 * dT) >> [3]       [12] SENS2 very-low-temperature multiplier
//   [4]  100 x the divisor from the      [13] T2 high-temperature multiplier
//        compensated value to mbar       [14] T2 high-temperature divisor exponent
//   [5]  T2 low-temperature multiplier   [15] OFF2 high-temperature multiplier
//   [6]  T2 low-temperature divisor exp  [16] final pressure divisor exponent
//                                        [17] SENS2 very-high-temperature multiplier
//   [7]  OFF2 low-temperature multiplier
//   [8]  OFF2 low-temperature divisor exponent
//
// A wrong entry gives a confidently wrong pressure and raises no fault, so
// extras/test/compensation_check.py reads this table out of this file and
// compares it with the datasheets entry by entry. Run it after any edit here.
static const uint16_t ConvTable[6][18] = {
    {16, 7, 15, 8, 10000, 1, 31,  3, 0, 7, 3,  0,  2, 0,  0, 0, 15, 1},  //01BA
    {17, 6, 16, 7, 10000, 1, 31, 61, 4, 2, 0, 20, 12, 0,  0, 0, 15, 0},  //02BA
    {18, 5, 17, 7, 10000, 3, 33,  3, 3, 7, 3,  0,  3, 0,  0, 0, 15, 0},  //05BA
    {18, 5, 17, 6, 10000, 3, 33,  3, 3, 7, 3,  0,  3, 0,  0, 0, 15, 0},  //07BA
    {16, 7, 15, 8,  1000, 3, 33,  3, 1, 5, 3,  7,  4, 7, 37, 1, 15, 0},  //14BA
    {16, 7, 15, 8,  1000, 3, 33,  3, 1, 5, 3,  7,  4, 7, 37, 1, 13, 0},  //30BA
};

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


MS5803::MS5803(uint8_t address, int MaxPressure)
// Base library type I2C
{
    _address = address; //set interface used for communication

    switch(MaxPressure)
    {
        // Set model number based on maximum pressure range
        case (1): {Model = 1; break;}    //BA01
        case (2): {Model = 2; break;}    //BA02
        case (5): {Model = 3; break;}    //BA05
        case (7): {Model = 4; break;}    //BA07
        case (14): {Model = 5; break;}   //BA14
        case (30): {Model = 6; break;}   //BA30
        default: {Model = 3; break;}    //BA05
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
    Wire.begin();  // Arduino Wire library initializer

    uint8_t i;
    uint8_t highByte = 0;
    uint8_t lowByte = 0;

    for(i = 0; i <= 7; i++){
        sendCommand(CMD_PROM + (i * 2));
        Wire.requestFrom((int)_address, 2);
        highByte = Wire.read();
        lowByte = Wire.read();
        coefficient[i] = (highByte << 8)|lowByte;
    // Uncomment below for debugging output.
       // Serial.print("C");
       // Serial.print(i);
       // Serial.print("= ");
       // Serial.println(coefficient[i]);
    }

    memcpy(ConvCoef, ConvTable[Model - 1], sizeof(ConvCoef));

    Wire.beginTransmission((int)_address);
    return Wire.endTransmission(); //Return sucess or failue of I2C connection
}


uint8_t MS5803::begin(uint8_t address, int MaxPressure)
// Initialize library for subsequent pressure measurements
{
    // Reset address and pressure if given in begin
    _address = address; //set interface used for communication

    switch(MaxPressure)
    {
        // Set model number based on maximum pressure range
        case (1): {Model = 1; break;}    //BA01
        case (2): {Model = 2; break;}    //BA02
        case (5): {Model = 3; break;}    //BA05
        case (7): {Model = 4; break;}    //BA07
        case (14): {Model = 5; break;}   //BA14
        case (30): {Model = 6; break;}   //BA30
        default: {Model = 3; break;}    //BA05
    }

    return begin();
}


float MS5803::getTemperature(temperature_units units, precision _precision)
// Return a temperature reading in either F or C.
{
    getMeasurements(_precision);
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
// Return a pressure reading units Pa.
{
    getMeasurements(_precision);
    float pressure_reported;
    pressure_reported = _pressure_actual;
    // pressure_reported = pressure_reported / 10;
    pressure_reported = pressure_reported / (float(ConvCoef[4])/100.0); //05BA model!
    return pressure_reported;
}


void MS5803::getMeasurements(precision _precision)
// Gets resuts from ADC and stores them into internal variables
{
    //Retrieve ADC result
    int32_t temperature_raw = getADCconversion(TEMPERATURE, _precision);
    int32_t pressure_raw = getADCconversion(PRESSURE, _precision);

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
        T2 = ConvCoef[5]* (((int64_t)dT * dT) >> ConvCoef[6]);
        OFF2 = ConvCoef[7] * TempSq / ((int64_t)1 << ConvCoef[8]);
        SENS2 = ConvCoef[9] * TempSq / ((int64_t)1 << ConvCoef[10]);

        if(temp_calc < -1500)
        // If temp_calc is below -15.0C
            //VERY LOW TEMP
        {
            int64_t ColdSq = (int64_t)(temp_calc + 1500) * (temp_calc + 1500);
            OFF2 = OFF2 + ConvCoef[11] * ColdSq;
            SENS2 = SENS2 + ConvCoef[12] * ColdSq;
        }
    }
    else
    // If temp_calc is above 20.0C
        //HIGH TEMP
    {
        T2 = ConvCoef[13] * ((int64_t)dT * dT) / ((int64_t)1 << ConvCoef[14]);
        OFF2 = ConvCoef[15] * TempSq / 16;
        SENS2 = 0;

        //Entry 17 is 1 on the 01BA, whose flow chart alone carries this term,
        //and 0 on every other variant, where it subtracts nothing. A constant
        //rather than a test on the model.
        if(temp_calc > 4500) SENS2 = SENS2 - ConvCoef[17] * ((int64_t)(temp_calc - 4500) * (temp_calc - 4500))/8;
    }

    // Now bring it all together to apply offsets


    OFF = ((int64_t)coefficient[2] << ConvCoef[0]) + (((coefficient[4] * (int64_t)dT)) >> ConvCoef[1]);    //05BA model!
    SENS = ((int64_t)coefficient[1] << ConvCoef[2]) + (((coefficient[3] * (int64_t)dT)) >> ConvCoef[3]); //05BA model!
    // OFF = ((int64_t)coefficient[2] << 16) + (((coefficient[4] * (int64_t)dT)) >> 7);
    // SENS = ((int64_t)coefficient[1] << 15) + (((coefficient[3] * (int64_t)dT)) >> 8);

    temp_calc = temp_calc - T2;
    OFF = OFF - OFF2;
    SENS = SENS - SENS2;

    // Now lets calculate the pressure
    pressure_calc = (((SENS * pressure_raw) / 2097152 ) - OFF) / ((int64_t)1 << ConvCoef[16]);

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
    Wire.requestFrom((int)_address, 3);

    while(Wire.available())
    {
        highByte = Wire.read();
        midByte = Wire.read();
        lowByte = Wire.read();
    }

    result = ((uint32_t)highByte << 16) + ((uint32_t)midByte << 8) + lowByte;

    return result;
}


void MS5803::sendCommand(uint8_t command)
{
    Wire.beginTransmission(_address);
    Wire.write(command);
    Wire.endTransmission();
}


void MS5803::sensorWait(uint8_t time)
// Delay function.  This can be modified to work outside of Arduino based MCU's
{
    delay(time);
};
