// Host harness: run the real MS5803 library against each datasheet's worked
// example.
//
// Every datasheet prints one set of D1 and D2 with the coefficients that
// produced them and the pressure and temperature they must give. Feeding those
// through the library is the strongest check available without a part: it
// exercises begin(), the table selection, the compensation, and the scaling to
// mbar, and its answer is the manufacturer's, not ours.
//
// The datasheets are in docs/. Values read 2026-09-28 from ENG_DS_MS5803-*_B3.
//
//   ./run.sh
//
// Exits non-zero if any variant misses its datasheet's answer.

#include <cstdio>
#include <cmath>
#include <cstddef>
#include "Wire.h"
#include "MS5803.h"

TwoWire Wire;

struct Case {
  const char *name;
  int maxPressure;          // what the caller passes to begin()
  uint16_t C[6];            // C1..C6, the datasheet's typical values
  uint32_t D1, D2;
  double mbar, celsius;     // what that page says the result is
};

// The 05BA prints P = 100002 in its table and then calls it 1100.02 mbar in the
// line beside it. The table value is the one its own page produces, and
// 1100.02 mbar is the 02BA's example pressure carried across, so 1000.02 mbar
// is what this expects.
static const Case CASES[] = {
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312}, 9085466, 8569150, 1000.09, 20.07},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165}, 6465444, 8077636, 1100.02, 20.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266}, 5251266, 8383686, 1000.02, 20.01},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261}, 10332180, 8592698, 5992.09, 34.56},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059}, 4311550, 8387300, 1000.50, 20.15},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305}, 4944364, 8198974, 5999.80, 19.71},
};

// Across temperature, where the second-order terms live. The datasheets publish
// no cold worked example, so these come from compensation_check.py, which
// implements the same flow charts independently; regenerate them with
// `python3 compensation_check.py --emit` only when a datasheet says to. Each D1
// is the one that gives a true 1013.25 mbar at that temperature, so a wrong
// second-order entry shows up as a pressure that is not 1013.25.
static const Case SPAN[] = {
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  9805983,  7089771,    1013.25,  -40.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  9560278,  7529764,    1013.25,  -20.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  9357257,  8015978,    1013.25,    0.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  9155043,  8566784,    1013.25,   20.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  8959194,  9159368,    1013.25,   40.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  8912621,  9307514,    1013.25,   45.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  8903408,  9337143,    1013.25,   46.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  8776469,  9751951,    1013.25,   60.00},
  {"01BA",  1, {40127, 36924, 23317, 23282, 33464, 28312},  8558915, 10492680,    1013.25,   85.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6628043,  6595229,    1013.25,  -40.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6480708,  7036327,    1013.25,  -20.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6371904,  7524185,    1013.25,    0.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6269229,  8077568,    1013.25,   20.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6173348,  8673245,    1013.25,   40.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6150640,  8822164,    1013.25,   45.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6146156,  8851947,    1013.25,   46.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  6085302,  9268921,    1013.25,   60.00},
  {"02BA",  2, {46372, 43981, 29059, 27842, 31553, 28165},  5984873, 10013516,    1013.25,   85.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5525947,  6846538,    1013.25,  -40.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5426978,  7314373,    1013.25,  -19.98},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5347002,  7821736,    1013.25,    0.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5272710,  8383232,    1013.25,   20.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5204521,  8976780,    1013.25,   40.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5188444,  9125167,    1013.25,   45.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5185272,  9154844,    1013.25,   46.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5142336,  9570328,    1013.25,   60.00},
  {"05BA",  5, {32428, 28100, 44250, 20003, 32747, 28266},  5071903, 10312262,    1013.25,   85.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  5009088,  6623301,    1013.25,  -40.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4954480,  7091385,    1013.25,  -20.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4911115,  7598661,    1013.25,    0.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4870862,  8160256,    1013.25,   20.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4833802,  8753909,    1013.25,   40.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4825005,  8902322,    1013.25,   45.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4823267,  8932005,    1013.25,   46.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4799651,  9347562,    1013.25,   60.00},
  {"07BA",  7, {46128, 40903, 27765, 26239, 31876, 28261},  4760557, 10089627,    1013.25,   85.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4335652,  6878435,    1013.30,  -40.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4327489,  7338006,    1013.30,  -20.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4323424,  7834815,    1013.30,    0.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4317351,  8382720,    1013.30,   20.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4310028,  8964978,    1013.30,   40.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4308169,  9112202,    1013.30,   45.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4307797,  9141647,    1013.30,   46.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4302528,  9557628,    1013.30,   60.00},
  {"14BA", 14, {46546, 42845, 29751, 29457, 32745, 29059},  4292901, 10313668,    1013.30,   85.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4047034,  6786920,    1013.30,  -40.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4046999,  7224773,    1013.30,  -20.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4048592,  7694782,    1013.30,    0.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4045709,  8206592,    1013.30,   20.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4039995,  8746272,    1013.30,   40.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4038456,  8882665,    1013.30,   45.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4038143,  8909998,    1013.30,   46.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4033594,  9294526,    1013.30,   60.00},
  {"30BA", 30, {29112, 26814, 19125, 17865, 32057, 31305},  4024692,  9991768,    1013.30,   85.00},
};

