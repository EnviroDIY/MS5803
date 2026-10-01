# MS5803

Arduino library for the MS5803 series of media-isolated pressure transducers (Measurement Specialties, now TE). One class covers every variant: 01BA, 02BA, 05BA, 07BA, 14BA, and 30BA.

## Standards

Follow the NW library standards for all work here. Two sources:
- **Within the NorthernWidget workspace:** the root `CLAUDE.md` (one level above `github/`) has the full checklist and code conventions.
- **Fresh clone / other location:** fetch [NorthernWidget/.github/RELEASING.md](https://github.com/NorthernWidget/.github/blob/main/RELEASING.md) for the release checklist and [NorthernWidget/.github/CONTRIBUTING.md](https://github.com/NorthernWidget/.github/blob/main/CONTRIBUTING.md) for naming and versioning conventions.

## The datasheets are in this repository

Read `docs/` before you touch a number in `setModel()`. All seven datasheets are there, one per variant plus the 01BA07, and each one carries the two pages that matter: "PRESSURE AND TEMPERATURE CALCULATION" and "SECOND ORDER TEMPERATURE COMPENSATION". A conversion constant comes from those pages and from nowhere else.

The trap here has already cost a day of work. An earlier version of `extras/test/compensation_check.py` took its second-order reference from the library source, which made the check agree by construction and hid two wrong 01BA constants. A reference transcribed from the thing it is meant to check is not a check. Render the flowchart pages as images and read them with your own eyes.

## Two instruments, and both must pass

```sh
extras/test/run.sh                          # 60 cases: every worked example, plus cold and hot
python3 extras/test/compensation_check.py   # every constant against the datasheets
```

`compensation_check.py` reads **two** switches: `MS5803::setModel()` here, and `setMS5803Model()` in the Walrus firmware at `../Project-Walrus/Firmware/Walrus_I2C_5BA/`. A sensor compensates its own reading, which is why these constants exist in two places at all, and one datasheet-derived reference checks both of those copies rather than leaving them free to drift apart. It will also tell you when a case omits a constant, because an omission is invisible in the output: the missing constant silently keeps whatever the previous case set.

Treat `run.sh` as the acceptance test for any change to the compensation, and require its output to be byte-identical before and after. Build the previous commit in a worktree and diff the two, rather than trusting that 60 of 60 still passing means nothing moved.

## What this library does not do

- **The 07BA above 1100 mbar is uncompensated.** That variant alone carries a third stage, and its PROM word 7 packs C7 and C8 in beside the CRC. You will find the formula in `src/MS5803.cpp`, recorded and deliberately not implemented, because the datasheet never says whether P and T enter it in mbar and degrees C or in the hundredths that its calculation page works in. No Northern Widget board carries a 07BA.
- **An unrecognised bar figure silently takes the 05BA's constants**, which gives you a confidently wrong pressure and raises nothing at all. `setModel()` is marked `//FIX!` where it happens. The Walrus firmware answers the same problem with a known-model flag and a notice, though whether this library should return NAN instead is still open.

## State, 2026-10-01

There is no tag and no release, and `version=0.1.2`. The repository sits well behind the NW library standard, missing its `README.md`, `CITATION.cff`, `.zenodo.json`, `keywords.txt`, `doxygen_NW.cfg`, and docs workflow, and carrying a dead `.travis.yml` in place of the last of those. Note also that `library.json` names `EnviroDIY/MS5803` as the repository where `library.properties` names `NorthernWidget/MS5803`; the lineage runs back through EnviroDIY to a SparkFun breakout library.

Two things downstream include `MS5803.h`, and between them they fix what you may change freely. `TP-Downhole_Library` asks four things of this library: `reset()`, `begin(address, MaxPressure)`, `getTemperature(units, precision)`, and `getPressure(precision)`. The deployment sketch at `Deployments/Margay/Margay_TP-DownholeLogger` (2018) constructs two objects on one bus, `MS5803 Downhole(ADDRESS_LOW, 2)` and `MS5803 Baro(ADDRESS_HIGH, 2)`, which is why the conversion constants are members here rather than file-scope globals as they are in the Walrus firmware. Everything beyond those leaves you an unusually free hand. Be aware that `TP-Downhole_Library` does not compile today for a reason that has nothing to do with this library: it still calls MCP3421's removed PascalCase names, `Begin`, `SetResolution`, and `GetVoltage`.

The `named-constants` branch is **superseded**, not pending, and you should not merge it. It was an earlier draft of the named-constants change that `master` now carries, and it sits one commit off an older `master`, with a checker that predates the firmware check and a tracked copy of the harness binary. Keep it as history.

## Hard rule

**Never** create a git tag, GitHub release, push to a shared remote, or submit to any external registry (Zenodo, Arduino Library Manager, etc.) unless explicitly asked in the current message. If in doubt, ask.
