// A scripted MS5803 on a stub I2C bus, for the host harness.
//
// The part is command-response, not a register file: a one-byte command is
// written, and the read that follows returns whatever that command set up. The
// three commands the library sends are PROM read (0xA0 + 2n, two bytes MSB
// first), convert (0x40 or 0x50 plus the oversampling bits, no read), and ADC
// read (0x00, three bytes MSB first, returning the value the last convert
// selected).
//
// Set prom[0..7], d1 and d2 before calling begin(). Word 0 is the
// manufacturer's reserved word and word 7 the CRC; neither is used by the
// library, and the datasheets' worked examples do not print them.
#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>

class TwoWire {
  public:
  uint16_t prom[8] = {0};
  uint32_t d1 = 0;            // digital pressure value
  uint32_t d2 = 0;            // digital temperature value
  uint8_t deviceAddress = 0x77;
  bool present = true;

  void begin() {}
  void beginTransmission(uint8_t adr) { _adr = adr; _nwrites = 0; }
  size_t write(uint8_t v) { if (_nwrites++ == 0) _command = v; return 1; }
  uint8_t endTransmission(bool = true) {
    // A convert command latches which value the next ADC read returns: 0x40 is
    // D1 and 0x50 is D2, with the low bits selecting the oversampling ratio.
    if (_nwrites == 1) {
      if ((_command & 0xF0) == 0x40) _pending = d1;
      else if ((_command & 0xF0) == 0x50) _pending = d2;
    }
    return (present && _adr == deviceAddress) ? 0 : 2;
  }

  uint8_t requestFrom(uint8_t adr, uint8_t n) {
    _adr = adr;
    if (!present || adr != deviceAddress) return 0;
    if (_command >= 0xA0 && _command <= 0xAE && !(_command & 1)) {
      uint16_t w = prom[(_command - 0xA0) / 2];
      _q.push_back(w >> 8); _q.push_back(w & 0xFF);
    } else if (_command == 0x00) {
      uint32_t v = _pending;
      _q.push_back(v >> 16); _q.push_back((v >> 8) & 0xFF); _q.push_back(v & 0xFF);
    }
    return n;
  }
  uint8_t requestFrom(int adr, int n) { return requestFrom((uint8_t)adr, (uint8_t)n); }
  int read() { if (_q.empty()) return -1; int v = _q.front(); _q.pop_front(); return v; }
  int available() { return (int)_q.size(); }

  private:
  uint8_t _adr = 0, _command = 0; unsigned _nwrites = 0;
  uint32_t _pending = 0; std::deque<uint8_t> _q;
};
extern TwoWire Wire;