static int run(const Case CASES[], size_t n, const char *heading) {
  int failures = 0;
  printf("\n%s\n\n", heading);
  printf("%-8s %12s %12s %10s %10s  %s\n",
         "variant", "mbar wanted", "mbar got", "C wanted", "C got", "verdict");
  for (size_t i = 0; i < n; i++) {
    const Case &c = CASES[i];
    for (int k = 0; k < 6; k++) Wire.prom[k + 1] = c.C[k];
    Wire.prom[0] = 0; Wire.prom[7] = 0;
    Wire.d1 = c.D1; Wire.d2 = c.D2;

    MS5803 sensor(0x77, c.maxPressure);
    if (sensor.begin() != 0) { printf("%-8s  begin() did not acknowledge\n", c.name); failures++; continue; }

    double p = sensor.getPressure(ADC_4096);
    double t = sensor.getTemperature(CELSIUS, ADC_4096);
    bool ok = fabs(p - c.mbar) < 0.005 && fabs(t - c.celsius) < 0.005;
    if (!ok) failures++;
    printf("%-8s %12.2f %12.2f %10.2f %10.2f  %s\n",
           c.name, c.mbar, p, c.celsius, t, ok ? "ok" : "FAIL");
  }
  return failures;
}

// A bar figure this library does not know must be refused, not given the 05BA's
// constants: that used to return a confidently wrong pressure and raise nothing.
static int unknownModelRefused() {
  printf("\nA bar figure the library does not know:\n\n");
  const Case &c = CASES[1];   // any worked example: its own bar figure is the control
  for (int k = 0; k < 6; k++) Wire.prom[k + 1] = c.C[k];
  Wire.prom[0] = 0; Wire.prom[7] = 0;
  Wire.d1 = c.D1; Wire.d2 = c.D2;

  MS5803 sensor(0x77, 99);   // 99 bar: no such part here
  if (sensor.begin() != 0) {
    printf("  begin() did not acknowledge\n");
    return 1;
  }
  int failures = 0;
  bool known = sensor.modelKnown();
  double p = sensor.getPressure(ADC_4096);
  double t = sensor.getTemperature(CELSIUS, ADC_4096);
  // Each getter keeps one meaning: a pressure, or nothing. The reading is not
  // lost, because the raw conversions have their own accessors, which is how
  // the Walrus reaches them.
  bool pRaw = (sensor.getPressureADC() == c.D1);
  bool tRaw = (sensor.getTemperatureADC() == c.D2);
  printf("  modelKnown()        = %-5s   %s\n", known ? "true" : "false", known ? "FAIL" : "ok");
  printf("  getPressure()       = %-10f   %s\n", p, std::isnan(p) ? "ok" : "FAIL");
  printf("  getTemperature()    = %-10f   %s\n", t, std::isnan(t) ? "ok" : "FAIL");
  printf("  getPressureADC()    = %-10lu   %s\n", (unsigned long)sensor.getPressureADC(), pRaw ? "ok" : "FAIL");
  printf("  getTemperatureADC() = %-10lu   %s\n", (unsigned long)sensor.getTemperatureADC(), tRaw ? "ok" : "FAIL");
  if (known) failures++;
  if (!std::isnan(p)) failures++;
  if (!std::isnan(t)) failures++;
  if (!pRaw) failures++;
  if (!tRaw) failures++;
  // The same readings through the part's own bar figure still convert, which
  // shows the refusal is aimed at the unknown figure and not at everything.
  MS5803 good(0x77, c.maxPressure);
  good.begin();
  double gp = good.getPressure(ADC_4096);
  bool ok = fabs(gp - c.mbar) < 0.005;
  printf("  %s still reads %.2f mbar (wanted %.2f)   %s\n", c.name, gp, c.mbar, ok ? "ok" : "FAIL");
  if (!ok) failures++;
  return failures;
}

// Constructing without naming a part must not assume one either: the default
// used to be the 02BA, which is the same silent guess one level up.
static int undeclaredModelRefused() {
  printf("\nA part nobody declared:\n\n");
  const Case &c = CASES[1];
  for (int k = 0; k < 6; k++) Wire.prom[k + 1] = c.C[k];
  Wire.prom[0] = 0; Wire.prom[7] = 0;
  Wire.d1 = c.D1; Wire.d2 = c.D2;

  MS5803 sensor;              // no bar figure given
  sensor.begin();
  int failures = 0;
  bool known = sensor.modelKnown();
  double p = sensor.getPressure(ADC_4096);
  printf("  modelKnown()        = %-5s   %s\n", known ? "true" : "false", known ? "FAIL" : "ok");
  printf("  getPressure()       = %-10f   %s\n", p, std::isnan(p) ? "ok" : "FAIL");
  printf("  getPressureADC()    = %-10lu   %s\n", (unsigned long)sensor.getPressureADC(),
         sensor.getPressureADC() == c.D1 ? "ok" : "FAIL");
  if (known) failures++;
  if (!std::isnan(p)) failures++;
  if (sensor.getPressureADC() != c.D1) failures++;
  return failures;
}

int main() {
  int failures = 0;
  failures += run(CASES, sizeof(CASES) / sizeof(CASES[0]),
                  "Worked example from each datasheet:");
  failures += run(SPAN, sizeof(SPAN) / sizeof(SPAN[0]),
                  "Across temperature, against the flow charts:");
  failures += unknownModelRefused();
  failures += undeclaredModelRefused();
  if (failures) { printf("\n%d case(s) missed the expected answer\n", failures); return 1; }
  printf("\nEvery case matches.\n");
  return 0;
}
