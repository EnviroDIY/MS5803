#!/usr/bin/env python3
"""Check the library's conversion table against the six MS5803 datasheets.

src/MS5803.cpp holds one eighteen-entry row per variant, and every entry is an
exponent or a multiplier taken from that variant's calculation pages. A wrong
entry produces a confidently wrong pressure and raises no fault, so the table is
checked here rather than trusted.

    python3 compensation_check.py
    python3 compensation_check.py --emit     # regression cases for ms5803_test.cpp

Three things run:

1. Every row of ConvTable in src/MS5803.cpp is compared with the reference
   below, entry by entry.

2. Each variant's worked example is reproduced. Every datasheet prints one set
   of D1 and D2 with the coefficients that produced them and the pressure and
   temperature they must give, which checks the reference itself rather than
   only the source. It reaches the first-order exponents, the scale in entry 4,
   and the final shift in entry 16; it does not reach the second-order terms,
   because five of the six examples sit within half a degree of 20 C.

3. --emit prints the pressures this reference gives across temperature. The
   datasheets publish no cold worked example, so those are what ms5803_test.cpp
   carries to hold the second-order terms still. Two implementations of the same
   flow chart agreeing is the check; regenerate only when the datasheets say to.

**Where the reference came from, and a trap to avoid.** Entries 0 to 4 and 16
are from the "PRESSURE AND TEMPERATURE CALCULATION" page and entries 5 to 15
from the "SECOND ORDER TEMPERATURE COMPENSATION" flow chart, read variant by
variant on 2026-09-28. An earlier pass took the second-order entries from the
library itself, which made this check agree with the code by construction and
hid two wrong entries in the 01BA row. A reference transcribed from the thing
it is meant to check is not a check.

One datasheet typo is expected rather than reproduced: the 05BA prints
P = 100002 in its table and then calls it 1100.02 mbar beside it. The table
value is what its own page produces, and 1100.02 mbar is the 02BA's example
pressure carried across.
"""

import re
import sys
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "src" / "MS5803.cpp"
# The Walrus firmware carries the same table, because a sensor compensates its
# own reading. Checked here too: one reference, both copies. Its path is given
# relative to the workspace these repositories sit in side by side.
FIRMWARE = Path(__file__).resolve().parents[3] / "Project-Walrus" / "Firmware" / "Walrus_I2C_5BA" / "Walrus_I2C_5BA.ino"

# Per variant: the datasheet's typical C1..C6, its worked D1 and D2, the
# pressure and temperature that page prints, and the seventeen-entry table.
VARIANTS = {
    "01BA": dict(
        C=[40127, 36924, 23317, 23282, 33464, 28312], D1=9085466, D2=8569150, P=100009, T=2007,
        table=[16, 7, 15, 8, 10000, 1, 31, 3, 0, 7, 3, 0, 2, 0, 0, 0, 15, 1]),
    "02BA": dict(
        C=[46372, 43981, 29059, 27842, 31553, 28165], D1=6465444, D2=8077636, P=110002, T=2000,
        table=[17, 6, 16, 7, 10000, 1, 31, 61, 4, 2, 0, 20, 12, 0, 0, 0, 15, 0]),
    "05BA": dict(
        C=[32428, 28100, 44250, 20003, 32747, 28266], D1=5251266, D2=8383686, P=100002, T=2001,
        table=[18, 5, 17, 7, 10000, 3, 33, 3, 3, 7, 3, 0, 3, 0, 0, 0, 15, 0]),
    "07BA": dict(
        C=[46128, 40903, 27765, 26239, 31876, 28261], D1=10332180, D2=8592698, P=599209, T=3456,
        table=[18, 5, 17, 6, 10000, 3, 33, 3, 3, 7, 3, 0, 3, 0, 0, 0, 15, 0]),
    "14BA": dict(
        C=[46546, 42845, 29751, 29457, 32745, 29059], D1=4311550, D2=8387300, P=10005, T=2015,
        table=[16, 7, 15, 8, 1000, 3, 33, 3, 1, 5, 3, 7, 4, 7, 37, 1, 15, 0]),
    "30BA": dict(
        C=[29112, 26814, 19125, 17865, 32057, 31305], D1=4944364, D2=8198974, P=59998, T=1971,
        table=[16, 7, 15, 8, 1000, 3, 33, 3, 1, 5, 3, 7, 4, 7, 37, 1, 13, 0]),
}

