# MS5803

Arduino library for the MS5803 series of media-isolated pressure transducers (Measurement Specialties, now TE Connectivity). One library covers every variant: 01BA, 02BA, 05BA, 07BA, 14BA and 30BA.

A part's constants come from the bar figure in its order code, and nothing else selects them. Declare the part you fitted; the library will not guess one.

## Usage

```c++
#include <MS5803.h>

MS5803 sensor;

void setup()
{
    Serial.begin(9600);
    if(!sensor.begin(ADDRESS_HIGH, 14))  //an MS5803-14BA, CSB pulled high
    {
        Serial.println("No MS5803 answered");
    }
}

void loop()
{
    Serial.print(sensor.getPressure(ADC_4096));
    Serial.println(" mbar");
    delay(1000);
}
```

`begin()` returns `true` when the part acknowledged on the bus. The second argument is the bar figure: 1, 2, 5, 7, 14 or 30.

## A part you have not declared

`getPressure()` and `getTemperature()` return `NAN` until a part is declared, rather than converting with another part's constants: that would give you a confidently wrong pressure and raise nothing. Ask `modelKnown()` to tell the two apart.

The reading is not lost. `getPressureADC()` and `getTemperatureADC()` give you D1 and D2 whatever the model, which means a reading taken before the part was identified can be compensated afterwards. The Walrus device does exactly this: with no part named in its calibration page it serves its raw conversions, and its library names those columns D1 and D2 instead of a pressure.

## Four types, one header

`#include <MS5803.h>` and you have all of them. Most callers want `MS5803` and can ignore the rest.

| Type | What it is |
|------|------------|
| `MS5803` | the part on a bus: `reset()`, `begin()`, `getPressure()`, `getTemperature()` |
| `MS5803_Compensation` | the constants and the datasheet's arithmetic, with no bus in it |
| `MS5803_Bus`, `MS5803_WireBus` | the seven bus calls, and the hardware bus that answers them |
| the addresses, commands and conversion options | `ADDRESS_LOW`, `CMD_PROM`, `ADC_4096` and the rest |

`MS5803_Compensation` is there for a board that must read the part itself. A board whose hardware TWI answers its own controller cannot use the hardware bus for its sensors. The Walrus reaches its MS5803 over a software bus with its own timing and its own fault handling, and therefore reads the two conversions itself and hands them over.

```c++
MS5803_Compensation ms5803;

ms5803.setModel(5);                 //an MS5803-05BA
ms5803.setCoefficients(prom);       //the eight PROM words this board read
ms5803.compensate(D1, D2);          //the two conversions this board read
int32_t mbar100 = ms5803.pressureActual();        //divide by mbarDivisor()
int32_t centiC  = ms5803.temperatureActual();     //hundredths of a degree
```

The integer accessors are there because a device serving registers needs integers: taking the float back to an integer would round a second time.

## Another bus

`MS5803` reads the part over the hardware `Wire` bus unless it is given another. Pass any object implementing `MS5803_Bus` (see that header: seven calls, a line each) to read the part over a software bus, a second hardware bus, or a mock.

## The datasheets are in this repository

`docs/` holds all seven, one per variant plus the 01BA07. A conversion constant comes from that variant's "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER TEMPERATURE COMPENSATION" pages and from nowhere else.

Two instruments check them, and both must pass:

```sh
extras/test/run.sh                          # every worked example, plus cold and hot
python3 extras/test/compensation_check.py   # every constant against the datasheets
```

## What this library does not do

**The 07BA above 1100 mbar is uncompensated.** That variant alone carries a third stage, and its PROM word 7 packs C7 and C8 in beside the CRC. The formula is recorded in `src/MS5803_Compensation.cpp` and deliberately not implemented, because the datasheet never says whether P and T enter it in mbar and degrees Celsius or in the hundredths its calculation page works in. No Northern Widget board carries a 07BA.

## License

GPL-3.0. See `LICENSE`.

## Authors

Bobby Schulz, Andrew D. Wickert and Chad Sandell, Northern Widget LLC. The lineage runs back through EnviroDIY to a SparkFun breakout library.
