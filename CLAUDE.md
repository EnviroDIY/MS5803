# MS5803

Arduino library for the MS5803 series of media-isolated pressure transducers (Measurement Specialties, now TE). One library covers every variant: 01BA, 02BA, 05BA, 07BA, 14BA, and 30BA.

Three headers, each one thing, split on 2026-10-02 because the Walrus firmware needs the second alone:
- `MS5803.h` is the part on a bus: `reset()`, `begin()` reading the PROM, `getMeasurements()`, and the float getters.
- `MS5803_Compensation.h` is the constant table and the datasheet's arithmetic, with no bus in it. A board that reads the part itself holds one of these and nothing else.
- `MS5803_Protocol.h` is the addresses, commands and conversion options, which both halves want and neither owns.

`MS5803` holds an `MS5803_Compensation` and delegates to it. The constants are members rather than file-scope globals, and that is load-bearing: see the two-objects-on-one-bus note below.

## Standards

Follow the NW library standards for all work here. Two sources:
- **Within the NorthernWidget workspace:** the root `CLAUDE.md` (one level above `github/`) has the full checklist and code conventions.
- **Fresh clone / other location:** fetch [NorthernWidget/.github/RELEASING.md](https://github.com/NorthernWidget/.github/blob/main/RELEASING.md) for the release checklist and [NorthernWidget/.github/CONTRIBUTING.md](https://github.com/NorthernWidget/.github/blob/main/CONTRIBUTING.md) for naming and versioning conventions.

## The datasheets are in this repository

Read `docs/` before you touch a number in `setModel()`. All seven datasheets are there, one per variant plus the 01BA07, and each one carries the two pages that matter: "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER TEMPERATURE COMPENSATION". A conversion constant comes from those pages and from nowhere else.

The trap here has already cost a day of work. An earlier version of `extras/test/compensation_check.py` took its second-order reference from the library source, which made the check agree by construction and hid two wrong 01BA constants. A reference transcribed from the thing it is meant to check is not a check. Render the flowchart pages as images and read them with your own eyes.

## Two instruments, and both must pass

```sh
extras/test/run.sh                          # every worked example, cold and hot, and the undeclared part
python3 extras/test/compensation_check.py   # every constant against the datasheets
```

`compensation_check.py` reads **one** switch, `MS5803_Compensation::setModel()` in `src/MS5803_Compensation.cpp`. It used to read two: the Walrus firmware carried its own table and its own arithmetic, and this check existed to hold the two copies in agreement. The firmware compensates through this library now (Project-Walrus 16e5d9b), and the check is inverted at that end: it **fails** if `setMS5803Model()` reappears in the firmware, because a second table is a regression rather than something to verify twice. It will also tell you when a case omits a constant, because an omission is invisible in the output: the missing constant silently keeps whatever the previous case set.

Treat `run.sh` as the acceptance test for any change to the compensation, and require its output to be byte-identical before and after. Build the previous commit in a worktree and diff the two, rather than trusting that 60 of 60 still passing means nothing moved.

## What this library does not do

- **The 07BA above 1100 mbar is uncompensated.** That variant alone carries a third stage, and its PROM word 7 packs C7 and C8 in beside the CRC. You will find the formula in `src/MS5803_Compensation.cpp`, recorded and deliberately not implemented, because the datasheet never says whether P and T enter it in mbar and degrees C or in the hundredths that its calculation page works in. No Northern Widget board carries a 07BA.
- **It does not guess a part.** Fixed 2026-10-02, and worth knowing because the old behaviour was silent: an unrecognised bar figure used to take the 05BA's constants, and the constructor defaulted to the 02BA, which meant a part nobody declared was compensated as one anyway. Both are gone. `MS5803_MODEL_UNDECLARED` is the default, `getPressure()` and `getTemperature()` return NAN until a part is declared, `modelKnown()` tells the two apart, and `getPressureADC()`/`getTemperatureADC()` still hand back D1 and D2, which keeps the reading for compensation afterwards. The harness proves it, and the old behaviour returned 2200.04 mbar where the worked example's answer is 1100.02.

## State, 2026-10-02

There is no tag and no release, and `version=0.1.2`, which is what made the API corrections of 2026-10-02 free to make: `begin()` now returns `bool` with true for success, where it used to return `endTransmission()`'s code and 0 meant success, which made `if(sensor.begin())` read as success and mean failure.

`README.md`, `keywords.txt` and `doxygen_NW.cfg` were added 2026-10-02. Still missing against the NW library standard: `CITATION.cff`, `.zenodo.json`, the docs workflow and `_docs/`, and `.doxybook/config.json`. The dead `.travis.yml` is still there in place of the workflow. `CITATION.cff` and `.zenodo.json` need the concept DOI, and the version needs Andy's word. Those four are held rather than forgotten. Note also that `library.json` names `EnviroDIY/MS5803` as the repository where `library.properties` names `NorthernWidget/MS5803`; the lineage runs back through EnviroDIY to a SparkFun breakout library.

Two things downstream include `MS5803.h`, and between them they fix what you may change freely. `TP-Downhole_Library` asks four things of this library: `reset()`, `begin(address, MaxPressure)`, `getTemperature(units, precision)`, and `getPressure(precision)`. All four survive, though `begin()` returns `bool` now and that library assigns it to a `uint8_t`. It is retired and does not compile anyway (TP-Downhole_Library#6 records why, and is not to be fixed). The deployment sketch at `Deployments/Margay/Margay_TP-DownholeLogger` (2018) constructs two objects on one bus, `MS5803 Downhole(ADDRESS_LOW, 2)` and `MS5803 Baro(ADDRESS_HIGH, 2)`, which is why the conversion constants are members here rather than file-scope globals as they are in the Walrus firmware. Everything beyond those leaves you an unusually free hand.

Three more findings in that library, from 2026-10-01, and the first one blocks giving it standard names. Its header reads `Pressure [mBar], Temp DH [C], Temp DHt [C],` while `GetString()` writes `getPressure()`, `getTemperature(0)`, `getTemperature(1)`, and `getTemperature(0)` is the **MS5803 die** where `getTemperature(1)` is the thermistor. Walrus_Library, which descends from this one, uses `Temp DH` for the medium and `Temp DHt` for the transducer, so one of the two libraries has its temperature columns the wrong way round. Furthermore `getTemperature()` returns nothing at all (`TP_Downhole.cpp:169`, "no return statement in function returning non-void") and `getTemperature(uint8_t Location)` falls off its end for any Location above 1 (line 163). Both are the compiler's words, not a reading.

The `named-constants` branch is **superseded**, not pending, and you should not merge it. It was an earlier draft of the named-constants change that `master` now carries, and it sits one commit off an older `master`, with a checker that predates the firmware check and a tracked copy of the harness binary. Keep it as history.

## Hard rule

**Never** create a git tag, GitHub release, push to a shared remote, or submit to any external registry (Zenodo, Arduino Library Manager, etc.) unless explicitly asked in the current message. If in doubt, ask.