MODEL = {"01BA": 1, "02BA": 2, "05BA": 3, "07BA": 4, "14BA": 5, "30BA": 6}


def compensate(C, cc, D1, D2, model=0):
    """The flow chart's arithmetic, in the library's order of operations."""
    C1, C2, C3, C4, C5, C6 = C
    dT = D2 - (C5 << 8)
    T = ((dT * C6) >> 23) + 2000
    TempSq = (T - 2000) ** 2
    if T < 2000:
        T2 = cc[5] * ((dT * dT) >> cc[6])
        OFF2 = cc[7] * TempSq // (1 << cc[8])
        SENS2 = cc[9] * TempSq // (1 << cc[10])
        if T < -1500:
            ColdSq = (T + 1500) ** 2
            OFF2 += cc[11] * ColdSq
            SENS2 += cc[12] * ColdSq
    else:
        T2 = cc[13] * (dT * dT) // (1 << cc[14])
        OFF2 = cc[15] * TempSq // 16
        SENS2 = 0
        if T >= 4500 and model == 1:
            SENS2 -= (T - 4500) ** 2 // 8
    OFF = (C2 << cc[0]) + ((C4 * dT) >> cc[1]) - OFF2
    SENS = (C1 << cc[2]) + ((C3 * dT) >> cc[3]) - SENS2
    return T - T2, ((SENS * D1) // 2097152 - OFF) // (1 << cc[16])


def declared_table(path=None):
    """ConvTable as a source file declares it, one list per model."""
    path = path or SOURCE
    if not path.exists():
        return "missing"
    src = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"ConvTable\[6\]\[18\][^=]*= \{(.*?)\n\};", src, re.S)
    if not m:
        return None
    return [[int(v) for v in row.split(",") if v.strip()]
            for row in re.findall(r"\{([^}]*)\}", m.group(1))]


def check_firmware_defines():
    """The Walrus firmware's live COEF0..COEF15, against the datasheets.

    The firmware is built for one variant: sixteen uncommented #defines, with
    the other variants' blocks commented out below them. So the check is which
    variant those sixteen are, and whether they are that variant's numbers
    exactly. It cannot reach the last two constants, because the firmware has
    neither: it hardcodes the final divisor at 32768 and carries no
    above-45 C term at all, which is right for the 05BA and wrong for a 30BA or
    an 01BA. That is a gap in the firmware, not in this check.
    """
    if not FIRMWARE.exists():
        print(f"  {FIRMWARE} is not beside this repository; skipped")
        return 0
    src = FIRMWARE.read_text(encoding="utf-8", errors="replace")
    live = {}
    for m in re.finditer(r"^#define COEF(\d+)\s+(\d+)", src, re.M):
        live[int(m.group(1))] = int(m.group(2))
    if len(live) != 16:
        print(f"  found {len(live)} live COEF defines, expected 16"); return 1
    got = [live[i] for i in range(16)]
    for name, v in VARIANTS.items():
        if got == v["table"][:16]:
            print(f"  built for the MS5803-{name}, and every constant matches its datasheet")
            if v["table"][16] != 15:
                print(f"  BUT the firmware hardcodes the final divisor at 32768 and the "
                      f"{name} needs 2^{v['table'][16]}")
                return 1
            if v["table"][17]:
                print(f"  BUT the {name} needs an above-45 C term the firmware does not have")
                return 1
            return 0
    print("  matches no variant's datasheet constants:")
    for name, v in VARIANTS.items():
        diff = [f"COEF{i}={got[i]} not {w}" for i, w in enumerate(v["table"][:16]) if got[i] != w]
        print(f"    against the {name}: " + "; ".join(diff))
    return 1


