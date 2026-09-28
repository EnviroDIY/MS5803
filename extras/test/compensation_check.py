#!/usr/bin/env python3
"""Check this library's conversion constants against the MS5803 datasheets.

The library holds one sixteen-entry table per variant, ConvTemp1 to ConvTemp6 in
MS5803.cpp, and every entry is an exponent or a multiplier taken from that
variant's "PRESSURE AND TEMPERATURE CALCULATION" page. A wrong entry produces a
confidently wrong pressure and raises no fault, so the table is checked here
rather than trusted.

Three checks run:

0. Every ConvTempN in src/MS5803.cpp is compared with the table the datasheet
   requires, entry by entry. The remaining checks use the datasheet's table, so
   this is the one that says whether the shipped source agrees with it.

1. Each variant's worked example is reproduced. Every datasheet prints one set of
   D1 and D2 with the coefficients that produced them and the pressure and
   temperature they must give. Reproducing it exercises the first-order
   exponents, the pressure scale in entry 4, and the final shift. It does not
   exercise the second-order terms, because five of the six examples sit within
   half a degree of 20 C where those terms are zero.

2. The cost of the truncated copy is reported. MS5803::begin() loads a table with
   memcpy(ConvCoef, ConvTempN, 16). On AVR an int is two bytes, so the table is
   32 bytes and eight of the sixteen entries never arrive; ConvCoef belongs to a
   global object, so the missing eight read as zero. Every one of them is a
   second-order temperature term, which is why the error appears only in the
   cold.

    python3 compensation_check.py

One datasheet typo is worked around rather than reproduced. The 05BA prints
P = 100002 in its table, which is 1000.02 mbar, and then says "= 1100.02 mbar"
in the line beside it. The table value is the correct one: it is what this
arithmetic gives from that page's own D1, D2 and coefficients, and 1100.02 mbar
is the 02BA's example pressure, carried across. Expect 100002.

Exits non-zero if any worked example fails to reproduce.
"""

import re
import sys
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "src" / "MS5803.cpp"

# Each entry: the datasheet's typical C1..C6, its worked D1 and D2, the pressure
# and temperature it prints, the library's constant table, and the final shift
# in P = (D1 * SENS / 2^21 - OFF) / 2^shift. Read from ENG_DS_MS5803-*_B3.pdf,
# 2026-09-28.
VARIANTS = {
    "01BA": dict(
        C=[40127, 36924, 23317, 23282, 33464, 28312],
        D1=9085466, D2=8569150, P=100009, T=2007, shift=15,
        table=[16, 7, 15, 8, 10000, 1, 31, 3, 0, 7, 0, 0, 3, 0, 0, 0],
    ),
    "02BA": dict(
        C=[46372, 43981, 29059, 27842, 31553, 28165],
        D1=6465444, D2=8077636, P=110002, T=2000, shift=15,
        table=[17, 6, 16, 7, 10000, 1, 31, 61, 4, 2, 0, 20, 12, 0, 0, 0],
    ),
    "05BA": dict(
        C=[32428, 28100, 44250, 20003, 32747, 28266],
        D1=5251266, D2=8383686, P=100002, T=2001, shift=15,
        table=[18, 5, 17, 7, 10000, 3, 33, 3, 3, 7, 3, 0, 3, 0, 0, 0],
    ),
    "07BA": dict(
        C=[46128, 40903, 27765, 26239, 31876, 28261],
        D1=10332180, D2=8592698, P=599209, T=3456, shift=15,
        table=[18, 5, 17, 6, 10000, 3, 33, 3, 3, 7, 3, 0, 3, 0, 0, 0],
    ),
    "14BA": dict(
        C=[46546, 42845, 29751, 29457, 32745, 29059],
        D1=4311550, D2=8387300, P=10005, T=2015, shift=15,
        table=[16, 7, 15, 8, 1000, 3, 33, 3, 1, 5, 3, 7, 4, 7, 37, 1],
    ),
    "30BA": dict(
        C=[29112, 26814, 19125, 17865, 32057, 31305],
        D1=4944364, D2=8198974, P=59998, T=1971, shift=13,
        table=[16, 7, 15, 8, 1000, 3, 33, 3, 1, 5, 3, 7, 4, 7, 37, 1],
    ),
}


def compensate(C, cc, shift, D1, D2):
    """The datasheet's own arithmetic, in the library's order of operations."""
    C1, C2, C3, C4, C5, C6 = C
    dT = D2 - (C5 << 8)
    T = ((dT * C6) >> 23) + 2000
    if T < 2000:
        T2 = cc[5] * ((dT * dT) >> cc[6]) if cc[6] else 0
        OFF2 = cc[7] * (T - 2000) ** 2 // 2 ** cc[8]
        SENS2 = cc[9] * (T - 2000) ** 2 // 2 ** cc[10]
        if T < -1500:
            OFF2 += cc[11] * (T + 1500) ** 2
            SENS2 += cc[12] * (T + 1500) ** 2
    else:
        T2 = cc[13] * (dT * dT) // 2 ** cc[14] if cc[14] else 0
        OFF2 = cc[15] * (T - 2000) ** 2 // 16
        SENS2 = 0
    OFF = (C2 << cc[0]) + ((C4 * dT) >> cc[1]) - OFF2
    SENS = (C1 << cc[2]) + ((C3 * dT) >> cc[3]) - SENS2
    return T - T2, ((SENS * D1) // 2097152 - OFF) // 2 ** shift


def declared_tables():
    """The ConvTemp1..ConvTemp6 initialisers as MS5803.cpp declares them."""
    src = SOURCE.read_text(encoding="utf-8", errors="replace")
    found = {}
    for m in re.finditer(r"int (ConvTemp(\d))\[\] = \{([^}]*)\}", src):
        found[int(m.group(2))] = [int(v) for v in m.group(3).split(",")]
    return found


def main():
    failures = 0

    print(f"Tables as declared in {SOURCE.name}:\n")
    declared = declared_tables()
    order = list(VARIANTS)
    for i, name in enumerate(order, start=1):
        got = declared.get(i)
        want = VARIANTS[name]["table"]
        if got is None:
            print(f"{name:>8}  ConvTemp{i} not found in the source")
            failures += 1
        elif len(got) != 16:
            print(f"{name:>8}  ConvTemp{i} declares {len(got)} initialisers, not 16: {got}")
            failures += 1
        elif got != want:
            diff = [f"[{k}] {a} should be {b}" for k, (a, b) in enumerate(zip(got, want)) if a != b]
            print(f"{name:>8}  ConvTemp{i} differs from the datasheet: {'; '.join(diff)}")
            failures += 1
        else:
            print(f"{name:>8}  ConvTemp{i} matches the datasheet")
    print()

    print("Worked example from each datasheet, using the datasheet's table:\n")
    print(f"{'variant':>8} {'P wanted':>10} {'P got':>10} {'T wanted':>10} {'T got':>8}  verdict")
    for name, v in VARIANTS.items():
        T, P = compensate(v["C"], v["table"], v["shift"], v["D1"], v["D2"])
        ok = (P == v["P"]) and (T == v["T"])
        failures += 0 if ok else 1
        print(f"{name:>8} {v['P']:>10} {P:>10} {v['T']:>10} {T:>8}  {'ok' if ok else 'FAIL'}")

    print("\nError from the truncated memcpy, in mbar at a true 1013.25 mbar:\n")
    cols = (2500, 1000, 500, 0, -1000, -2000)
    print(f"{'variant':>8} " + " ".join(f"{c/100:>8.0f}C" for c in cols))
    for name, v in VARIANTS.items():
        cut = v["table"][:8] + [0] * 8
        row = []
        for Tset in cols:
            dT = (Tset - 2000) * 2 ** 23 // v["C"][5]
            D2 = dT + (v["C"][4] << 8)
            lo, hi = 0, 1 << 24        # find the D1 that gives 1013.25 mbar
            while lo < hi:
                mid = (lo + hi) // 2
                p = compensate(v["C"], v["table"], v["shift"], mid, D2)[1]
                if p / (v["table"][4] / 100.0) < 1013.25:
                    lo = mid + 1
                else:
                    hi = mid
            good = compensate(v["C"], v["table"], v["shift"], lo, D2)[1]
            bad = compensate(v["C"], cut, v["shift"], lo, D2)[1]
            row.append((bad - good) / (v["table"][4] / 100.0))
        print(f"{name:>8} " + " ".join(f"{e:>9.1f}" for e in row))
    print("\n1 mbar is about 1.02 cm of water.")

    if failures:
        print(f"\n{failures} check(s) failed", file=sys.stderr)
        return 1
    print("\nAll six variants reproduce their datasheet's worked example.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