def d2_for(C, cc, model, celsius_x100):
    """The D2 whose *reported* temperature is this, after the T2 correction."""
    lo, hi = 0, 1 << 24
    while lo < hi:
        mid = (lo + hi) // 2
        if compensate(C, cc, 0, mid, model)[0] < celsius_x100:
            lo = mid + 1
        else:
            hi = mid
    return lo


def d1_for(C, cc, D2, model, mbar):
    """The D1 that gives this pressure, by bisection."""
    lo, hi = 0, 1 << 24
    while lo < hi:
        mid = (lo + hi) // 2
        if compensate(C, cc, mid, D2, model)[1] / (cc[4] / 100.0) < mbar:
            lo = mid + 1
        else:
            hi = mid
    return lo


# The rated range is -40 to +85 C. The two above 45 C reach the 01BA's own
# very-high-temperature branch, which no other variant has.
CASES = (-4000, -2000, 0, 2000, 4000, 4500, 4600, 6000, 8500)   # reported, 0.01 C


def emit():
    """Regression cases for ms5803_test.cpp, computed from the flow chart."""
    for name, v in VARIANTS.items():
        for Tx in CASES:
            D2 = d2_for(v["C"], v["table"], MODEL[name], Tx)
            D1 = d1_for(v["C"], v["table"], D2, MODEL[name], 1013.25)
            T, P = compensate(v["C"], v["table"], D1, D2, MODEL[name])
            print('  {"%s", %2d, {%s}, %8d, %8d, %10.2f, %7.2f},'
                  % (name, {1: 1, 2: 2, 3: 5, 4: 7, 5: 14, 6: 30}[MODEL[name]],
                     ", ".join(str(c) for c in v["C"]), D1, D2,
                     P / (v["table"][4] / 100.0), T / 100.0))


def main(argv):
    if argv and argv[0] == "--emit":
        emit()
        return 0

    failures = 0
    print(f"ConvTable in {SOURCE.name}, against the datasheets:\n")
    declared = declared_table()
    if declared in (None, "missing"):
        print("  ConvTable[6][18] not found in the source")
        return 1
    for i, (name, v) in enumerate(VARIANTS.items()):
        got, want = declared[i] if i < len(declared) else None, v["table"]
        if got is None:
            print(f"{name:>8}  row {i} missing"); failures += 1
        elif len(got) != 18:
            print(f"{name:>8}  row {i} has {len(got)} entries, not 18: {got}"); failures += 1
        elif got != want:
            diff = "; ".join(f"[{k}] {a} should be {b}"
                             for k, (a, b) in enumerate(zip(got, want)) if a != b)
            print(f"{name:>8}  {diff}"); failures += 1
        else:
            print(f"{name:>8}  matches")

    print(f"\nThe live #define COEF block in {FIRMWARE.name}:\n")
    failures += check_firmware_defines()

    print("\nWorked example from each datasheet:\n")
    print(f"{'variant':>8} {'P wanted':>10} {'P got':>10} {'T wanted':>10} {'T got':>8}  verdict")
    for name, v in VARIANTS.items():
        T, P = compensate(v["C"], v["table"], v["D1"], v["D2"], MODEL[name])
        ok = (P == v["P"]) and (T == v["T"])
        failures += 0 if ok else 1
        print(f"{name:>8} {v['P']:>10} {P:>10} {v['T']:>10} {T:>8}  {'ok' if ok else 'FAIL'}")

    if failures:
        print(f"\n{failures} check(s) failed", file=sys.stderr)
        return 1
    print("\nThe table matches the datasheets, and every variant reproduces its worked example.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
