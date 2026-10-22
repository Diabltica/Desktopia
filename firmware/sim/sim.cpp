// Host-side simulation bench for the Desktopia firmwares.
// Both boards run in the same process, from their real sources:
//   motherboard (ATmega32U4): buttons, hmi_link, motor, presets, tof_sensor, current_monitor, desk_controller, watchdog
//   HMI board (RP2040):        hmi_app, display, gc9a01, ui_text
//   both:                      shared/hmi_link/hmi_protocol
// against mocks of the Arduino cores, a simulated desk, a noisy ToF sensor, the switched driver supply (Q5), an INA226
// with its ALERT line on INT6, an RS-485 bus (cord plugged / unplugged, data pair cut, A/B swapped by a reversed cord,
// corrupted bytes) and a GC9A01 emulator that rebuilds the screen pixel by pixel. The two cores of the RP2040 are run
// one after the other at every simulated millisecond.
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <vector>
#include <deque>
#include <memory>
#include <string>
#include <cmath>
#include <cstdarg>
#include <random>
#include <algorithm>
#include "board_pins.h"
#include "config.h"
#include "buttons.h"
#include "current_monitor.h"
#include "hmi_link.h"
#include "motor.h"
#include "presets.h"
#include "tof_sensor.h"
#include "desk_controller.h"
#include "watchdog.h"
#include "hmi_app.h"
#include "hmi_config.h"
#include "hmi_hw.h"
#include "hmi_pins.h"
#include <avr/wdt.h>

// ------------------------------------------------------------------------------------------------
//  Mocked Arduino / AVR / RP2040 world
// ------------------------------------------------------------------------------------------------
static uint32_t g_now = 0;
static bool g_delayRunsClock = true;                // false while the HMI board boots in the background
static uint8_t g_pinLevel[64];
static uint8_t g_pinMode[64];
volatile uint8_t TCCR1A, TCCR1B, PORTB, PORTD;
volatile uint16_t ICR1, OCR1B;
static void (*g_isr[8])() = {nullptr};             // attachInterrupt() table, by interrupt number
static int g_isrMode[8];
static unsigned g_supplyCutUnderLoad = 0;           // driver supply switched off by the loop with the PWM running
SPIClass SPI;
TwoWire Wire;
EEPROMClass EEPROM;
SerialMock Serial;
SimTof* g_simTof = nullptr;
volatile uint8_t WDTCSR, MCUSR;
unsigned g_wdrCount = 0;

unsigned long millis() { return g_now; }
void delay(unsigned long ms) { if (g_delayRunsClock) g_now += (uint32_t)ms; }
void delayMicroseconds(unsigned int) {}
void pinMode(uint8_t p, uint8_t m) { g_pinMode[p] = m; }
void attachInterrupt(uint8_t n, void (*f)(), int mode) { if (n < 8) { g_isr[n] = f; g_isrMode[n] = mode; } }

static double pwmDuty() { return ((TCCR1A & _BV(COM1B1)) && ICR1) ? (double)OCR1B / ICR1 : 0.0; }
static bool supplySwitchOn() { return PORTD & _BV(pins::MOTOR_POWER_PORT_BIT); }   // Q5 gate driven (MOTOR_PWR high)

// I_ALERT (PE6 / INT6): open drain, pulled up by R25. A falling edge runs the handler attached to INT6.
static void setAlertLine(bool low) {
  uint8_t old = g_pinLevel[pins::I_ALERT];
  g_pinLevel[pins::I_ALERT] = low ? LOW : HIGH;
  int n = digitalPinToInterrupt(pins::I_ALERT);
  if (old == HIGH && low && n >= 0 && g_isr[n] && (g_isrMode[n] == FALLING || g_isrMode[n] == CHANGE)) g_isr[n]();
}

struct Lcd;
static Lcd* g_lcd = nullptr;
static void lcdCsEdge();
static void lcdResetLine(bool high);

void digitalWrite(uint8_t p, uint8_t v) {
  bool old = g_pinLevel[p];
  g_pinLevel[p] = v;
  if (p == pins::MOTOR_PWM) {
    if (v) PORTB |= _BV(PORTB6); else PORTB &= (uint8_t)~_BV(PORTB6);
  }
  if (p == pins::MOTOR_POWER) {
    if (v) PORTD |= _BV(pins::MOTOR_POWER_PORT_BIT);
    else {
      if (supplySwitchOn() && pwmDuty() > 0) g_supplyCutUnderLoad++;
      PORTD &= (uint8_t)~_BV(pins::MOTOR_POWER_PORT_BIT);
    }
  }
  if (p == hmipins::LCD_CS && v && !old) lcdCsEdge();
  if (p == hmipins::LCD_RST) lcdResetLine(v != 0);
}
int digitalRead(uint8_t p) { return g_pinLevel[p]; }

// The two firmwares share the pin array: their pins must not overlap.
static bool pinsDisjoint() {
  const uint8_t mb[] = {pins::MOTOR_DIR, pins::MOTOR_PWM, pins::MOTOR_POWER, pins::I_ALERT, pins::RS485_DE};
  const uint8_t hmiPins[] = {hmipins::LINK_DE, hmipins::BTN_UP, hmipins::BTN_DOWN, hmipins::BTN_PRESET1, hmipins::BTN_PRESET2,
                             hmipins::BTN_PRESET3, hmipins::LCD_CS, hmipins::LCD_DC, hmipins::LCD_RST};
  for (uint8_t a : mb) for (uint8_t b : hmiPins) if (a == b) return false;
  return true;
}

// ------------------------------------------------------------------------------------------------
//  GC9A01 emulator (on the HMI board, powered through the cord)
// ------------------------------------------------------------------------------------------------
struct Lcd {
  uint16_t fb[240 * 240];
  uint8_t written[240 * 240];                       // pixel written since the panel was powered or hardware-reset
  uint8_t cmd = 0;
  std::vector<uint8_t> args;
  uint16_t x0 = 0, x1 = 239, y0 = 0, y1 = 239, cx = 0, cy = 0;
  bool inRamwr = false, haveHi = false;
  uint8_t hi = 0;
  uint32_t pixelsInWindow = 0, expectedPixels = 0;
  bool powered = true, inReset = false;
  // statistics
  uint64_t bytes = 0, windows = 0;
  unsigned csViolations = 0, windowMismatch = 0, initCmds = 0, hwResets = 0, swResets = 0;
  uint8_t madctl = 0;
  bool displayOn = false, sleepOut = false;
  Lcd() { powerUp(); }

  void restart(bool memoryLost) {                   // power-up, hardware reset (memoryLost) or SWRESET
    cmd = 0; args.clear(); inRamwr = false; haveHi = false;
    x0 = 0; x1 = 239; y0 = 0; y1 = 239;
    madctl = 0; displayOn = sleepOut = false;
    if (memoryLost) memset(written, 0, sizeof(written));
  }
  void powerUp() {
    for (int i = 0; i < 240 * 240; i++) fb[i] = 0xF81F;   // random-looking content: magenta shows what was never redrawn
    powered = true; inReset = false;
    restart(true);
  }
  void powerDown() { powered = false; displayOn = sleepOut = false; }
  void resetLine(bool high) {
    if (!powered) return;
    if (!high) { inReset = true; return; }
    if (inReset) { inReset = false; hwResets++; restart(true); }
  }
  // Fraction of the visible disc written since the panel was powered or hardware-reset.
  double coverage() const {
    unsigned in = 0, done = 0;
    for (int y = 0; y < 240; y++) for (int x = 0; x < 240; x++) {
      double dx = x - 119.5, dy = y - 119.5;
      if (dx * dx + dy * dy > 120.0 * 120.0) continue;
      in++; if (written[y * 240 + x]) done++;
    }
    return (double)done / in;
  }
  bool faultDiscShown() const { return fb[52 * 240 + 110] == 0xF800; }   // red warning disc of the Fault screen

  void closeWindowCheck() {
    if (inRamwr && pixelsInWindow != expectedPixels) {
      windowMismatch++;
      printf("  [LCD] window mismatch: wrote %u pixels, window holds %u\n", pixelsInWindow, expectedPixels);
    }
    inRamwr = false;
  }
  void byte(uint8_t b) {
    bytes++;
    if (!powered || inReset) return;
    if (g_pinLevel[hmipins::LCD_CS] != LOW) { csViolations++; return; }
    if (g_pinLevel[hmipins::LCD_DC] == LOW) {          // command
      closeWindowCheck();
      cmd = b;
      args.clear();
      initCmds++;
      if (b == 0x2C) {
        inRamwr = true; haveHi = false; cx = x0; cy = y0; pixelsInWindow = 0;
        expectedPixels = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
        windows++;
      }
      if (b == 0x01) { swResets++; restart(false); }
      if (b == 0x11) sleepOut = true;
      if (b == 0x29) displayOn = true;
      return;
    }
    if (inRamwr) {
      if (!haveHi) { hi = b; haveHi = true; return; }
      haveHi = false;
      fb[cy * 240 + cx] = (uint16_t)((hi << 8) | b);
      written[cy * 240 + cx] = 1;
      pixelsInWindow++;
      if (++cx > x1) { cx = x0; if (++cy > y1) cy = y0; }
      return;
    }
    args.push_back(b);
    if (cmd == 0x2A && args.size() == 4) { x0 = (uint16_t)((args[0] << 8) | args[1]); x1 = (uint16_t)((args[2] << 8) | args[3]); }
    if (cmd == 0x2B && args.size() == 4) { y0 = (uint16_t)((args[0] << 8) | args[1]); y1 = (uint16_t)((args[2] << 8) | args[3]); }
    if (cmd == 0x36) madctl = b;
  }
  void savePpm(const char* path) const {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n240 240\n255\n");
    for (int y = 0; y < 240; y++) for (int x = 0; x < 240; x++) {
      uint16_t c = fb[y * 240 + x];
      uint8_t r = (uint8_t)(((c >> 11) & 31) * 255 / 31), g = (uint8_t)(((c >> 5) & 63) * 255 / 63), b = (uint8_t)((c & 31) * 255 / 31);
      double dx = x - 119.5, dy = y - 119.5;
      if (dx * dx + dy * dy > 120.0 * 120.0) { r = g = b = 40; }     // outside the round panel
      fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
  }
};
static void lcdCsEdge() { if (g_lcd) g_lcd->closeWindowCheck(); }
static void lcdResetLine(bool high) { if (g_lcd) g_lcd->resetLine(high); }
void simSpiByte(uint8_t b) { if (g_lcd) g_lcd->byte(b); }

// ------------------------------------------------------------------------------------------------
//  RS-485 bus between the two transceivers (SP485EEN / SP3485EN) + the RJ11 cord
//  A byte reaches the other side only if the sender's transceiver drives the bus (DE high), the HMI board is
//  powered (the cord is in) and the data pair is intact. With A and B swapped relative to the HMI board's UART
//  polarity, the receiver gets garbage.
// ------------------------------------------------------------------------------------------------
struct Bus {
  bool cordIn = true;                 // power + data
  bool dataCut = false;               // power OK, A/B open
  bool swapped = false;               // reversed telephone cord: A and B exchanged
  double byteErrorRate = 0.0;         // probability that a byte arrives with one bit flipped
  bool hmiInverted = false;           // what hmiHwSetLinkInverted() last set
  std::mt19937 rng{4242};
  unsigned collisions = 0, garbled = 0, corrupted = 0;
  double uni() { std::uniform_real_distribution<double> d(0.0, 1.0); return d(rng); }
} g_bus;

void hmiHwSetLinkInverted(bool inverted) { g_bus.hmiInverted = inverted; }
void hmiHwLock() {}
void hmiHwUnlock() {}

struct SimUart : public Stream {
  std::deque<uint8_t> rx;
  SimUart* peer = nullptr;
  uint8_t dePin;
  bool hmiSide;
  unsigned writtenWithoutDe = 0;
  uint64_t txBytes = 0;
  SimUart(uint8_t de, bool hmi) : dePin(de), hmiSide(hmi) {}

  int available() override { return (int)rx.size(); }
  int read() override { if (rx.empty()) return -1; int b = rx.front(); rx.pop_front(); return b; }
  int peek() override { return rx.empty() ? -1 : rx.front(); }
  size_t write(uint8_t b) override {
    txBytes++;
    if (!g_pinLevel[dePin]) { writtenWithoutDe++; return 1; }          // transceiver not driving: the byte is lost
    if (g_pinLevel[peer->dePin] && g_bus.cordIn) g_bus.collisions++;   // both drive the bus at the same time
    if (!g_bus.cordIn || g_bus.dataCut) return 1;
    uint8_t out = b;
    if (g_bus.swapped != g_bus.hmiInverted) { out = (uint8_t)g_bus.rng(); g_bus.garbled++; }
    if (g_bus.byteErrorRate > 0 && g_bus.uni() < g_bus.byteErrorRate) { out ^= (uint8_t)(1u << (g_bus.rng() & 7)); g_bus.corrupted++; }
    peer->rx.push_back(out);
    return 1;
  }
};

// ------------------------------------------------------------------------------------------------
//  INA226 (U6) across the 2 mOhm shunt R23: averaged conversions, current register, shunt over-voltage alert
//  (latched or not), Mask / Enable read clearing the latch, reset to the power-on defaults.
// ------------------------------------------------------------------------------------------------
struct SimIna {
  bool present = true;
  uint16_t config = 0x4127, cal = 0, mask = 0, limit = 0;
  uint8_t ptr = 0;
  bool aff = false;                                  // alert function flag
  int16_t shunt = 0, current = 0;
  double sumA = 0;
  uint32_t sumMs = 0, convUs = 0;

  void powerOnReset() {
    config = 0x4127; cal = 0; mask = 0; limit = 0; aff = false; shunt = current = 0; sumA = 0; sumMs = 0; convUs = 0;
    setAlertLine(false);
  }
  uint32_t periodUs() const {                        // averages x (bus + shunt conversion times)
    static const uint16_t kCt[8] = {140, 204, 332, 588, 1100, 2116, 4156, 8244};
    static const uint16_t kAvg[8] = {1, 4, 16, 64, 128, 256, 512, 1024};
    return (uint32_t)kAvg[(config >> 9) & 7] * (kCt[(config >> 6) & 7] + kCt[(config >> 3) & 7]);
  }
  bool alertActive() const { return aff && (mask & 0xFC00); }   // APOL = 0: active low
  void step1ms(double amps) {
    if (!present || (config & 7) != 7) return;       // only the continuous mode is modelled
    sumA += amps; sumMs++; convUs += 1000;
    if (convUs < periodUs()) return;
    convUs -= periodUs();
    double v = std::lround(sumA / sumMs * 0.002 / 2.5e-6);    // 2 mOhm, 2.5 uV steps
    sumA = 0; sumMs = 0;
    shunt = (int16_t)std::max(-32768.0, std::min(32767.0, v));
    current = (int16_t)((int32_t)shunt * cal / 2048);
    bool over = (mask & 0x8000) && shunt > (int16_t)limit;
    if (mask & 0x0001) { if (over) aff = true; }     // LEN: stays set until Mask / Enable is read
    else aff = over;
    setAlertLine(alertActive());                     // a falling edge runs the INT6 handler right here
  }
  uint16_t read() {
    switch (ptr) {
      case 0x00: return config;
      case 0x01: return (uint16_t)shunt;
      case 0x02: return 19200;                       // 24 V bus, 1.25 mV steps
      case 0x04: return (uint16_t)current;
      case 0x05: return cal;
      case 0x06: {
        uint16_t v = (uint16_t)(mask | (aff ? 0x0010 : 0) | 0x0008);
        if (mask & 0x0001) { aff = false; setAlertLine(false); }
        return v;
      }
      case 0x07: return limit;
      case 0xFE: return 0x5449;
      case 0xFF: return 0x2260;
    }
    return 0;
  }
  void write(uint8_t reg, uint16_t v) {
    switch (reg) {
      case 0x00: if (v & 0x8000) powerOnReset(); else config = v; break;
      case 0x05: cal = v & 0x7FFF; break;
      case 0x06: mask = v & 0xFC03; setAlertLine(alertActive()); break;
      case 0x07: limit = v; break;
    }
  }
} g_ina;

// What the firmware is expected to program (4 averages of 1.1 ms conversions, 1.25 mA current LSB, SOL latched)
static const uint16_t kInaConfig = 0x4327;
static const uint16_t kInaCal = (uint16_t)(5120000000ULL / (1250ULL * INA226_SHUNT_MICROOHM));
static const uint16_t kInaLimit = (uint16_t)((uint32_t)MOTOR_CURRENT_LIMIT_MA * INA226_SHUNT_MICROOHM / 2500UL);
static const uint16_t kInaMask = 0x8001;

// ------------------------------------------------------------------------------------------------
//  I2C bus: VL53L1CB (its registers go through the mocked ST driver below) and INA226
// ------------------------------------------------------------------------------------------------
static bool g_tofPresent = true;

void TwoWire::beginTransmission(uint8_t a) { _addr = a; _txLen = 0; }
size_t TwoWire::write(uint8_t b) { if (_txLen < 8) _tx[_txLen++] = b; return 1; }
uint8_t TwoWire::endTransmission(uint8_t) {
  if (_addr == ((TOF_I2C_ADDR_8BIT >> 1) & 0x7F)) return g_tofPresent ? 0 : 2;
  if (_addr == INA226_I2C_ADDR) {
    if (!g_ina.present) return 2;
    if (_txLen >= 1) g_ina.ptr = _tx[0];
    if (_txLen == 3) g_ina.write(_tx[0], (uint16_t)((_tx[1] << 8) | _tx[2]));
    return 0;
  }
  return 2;
}
uint8_t TwoWire::requestFrom(uint8_t a, uint8_t n) {
  _rxLen = 0; _rxPos = 0;
  if (a == INA226_I2C_ADDR && g_ina.present) {
    uint16_t v = g_ina.read();
    _rx[0] = (uint8_t)(v >> 8); _rx[1] = (uint8_t)v;
    _rxLen = n < 2 ? n : 2;
  }
  return _rxLen;
}
int TwoWire::read() { return _rxPos < _rxLen ? _rx[_rxPos++] : -1; }
int TwoWire::available() { return _rxLen - _rxPos; }

struct SimTof {
  bool ready = false;
  uint16_t distance = 0;
  uint8_t status = 0;
  unsigned violations = 0, calls = 0;
} g_tof;
int8_t simTofCall(const char* what) {
  g_tof.calls++;
  if (!g_tofPresent) { g_tof.violations++; printf("  [VIOLATION] ST driver entered while the sensor does not ACK (%s)\n", what); return -1; }
  return 0;
}
void simTofReady(uint8_t* r) { *r = g_tof.ready ? 1 : 0; }
void simTofStatus(uint8_t* s) { *s = g_tof.status; }
void simTofDistance(uint16_t* d) { *d = g_tof.distance; }
void simTofClear() { g_tof.ready = false; }

// ------------------------------------------------------------------------------------------------
//  Physical world: desk + sensor + buttons
//  The desk is derived from config.h, so the scenarios stay meaningful when the settings are tuned:
//  full speed at MOTOR_PWM_MAX_PERCENT, a crawl (1/6 of it) at MOTOR_PWM_MIN_PERCENT, mechanical
//  end stops just outside HEIGHT_MIN_MM / HEIGHT_MAX_MM.
// ------------------------------------------------------------------------------------------------
static const double kMaxDuty = MOTOR_PWM_MAX_PERCENT / 100.0;
static const double kMinDuty = MOTOR_PWM_MIN_PERCENT / 100.0;
static const uint32_t kQ5RiseMs = 15;               // Q5 soft start (C21 Miller capacitor, R21 / R20): driver supply up
static const uint8_t kButtonPin[5] = {hmipins::BTN_UP, hmipins::BTN_DOWN, hmipins::BTN_PRESET1, hmipins::BTN_PRESET2, hmipins::BTN_PRESET3};

struct World {
  double h = 900.0;                  // true desk height in mm
  double mechMin = HEIGHT_MIN_MM - 10, mechMax = HEIGHT_MAX_MM + 15;
  double polarity = +1.0;            // -1 = motor wired the other way
  bool jam = false;
  double vMaxMmS = 28.0;             // speed at MOTOR_PWM_MAX_PERCENT
  double dutyThreshold = std::max(0.0, (6 * kMinDuty - kMaxDuty) / 5);   // below this the desk does not move
  double noiseSigma = 1.2;
  double outlierProb = 0.02, invalidProb = 0.02;
  uint32_t nextSample = 0;
  uint8_t targetBits = 0;
  uint32_t bounceUntil[5] = {0};
  std::mt19937 rng{12345};
  unsigned pwmStuckHigh = 0;
  double maxDuty = 0;
  // driver supply (Q5) and motor current, seen by the INA226 on the 24 V side
  double runA = 4.0;                 // supply current at MOTOR_PWM_MAX_PERCENT, normal load
  double jamA = 4.0;                 // the same with the desk jammed (default: the drive slips, no extra current)
  double amps = 0;
  bool supplyWasOn = false;
  uint32_t supplyOnAt = 0;
  unsigned pwmWithoutSupply = 0;

  double duty() const { return pwmDuty(); }
  bool supplyReady() const { return supplySwitchOn() && g_now - supplyOnAt >= kQ5RiseMs; }
  bool dirUp() const { return g_pinLevel[pins::MOTOR_DIR] == MOTOR_DIR_UP_LEVEL; }
  double gauss() { std::normal_distribution<double> d(0.0, 1.0); return d(rng); }
  double uni() { std::uniform_real_distribution<double> d(0.0, 1.0); return d(rng); }

  void setButton(int bit, bool pressed, uint32_t bounceMs = 3) {
    if (pressed) targetBits |= (uint8_t)(1 << bit); else targetBits &= (uint8_t)~(1 << bit);
    bounceUntil[bit] = g_now + bounceMs;
  }
  void stepOneMs() {
    // buttons with contact bounce: switch to GND, RP2040 pull-ups
    for (int b = 0; b < 5; b++) {
      bool v = (targetBits >> b) & 1;
      if ((int32_t)(g_now - bounceUntil[b]) < 0) v = uni() < 0.5;
      g_pinLevel[kButtonPin[b]] = v ? LOW : HIGH;
    }
    // driver supply -> motor -> desk
    bool on = supplySwitchOn();
    if (on && !supplyWasOn) supplyOnAt = g_now;
    supplyWasOn = on;
    double d = duty();
    if (d > maxDuty) maxDuty = d;
    if (!(TCCR1A & _BV(COM1B1)) && (PORTB & _BV(PORTB6))) pwmStuckHigh++;   // output disconnected but latch high = full speed!
    if (d > 0 && !supplyReady()) pwmWithoutSupply++;                      // PWM on the driver input, supply off or rising
    double v = 0;
    if (supplyReady() && d > dutyThreshold) v = vMaxMmS * (d - dutyThreshold) / (kMaxDuty - dutyThreshold);
    if (jam) v = 0;
    amps = supplyReady() ? d / kMaxDuty * (jam ? jamA : runA) : 0.0;
    g_ina.step1ms(amps);                                                  // may run the INT6 handler before the next loop
    h += (dirUp() ? 1.0 : -1.0) * polarity * v * 0.001;
    if (h < mechMin) h = mechMin;
    if (h > mechMax) h = mechMax;
    // ToF: a new sample every 37 ms (33 ms budget + 4 ms), noisy, sometimes invalid or an outlier
    if (g_tofPresent && (int32_t)(g_now - nextSample) >= 0) {
      double dist = (h - TOF_HEIGHT_OFFSET_MM) / (double)TOF_HEIGHT_SIGN + gauss() * noiseSigma;
      if (uni() < outlierProb) dist += (uni() < 0.5 ? -1 : 1) * (30 + 60 * uni());
      g_tof.distance = (uint16_t)std::lround(dist);
      g_tof.status = (uni() < invalidProb) ? 4 : 0;
      g_tof.ready = true;
      nextSample = g_now + 37;
    }
  }
};

// ------------------------------------------------------------------------------------------------
//  Test rig
// ------------------------------------------------------------------------------------------------
static int g_checks = 0, g_fails = 0;
static double g_worstFrameMs = 0; static uint64_t g_worstFrameBytes = 0; static const char* g_worstFrameScreen = "";
static void check(bool ok, const char* fmt, ...) {
  g_checks++;
  if (!ok) {
    g_fails++;
    va_list ap; va_start(ap, fmt);
    printf("  FAIL: "); vprintf(fmt, ap); printf("\n");
    va_end(ap);
  }
}

static const char* screenName(UiModel::Screen s) {
  switch (s) { case UiModel::Screen::Boot: return "Boot"; case UiModel::Screen::Main: return "Main"; case UiModel::Screen::SaveGauge: return "SaveGauge";
               case UiModel::Screen::SaveSuccess: return "SaveSuccess"; case UiModel::Screen::Notice: return "Notice"; case UiModel::Screen::Fault: return "Fault";
               case UiModel::Screen::Count: break; }
  return "?";
}
struct Sample { uint32_t t; double h; double tofH; double duty; bool dirUp; UiModel::Screen screen; UiModel::Arrow arrow; uint8_t gauge; bool linkUp;
                bool supply; double amps; };

// The HMI board takes ~0.7 s to bring its panel up (core 1); the link (core 0) runs meanwhile.
static const uint32_t kPanelInitMs = 700;

struct Rig {
  World w;
  Lcd lcd;
  // motherboard
  SimUart mbUart{pins::RS485_DE, false};
  HmiLink link{mbUart, pins::RS485_DE};
  Buttons buttons;
  TofSensor tof;
  Motor motor;
  Presets presets;
  CurrentMonitor current;
  DeskController ctrl{link, buttons, tof, motor, presets, current};
  // HMI board
  SimUart hmiUart{hmipins::LINK_DE, true};
  std::unique_ptr<HmiApp> hmi;
  uint32_t panelReadyAt = 0;
  unsigned hmiBoots = 0;

  std::vector<Sample> trace;
  std::vector<std::string> shotNames;
  unsigned dirChangesWhileMoving = 0;
  uint8_t lastDirLevel = 0;
  uint32_t loopStepMs = 1;

  Rig(double startHeight = 900.0, bool keepEeprom = false, uint32_t seed = 12345, uint32_t startMs = 1000, bool cordAtBoot = true,
      bool inaAtBoot = true) {
    memset(g_pinLevel, 0, sizeof(g_pinLevel));
    memset(g_pinMode, 0, sizeof(g_pinMode));
    for (uint8_t p : kButtonPin) g_pinLevel[p] = HIGH;
    g_pinLevel[pins::I_ALERT] = HIGH;                // R25 pull-up
    TCCR1A = TCCR1B = PORTB = PORTD = 0; ICR1 = OCR1B = 0;
    memset(g_isr, 0, sizeof(g_isr));
    g_supplyCutUnderLoad = 0;
    g_ina = SimIna(); g_ina.present = inaAtBoot;
    g_tof = SimTof(); g_tofPresent = true;
    uint32_t busSeed = seed * 7 + 1;
    g_bus = Bus(); g_bus.rng.seed(busSeed);
    if (!keepEeprom) { memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem)); EEPROM.writes = 0; }
    g_now = startMs;
    g_lcd = &lcd;
    mbUart.peer = &hmiUart; hmiUart.peer = &mbUart;
    w.h = startHeight;
    w.rng.seed(seed);
    // both boards power up together: the HMI board's panel init runs while the motherboard boots
    if (cordAtBoot) hmiPowerUp(); else { g_bus.cordIn = false; lcd.powerDown(); }
    // same order as main.cpp setup()
    motor.begin();
    link.begin();
    presets.begin();
    current.begin();
    // the physical world already exists while the firmware boots: let the sensor produce a first sample
    w.nextSample = g_now;
    w.stepOneMs();
    tof.begin();
    ctrl.begin(g_now, false);
    lastDirLevel = g_pinLevel[pins::MOTOR_DIR];
  }
  bool expectSensorViolations = false;
  ~Rig() {
    check(lcd.csViolations == 0, "%u SPI bytes were sent while CS was high", lcd.csViolations);
    check(lcd.windowMismatch == 0, "%u pixel windows were not filled exactly", lcd.windowMismatch);
    check(expectSensorViolations || g_tof.violations == 0, "ST driver entered %u times while the sensor did not ACK", g_tof.violations);
    check(w.pwmStuckHigh == 0, "PWM pin latched high while disconnected for %u ms", w.pwmStuckHigh);
    check(w.maxDuty <= kMaxDuty + 0.0005, "duty reached %.3f", w.maxDuty);
    check(w.pwmWithoutSupply == 0, "PWM sent to the driver for %u ms while its supply was off or still rising", w.pwmWithoutSupply);
    check(g_supplyCutUnderLoad == 0, "driver supply switched off %u times with the PWM running", g_supplyCutUnderLoad);
    check(mbUart.writtenWithoutDe == 0 && hmiUart.writtenWithoutDe == 0, "bytes written with the transceiver not driving (MB %u, HMI %u)", mbUart.writtenWithoutDe, hmiUart.writtenWithoutDe);
    check(g_bus.collisions == 0, "%u bytes sent while the other side was driving the bus", g_bus.collisions);
    g_lcd = nullptr;
  }

  // HMI board power: the cord carries its supply.
  void hmiPowerUp() {
    g_bus.cordIn = true;
    lcd.powerUp();
    hmiUart.rx.clear();
    hmi.reset(new HmiApp(hmiUart));
    hmi->beginLink(g_now);
    g_delayRunsClock = false;                        // core 1 runs the panel init alongside core 0 and the motherboard
    hmi->beginDisplay();
    g_delayRunsClock = true;
    panelReadyAt = g_now + kPanelInitMs;
    hmiBoots++;
  }
  void hmiPowerDown() {
    g_bus.cordIn = false;
    hmi.reset();
    lcd.powerDown();
    g_pinLevel[hmipins::LINK_DE] = LOW;              // unpowered transceiver
    hmiUart.rx.clear();
  }
  void plug(bool in) { if (in && !hmi) hmiPowerUp(); else if (!in && hmi) hmiPowerDown(); }
  bool hmiLinkUp() const { return hmi && hmi->linkUp(g_now); }

  void step() {
    g_now += 1;
    w.stepOneMs();
    uint8_t before = lastDirLevel;
    double dutyBefore = w.duty();                        // what the bridge was driving when DIR may change below
    if ((g_now % loopStepMs) == 0) {
      ctrl.update(g_now);
      link.update(ctrl.ui(), g_now);
    }
    if (hmi) {
      hmi->updateLink(g_now);                            // core 0
      if ((int32_t)(g_now - panelReadyAt) >= 0) {        // core 1
        uint64_t b0 = lcd.bytes, w0 = lcd.windows;
        hmi->updateDisplay(g_now);
        uint64_t db = lcd.bytes - b0, dw = lcd.windows - w0;
        double estMs = (db * 8.0 / (DISPLAY_SPI_HZ / 1e6) + dw * 5.0) / 1000.0;   // SPI time + a few us per window
        if (estMs > g_worstFrameMs) { g_worstFrameMs = estMs; g_worstFrameBytes = db; g_worstFrameScreen = screenName(ctrl.ui().screen); }
      }
    }
    if (g_pinLevel[pins::MOTOR_DIR] != before) {
      if (dutyBefore > 0.0) dirChangesWhileMoving++;
      lastDirLevel = g_pinLevel[pins::MOTOR_DIR];
    }
    const UiModel& u = ctrl.ui();
    trace.push_back({g_now, w.h, (double)tof.heightMm(), w.duty(), w.dirUp(), u.screen, u.arrow, u.gaugePercent, link.connected(),
                     supplySwitchOn(), w.amps});
  }
  void run(uint32_t ms) { for (uint32_t i = 0; i < ms; i++) step(); }
  void press(int bit) { w.setButton(bit, true); }
  void release(int bit) { w.setButton(bit, false); }
  void tap(int bit, uint32_t ms) { press(bit); run(ms); release(bit); }
  void shot(const char* name) {
    char path[256]; snprintf(path, sizeof(path), "shots/%s.ppm", name);
    // let the display catch up before photographing
    if (hmi) for (int i = 0; i < 60; i++) hmi->updateDisplay(g_now + 100 + i * 30);
    lcd.savePpm(path);
    shotNames.push_back(name);
  }
  const Sample* at(uint32_t t) const { for (const Sample& s : trace) if (s.t >= t) return &s; return trace.empty() ? nullptr : &trace.back(); }
  double maxDutyBetween(uint32_t a, uint32_t b) const { double m = 0; for (const Sample& s : trace) if (s.t >= a && s.t <= b && s.duty > m) m = s.duty; return m; }
  unsigned linkDrops() const { unsigned n = 0; bool prev = false; for (const Sample& s : trace) { if (prev && !s.linkUp) n++; prev = s.linkUp; } return n; }
};

static const int UP = hmi::kBitUp, DOWN = hmi::kBitDown, P1 = hmi::kBitPreset1, P2 = hmi::kBitPreset2, P3 = hmi::kBitPreset3;

// Worst case from a press to the motherboard seeing it: HMI debounce, one poll, motherboard debounce.
static const uint32_t kPressLatencyMs = HMI_DEBOUNCE_MS + HMI_POLL_PERIOD_MS + BTN_DEBOUNCE_SAMPLES * BTN_SAMPLE_PERIOD_MS + 5;
// Worst case from the link breaking to the motor being cut.
static const uint32_t kLinkLossStopMs = HMI_LINK_TIMEOUT_MS + HMI_POLL_PERIOD_MS + BTN_IO_FAIL_LIMIT * BTN_SAMPLE_PERIOD_MS + 5;

// ------------------------------------------------------------------------------------------------
//  Tests
// ------------------------------------------------------------------------------------------------
static void testBoot() {
  printf("\n[boot] both boards, panel init, link up, idle state\n");
  Rig r;
  check(pinsDisjoint(), "motherboard and HMI pins overlap in the simulation");
  check(r.lcd.sleepOut && r.lcd.displayOn, "panel did not receive sleep-out + display-on");
  check(r.lcd.hwResets >= 1, "panel brought up without a hardware reset");
  static const uint8_t kExpectedMadctl[4] = {0x48, 0x28, 0x88, 0xE8};
  check(r.lcd.madctl == kExpectedMadctl[DISPLAY_ROTATION & 3], "MADCTL 0x%02X (expected 0x%02X for rotation %d)", r.lcd.madctl, kExpectedMadctl[DISPLAY_ROTATION & 3], DISPLAY_ROTATION);
  check(g_pinLevel[pins::MOTOR_DIR] == MOTOR_DIR_DOWN_LEVEL, "DIR pin not in a defined state after boot");
  check(g_pinMode[pins::RS485_DE] == OUTPUT && g_pinLevel[pins::RS485_DE] == LOW, "motherboard transceiver not in receive mode at boot");
  r.run(400);
  check(r.link.connected() && r.hmiLinkUp(), "link not up after 400 ms (MB %d, HMI %d)", r.link.connected(), r.hmiLinkUp());
  check(!r.hmi->inverted(), "straight cord: the HMI board should not invert its UART");
  uint32_t polls0 = r.link.polls(), replies0 = r.link.replies();
  r.run(1000);
  uint32_t polls = r.link.polls() - polls0, replies = r.link.replies() - replies0;
  check(polls >= 1000 / HMI_POLL_PERIOD_MS - 2 && replies == polls, "1 s of link: %u polls, %u answers", polls, replies);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "idle screen is %s", screenName(r.ctrl.ui().screen));
  check(r.ctrl.ui().heightValid, "height not valid");
  check(std::fabs(r.ctrl.ui().heightMm - r.w.h) < 6.0, "displayed height %d vs true %.1f", r.ctrl.ui().heightMm, r.w.h);
  check(r.w.duty() == 0.0 && r.w.pwmStuckHigh == 0, "motor should be idle at boot");
  check(r.lcd.coverage() > 0.999, "only %.1f %% of the panel drawn", 100 * r.lcd.coverage());
  r.shot("01_idle");
  printf("  1 s of link: %u polls, %u answers, %llu bytes sent by the motherboard\n", polls, replies, (unsigned long long)r.mbUart.txBytes);
}

static void testManualUp() {
  printf("\n[manual up] soft start, %d %% ceiling, direction level, soft stop\n", MOTOR_PWM_MAX_PERCENT);
  Rig r;
  r.run(500);
  double h0 = r.w.h;
  r.press(UP);
  uint32_t tp = g_now;
  r.run(300);
  double d300 = r.w.duty();
  r.run(1300);
  double d1600 = r.w.duty();
  r.run(1400);
  r.shot("02_moving_up");
  check(r.ctrl.ui().arrow == UiModel::Arrow::Up, "arrow should be Up while moving up");
  r.release(UP);
  uint32_t tr = g_now;
  r.run(450 + kPressLatencyMs);
  double maxD = r.maxDutyBetween(tp, g_now);
  const double ramp300 = kMaxDuty * 300 / MOTOR_SOFT_START_MS;     // where the soft start should be 300 ms after the press
  check(maxD <= kMaxDuty + 0.0005, "max duty %.3f exceeds %d %%", maxD, MOTOR_PWM_MAX_PERCENT);
  check(d300 > 0.0 && d300 < ramp300 * 1.5, "duty 300 ms after the press = %.3f (soft start expected < %.3f)", d300, ramp300 * 1.5);
  check(d1600 > 0.75 * kMaxDuty && d1600 <= kMaxDuty + 0.0005, "duty 1.6 s after the press = %.3f (expected close to %.2f)", d1600, kMaxDuty);
  check(r.at(tp + 500)->dirUp, "DIR pin should be at the UP level while going up");
  check(r.w.h - h0 > 25.0, "desk rose only %.1f mm", r.w.h - h0);
  check(r.w.duty() == 0.0, "duty %.3f after release (expected 0)", r.w.duty());
  check(r.ctrl.ui().arrow == UiModel::Arrow::None, "arrow should disappear after the stop");
  bool mono = true; double prev = 0;
  for (const Sample& s : r.trace) if (s.t > tp + 40 + kPressLatencyMs && s.t < tp + 1400) { if (s.duty + 1e-9 < prev) mono = false; prev = s.duty; }
  check(mono, "duty is not monotonic during the soft start");
  uint32_t tz = 0; for (const Sample& s : r.trace) if (s.t > tr && s.duty == 0.0) { tz = s.t; break; }
  check(tz && tz - tr < 450 + kPressLatencyMs, "soft stop took %u ms after the release", tz - tr);
  printf("  rose %.1f mm, soft-start duty at +300 ms = %.3f, at +1.6 s = %.3f, stop %u ms after the release\n", r.w.h - h0, d300, d1600, tz - tr);
}

static void testManualDown() {
  printf("\n[manual down] DIR level and arrow\n");
  Rig r;
  r.run(400);
  double h0 = r.w.h;
  r.press(DOWN);
  r.run(2500);
  r.shot("03_moving_down");
  check(!r.at(g_now - 100)->dirUp, "DIR pin should be at the DOWN level");
  check(r.ctrl.ui().arrow == UiModel::Arrow::Down, "arrow should be Down");
  r.release(DOWN);
  r.run(500);
  check(r.w.h < h0 - 15.0, "desk went from %.1f to %.1f", h0, r.w.h);
}

static void testPresetSave() {
  printf("\n[preset save] long press: gauge 0 -> 100 %% between 400 and 2000 ms, then success + EEPROM\n");
  Rig r(950.0);
  r.run(400);
  double trueH = r.w.h;
  r.press(P1);
  uint32_t tp = g_now;
  uint32_t tGauge = 0, tSuccess = 0;
  int shot35 = 0, shot85 = 0;
  uint8_t lastGauge = 0; bool gaugeMono = true;
  for (uint32_t i = 0; i < 2600; i++) {
    r.step();
    const UiModel& u = r.ctrl.ui();
    if (!tGauge && u.screen == UiModel::Screen::SaveGauge) tGauge = g_now - tp;
    if (!tSuccess && u.screen == UiModel::Screen::SaveSuccess) tSuccess = g_now - tp;
    if (u.screen == UiModel::Screen::SaveGauge) { if (u.gaugePercent < lastGauge) gaugeMono = false; lastGauge = u.gaugePercent; }
    if (!shot35 && u.screen == UiModel::Screen::SaveGauge && u.gaugePercent >= 35) { r.shot("04_gauge_35"); shot35 = 1; }
    if (!shot85 && u.screen == UiModel::Screen::SaveGauge && u.gaugePercent >= 85) { r.shot("05_gauge_85"); shot85 = 1; }
    if (tSuccess && !r.shotNames.empty() && r.shotNames.back() != "06_success" && g_now - tp > tSuccess + 150) { r.shot("06_success"); }
  }
  check(tGauge >= 400 && tGauge <= 400 + kPressLatencyMs, "gauge appeared %u ms after the press (expected 400-%u)", tGauge, 400 + kPressLatencyMs);
  check(tSuccess >= 1990 && tSuccess <= 2000 + kPressLatencyMs, "success shown %u ms after the press (expected 2000-%u)", tSuccess, 2000 + kPressLatencyMs);
  check(gaugeMono, "gauge percentage went backwards");
  check(r.presets.isSet(0), "preset 1 not stored");
  check(std::fabs((double)r.presets.get(0) - trueH) < 5.0, "stored %u, true height %.1f", r.presets.get(0), trueH);
  check(r.w.duty() == 0.0 && r.w.maxDuty == 0.0, "motor moved during a preset save");
  r.run(2200);                                           // button still held: success then main, no goto afterwards
  r.release(P1);
  r.run(600);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen after the save: %s", screenName(r.ctrl.ui().screen));
  check(r.w.maxDuty == 0.0, "motor moved after the long press release (spurious goto)");
  Presets p2; p2.begin();
  check(p2.isSet(0) && p2.get(0) == r.presets.get(0) && !p2.isSet(1) && !p2.isSet(2), "EEPROM persistence: p1=%u set=%d", p2.get(0), p2.isSet(0));
  printf("  gauge appeared at +%u ms, success at +%u ms, stored %u mm (true %.1f), EEPROM writes: %u\n", tGauge, tSuccess, r.presets.get(0), trueH, EEPROM.writes);
}

static void testGoto() {
  printf("\n[goto preset] approach profile, accuracy, DIR only changes at 0 %%\n");
  Rig r(1000.0);
  r.run(300);
  r.tap(P1, 2300);                                       // save 1000 mm
  r.run(2500);
  uint16_t target = r.presets.get(0);
  check(r.presets.isSet(0), "preset not saved");
  r.press(DOWN); r.run(7500); r.release(DOWN); r.run(900);
  double start = r.w.h;
  check(start < target - 110, "setup: desk only %.1f mm below the target", target - start);
  size_t idx0 = r.trace.size();
  r.tap(P1, 120);                                        // short press
  uint32_t tp = g_now;
  r.run(100);
  check(r.ctrl.ui().arrow == UiModel::Arrow::Up, "arrow should show Up during the goto");
  r.run(12000);
  double err = r.w.h - target;
  check(std::fabs(err) <= 4.0, "final error %.1f mm", err);
  check(r.w.duty() == 0.0, "motor still running after 12 s");
  double dutyAt40 = -1, dutyAt10 = -1, dutyAt100 = -1, maxD = 0; uint32_t tEnd = 0;
  for (size_t i = idx0; i < r.trace.size(); i++) {
    const Sample& s = r.trace[i];
    double rem = target - s.h;
    if (s.duty > maxD) maxD = s.duty;
    if (rem <= 100 && dutyAt100 < 0 && s.t > tp + 200) dutyAt100 = s.duty;
    if (rem <= 40 && dutyAt40 < 0) dutyAt40 = s.duty;
    if (rem <= 10 && dutyAt10 < 0) dutyAt10 = s.duty;
    if (s.t > tp + 300 && s.duty == 0.0 && !tEnd) tEnd = s.t - tp;
  }
  const double ramp40 = kMinDuty + (kMaxDuty - kMinDuty) * 40 / APPROACH_DISTANCE_MM;
  const double ramp10 = kMinDuty + (kMaxDuty - kMinDuty) * 10 / APPROACH_DISTANCE_MM;
  check(maxD <= kMaxDuty + 0.0005, "max duty %.3f", maxD);
  check(dutyAt100 > 0.83 * kMaxDuty, "duty %.3f at 100 mm from the target (should be at full speed)", dutyAt100);
  check(dutyAt40 > ramp40 - 0.15 && dutyAt40 < ramp40 + 0.06, "duty %.3f at 40 mm (expected ~%.2f following the ramp)", dutyAt40, ramp40);
  check(dutyAt10 > 0 && dutyAt10 < ramp10 + 0.10, "duty %.3f at 10 mm (expected close to the %.2f floor)", dutyAt10, kMinDuty);
  check(r.dirChangesWhileMoving == 0, "DIR pin toggled %u times while the duty was not zero", r.dirChangesWhileMoving);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen after the goto: %s", screenName(r.ctrl.ui().screen));
  printf("  from %.1f to %.1f mm (target %u): error %.2f mm, duty @100mm=%.2f @40mm=%.2f @10mm=%.2f, took %.1f s\n", start, r.w.h, target, err, dutyAt100, dutyAt40, dutyAt10, tEnd / 1000.0);
  r.press(UP); r.run(7500); r.release(UP); r.run(900);
  double start2 = r.w.h;
  r.tap(P1, 90); r.run(15000);
  double err2 = r.w.h - target;
  check(std::fabs(err2) <= 4.0, "downward goto: final error %.1f mm (from %.1f)", err2, start2);
  check(r.dirChangesWhileMoving == 0, "DIR toggled with non-zero duty during the downward goto");
  printf("  downward goto from %.1f: error %.2f mm\n", start2, err2);
}

static void testGotoCancelAndEmpty() {
  printf("\n[goto cancel / empty preset / aborted long press / conflicts]\n");
  Rig r(1000.0);
  r.run(300);
  r.tap(P2, 100);
  r.run(200);
  check(r.ctrl.ui().screen == UiModel::Screen::Notice, "short press on an empty preset shows %s", screenName(r.ctrl.ui().screen));
  r.shot("07_notice_nopreset");
  check(r.w.maxDuty == 0.0, "desk moved for an empty preset");
  r.run(1700);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "notice should disappear, screen=%s", screenName(r.ctrl.ui().screen));
  size_t writes0 = EEPROM.writes;
  r.press(P2); r.run(1000);
  check(r.ctrl.ui().screen == UiModel::Screen::SaveGauge, "gauge should be visible after 1 s");
  r.release(P2); r.run(300);
  check(!r.presets.isSet(1) && EEPROM.writes == writes0, "an aborted long press must not save");
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "aborted long press: screen=%s", screenName(r.ctrl.ui().screen));
  check(r.w.maxDuty == 0.0, "an aborted long press must not move the desk");
  r.tap(P3, 2300); r.run(2500);
  r.press(DOWN); r.run(3000); r.release(DOWN); r.run(700);
  double hBefore = r.w.h;
  r.tap(P3, 100);
  r.run(1500);
  check(r.w.duty() > 0.2 && r.ctrl.ui().arrow == UiModel::Arrow::Up, "goto should be running");
  r.press(P1);                                           // any button cancels
  r.run(500);
  check(r.w.duty() == 0.0, "motor not stopped by a button press during a goto (duty %.3f)", r.w.duty());
  double hStop = r.w.h;
  r.release(P1); r.run(1500);
  check(std::fabs(r.w.h - hStop) < 1.0, "the cancelling press triggered something after its release (%.1f -> %.1f)", hStop, r.w.h);
  check(hStop > hBefore + 5, "goto did not move before the cancel");
  r.press(UP); r.press(DOWN); r.run(500);
  check(r.w.duty() == 0.0, "Up+Down must not move the desk");
  r.release(DOWN); r.run(500);
  check(r.w.duty() == 0.0, "releasing one of the two buttons must not start a movement");
  r.release(UP); r.run(300);
  r.press(UP); r.run(800);
  double dBefore = r.w.duty();
  r.tap(P3, 120);
  r.run(600);
  check(r.w.duty() >= dBefore - 0.001 && r.ctrl.ui().arrow == UiModel::Arrow::Up, "a preset tap during a manual move must be ignored");
  r.release(UP); r.run(600);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen: %s", screenName(r.ctrl.ui().screen));
}

static void testGotoStopUpDown() {
  printf("\n[goto stop with Up + Down] the second button of the pair must not start a manual move\n");
  Rig r(1000.0);
  r.run(300);
  r.tap(P1, 2300); r.run(2500);                          // preset 1 = 1000 mm
  r.press(DOWN); r.run(4000); r.release(DOWN); r.run(700);
  r.tap(P1, 100); r.run(1500);
  check(r.w.duty() > 0.2 && r.ctrl.ui().arrow == UiModel::Arrow::Up, "goto should be running");
  uint32_t t0 = g_now;
  r.press(UP); r.run(60); r.press(DOWN);                 // not exactly together, as a hand does it
  uint32_t tStop = 0;
  for (int i = 0; i < 600; i++) { r.step(); if (!tStop && r.w.duty() == 0.0) tStop = g_now - t0; }
  double hStop = r.w.h;
  check(tStop > 0 && tStop <= 450, "goto stopped %u ms after Up + Down", tStop);
  r.release(UP); r.run(400);                             // Down still held alone for a while
  check(r.w.duty() == 0.0 && r.ctrl.ui().arrow == UiModel::Arrow::None, "Down held after the stop started a move");
  r.release(DOWN); r.run(800);
  check(std::fabs(r.w.h - hStop) < 1.0, "the desk moved after the stop (%.1f -> %.1f)", hStop, r.w.h);
  r.press(DOWN); r.run(1500);
  check(r.w.duty() > 0.1, "Down does not work after the stop");
  r.release(DOWN); r.run(600);
  printf("  goto stopped %u ms after Up + Down, desk still afterwards\n", tStop);
}

static void testLimits() {
  printf("\n[limits] slow down and stop at HEIGHT_MAX / HEIGHT_MIN, no restart while held\n");
  Rig r(1130.0);
  r.run(400);
  r.press(UP); r.run(9000);
  double hMax = r.w.h;
  check(r.w.duty() == 0.0, "motor still driving at the upper limit");
  check(hMax >= HEIGHT_MAX_MM - 4 && hMax <= HEIGHT_MAX_MM + 4, "stopped at %.1f mm (limit %d)", hMax, HEIGHT_MAX_MM);
  r.run(2000);
  check(std::fabs(r.w.h - hMax) < 0.5, "desk restarted while Up is still held");
  r.release(UP); r.run(300);
  r.press(DOWN); r.run(1500);
  check(r.w.h < hMax - 3, "Down must work after the upper limit");
  r.release(DOWN); r.run(500);
  Rig r2(HEIGHT_MIN_MM + 120.0);
  r2.run(400);
  r2.press(DOWN); r2.run(9000);
  check(r2.w.duty() == 0.0 && std::fabs(r2.w.h - HEIGHT_MIN_MM) <= 4, "lower limit: stopped at %.1f (limit %d)", r2.w.h, HEIGHT_MIN_MM);
  double dNear = 0; for (const Sample& s : r2.trace) if (s.h < HEIGHT_MIN_MM + 15 && s.duty > 0) { dNear = s.duty; break; }
  check(dNear > 0 && dNear < 0.35, "duty %.3f 15 mm before the lower limit", dNear);
  printf("  upper stop %.1f mm, lower stop %.1f mm, mech limits never hit: %s\n", hMax, r2.w.h, (hMax < r.w.mechMax - 1 && r2.w.h > r2.w.mechMin + 1) ? "yes" : "NO");
}

static void testSensorLoss() {
  printf("\n[sensor loss] motor must stop quickly, fault shown, recovery when the sensor is back\n");
  Rig r(900.0);
  r.run(400);
  r.press(UP); r.run(1500);
  check(r.w.duty() > 0.2, "should be moving");
  uint32_t tLoss = g_now;
  g_tofPresent = false;
  uint32_t tStop = 0;
  for (int i = 0; i < 1500; i++) { r.step(); if (!tStop && r.w.duty() == 0.0) tStop = g_now - tLoss; }
  check(tStop > 0 && tStop <= 520, "motor stopped %u ms after the sensor disappeared (limit ~ TOF_STALE_TIMEOUT_MS + a loop)", tStop);
  check(r.ctrl.ui().screen == UiModel::Screen::Fault, "screen: %s", screenName(r.ctrl.ui().screen));
  r.shot("08_fault_sensor");
  check(r.lcd.faultDiscShown(), "the HMI board does not show the fault screen");
  check(g_tof.violations == 0, "the ST driver was entered %u times while the sensor did not ACK", g_tof.violations);
  r.release(UP); r.run(500);
  g_tofPresent = true;
  r.run(5000);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "after the sensor came back: %s", screenName(r.ctrl.ui().screen));
  check(r.ctrl.ui().heightValid, "height should be valid again");
  r.press(UP); r.run(1500);
  check(r.w.duty() > 0.1, "desk should move again after the recovery");
  r.release(UP); r.run(600);
  printf("  motor cut %u ms after the loss\n", tStop);
}

static void testFaults() {
  printf("\n[faults] wrong way, stall, watchdog ISR\n");
  {
    Rig r(900.0);
    r.w.polarity = -1.0;                                  // motor wired backwards
    r.run(400);
    double h0 = r.w.h;
    r.press(UP);
    uint32_t tp = g_now, tf = 0;
    for (int i = 0; i < 6000; i++) { r.step(); if (!tf && r.ctrl.ui().screen == UiModel::Screen::Fault) tf = g_now - tp; }
    check(tf > 0, "wrong direction not detected");
    check(h0 - r.w.h < 30.0, "desk travelled %.1f mm the wrong way before the stop", h0 - r.w.h);
    check(r.w.duty() == 0.0, "motor not stopped after the wrong-way fault");
    r.shot("09_fault_wrongway");
    printf("  wrong way detected after %u ms and %.1f mm\n", tf, h0 - r.w.h);
  }
  {
    Rig r(900.0);
    r.w.jam = true;
    r.run(400);
    r.press(UP);
    uint32_t tp = g_now, tf = 0;
    for (int i = 0; i < 6000; i++) { r.step(); if (!tf && r.ctrl.ui().screen == UiModel::Screen::Fault) tf = g_now - tp; }
    const uint32_t tStall = (uint32_t)(MOTOR_SOFT_START_MS * kMinDuty / kMaxDuty) + STALL_TIMEOUT_MS;
    check(tf + 200 >= tStall && tf <= tStall + 800, "stall detected after %u ms (expected ~%u ms)", tf, tStall);
    check(r.w.duty() == 0.0, "motor not stopped after the stall fault");
    printf("  stall detected after %u ms\n", tf);
  }
  {
    Rig r(900.0);
    r.run(400);
    r.press(UP); r.run(1500);
    motorFailsafeCut();                                   // what the watchdog interrupt does
    check(!(TCCR1A & _BV(COM1B1)) && !(PORTB & _BV(PORTB6)), "failsafe must disconnect and clear the PWM pin");
    check(!supplySwitchOn(), "failsafe must switch the driver supply off");
    r.run(5);
    check(r.w.duty() == 0.0, "duty must stay 0 after the failsafe");
    check(r.ctrl.ui().screen == UiModel::Screen::Fault, "failsafe should raise a fault, screen=%s", screenName(r.ctrl.ui().screen));
    r.release(UP); r.run(4000);
    check(r.ctrl.ui().screen == UiModel::Screen::Main, "fault should clear: %s", screenName(r.ctrl.ui().screen));
  }
}

static unsigned supplyEdges(const Rig& r) {
  unsigned n = 0; bool prev = false;
  for (const Sample& s : r.trace) { if (s.supply != prev) n++; prev = s.supply; }
  return n;
}

static void testMotorSupply() {
  printf("\n[driver supply] on with the first move, PWM once it has settled, off after %lu s without a move\n",
         (unsigned long)(MOTOR_POWER_IDLE_OFF_MS / 1000));
  Rig r(900.0);
  check(g_pinMode[pins::MOTOR_POWER] == OUTPUT && !supplySwitchOn(), "driver supply not switched off at boot");
  r.run(400);
  check(!supplySwitchOn(), "driver supply switched on without a move");
  r.press(UP);
  uint32_t tp = g_now;
  r.run(1500); r.release(UP); r.run(600);
  uint32_t tOn = 0, tPwm = 0;
  for (const Sample& s : r.trace) if (s.t > tp) { if (!tOn && s.supply) tOn = s.t; if (!tPwm && s.duty > 0) tPwm = s.t; }
  check(tOn && tOn - tp <= kPressLatencyMs + 3, "supply on %u ms after the press", tOn - tp);
  check(tPwm && tPwm - tOn >= MOTOR_POWER_SETTLE_MS && tPwm - tOn <= MOTOR_POWER_SETTLE_MS + 5,
        "first PWM %u ms after the supply (expected %u)", tPwm - tOn, (unsigned)MOTOR_POWER_SETTLE_MS);
  check(r.w.h > 905.0, "desk did not move (%.1f mm)", r.w.h);

  r.run(60000);                                          // a move inside the idle time does not wait for the supply
  check(supplySwitchOn(), "supply off 60 s after a move");
  r.press(DOWN);
  uint32_t tp2 = g_now;
  r.run(800); r.release(DOWN); r.run(700);
  uint32_t tPwm2 = 0;
  for (const Sample& s : r.trace) if (s.t > tp2 && s.duty > 0) { tPwm2 = s.t; break; }
  check(tPwm2 && tPwm2 - tp2 <= kPressLatencyMs + 3, "second move: first PWM %u ms after the press", tPwm2 - tp2);

  uint32_t tLast = 0;
  for (const Sample& s : r.trace) if (s.duty > 0) tLast = s.t;
  uint32_t tOff = 0;
  while (g_now - tLast < MOTOR_POWER_IDLE_OFF_MS + 2000) { r.step(); if (!tOff && !supplySwitchOn()) tOff = g_now; }
  check(tOff && tOff - tLast >= MOTOR_POWER_IDLE_OFF_MS && tOff - tLast <= MOTOR_POWER_IDLE_OFF_MS + 20,
        "supply off %u ms after the last move (expected %lu)", tOff - tLast, (unsigned long)MOTOR_POWER_IDLE_OFF_MS);
  check(r.ctrl.ui().screen == UiModel::Screen::Main && r.ctrl.ui().heightValid, "screen after the supply went off: %s", screenName(r.ctrl.ui().screen));

  r.press(UP);                                           // and back on with the next move
  uint32_t tp3 = g_now;
  r.run(1200); r.release(UP); r.run(600);
  uint32_t tOn3 = 0, tPwm3 = 0;
  for (const Sample& s : r.trace) if (s.t > tp3) { if (!tOn3 && s.supply) tOn3 = s.t; if (!tPwm3 && s.duty > 0) tPwm3 = s.t; }
  check(tOn3 && tPwm3 && tPwm3 - tOn3 >= MOTOR_POWER_SETTLE_MS, "after the idle cut: supply %u ms, PWM %u ms after the press", tOn3 - tp3, tPwm3 - tp3);
  check(supplyEdges(r) == 3, "the supply switched %u times (expected on, off, on)", supplyEdges(r));
  printf("  supply on %u ms after the press, PWM %u ms later; off %.1f s after the last move; %u switchings\n",
         tOn - tp, tPwm - tOn, (tOff - tLast) / 1000.0, supplyEdges(r));
}

static void testOvercurrent() {
  printf("\n[overcurrent] INA226 alert on INT6: PWM and driver supply cut in the interrupt, fault, recovery\n");
  Rig r(900.0);
  int n = digitalPinToInterrupt(pins::I_ALERT);
  check(n >= 0 && g_isr[n] && g_isrMode[n] == FALLING, "I_ALERT is not on an external interrupt (falling edge)");
  check(r.current.present() && g_ina.config == kInaConfig && g_ina.cal == kInaCal && g_ina.limit == kInaLimit && g_ina.mask == kInaMask,
        "INA226 programming: config 0x%04X cal %u limit %u mask 0x%04X", g_ina.config, g_ina.cal, g_ina.limit, g_ina.mask);
  r.run(400);
  r.press(UP); r.run(2500);
  int32_t ma = 0;
  bool read = r.current.readCurrentMa(ma);
  check(read && std::fabs(ma - r.w.amps * 1000) <= 0.02 * r.w.amps * 1000 + 5, "INA226 reads %ld mA, true %.0f mA", (long)ma, r.w.amps * 1000);
  check(g_pinLevel[pins::I_ALERT] == HIGH && r.ctrl.ui().screen == UiModel::Screen::Main, "alert at the normal current");
  r.release(UP); r.run(600);

  r.w.jam = true; r.w.jamA = 15.0;                       // jammed: the current climbs with the soft start
  r.press(UP);
  uint32_t tOver = 0, tCut = 0, tFault = 0;
  double peak = 0;
  for (int i = 0; i < 3000; i++) {
    r.step();
    if (r.w.amps > peak) peak = r.w.amps;
    if (!tOver && r.w.amps * 1000 > MOTOR_CURRENT_LIMIT_MA) tOver = g_now;
    if (tOver && !tCut && !supplySwitchOn()) {
      tCut = g_now;
      check(r.w.duty() == 0.0, "supply cut but the PWM still runs");
    }
    if (!tFault && r.ctrl.ui().screen == UiModel::Screen::Fault) tFault = g_now;
  }
  check(tOver && tCut && tCut - tOver <= 20, "cut %u ms after the current crossed %d mA (2 INA226 samples max)", tCut - tOver, MOTOR_CURRENT_LIMIT_MA);
  check(tFault && tFault - tCut <= 2, "fault shown %u ms after the cut", tFault - tCut);
  check(r.ctrl.ui().title == UiText::Overcurrent, "fault title %u", (unsigned)r.ctrl.ui().title);
  check(!r.motor.isPowered() && !supplySwitchOn() && r.maxDutyBetween(tCut, g_now) == 0.0, "the motor restarted while Up is held");
  check(g_pinLevel[pins::I_ALERT] == HIGH && !g_ina.aff, "INA226 alert latch not cleared by the firmware");
  r.shot("14_fault_overcurrent");

  r.release(UP);
  r.w.jam = false;
  r.run(FAULT_MIN_DISPLAY_MS + 500);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "fault not cleared: %s", screenName(r.ctrl.ui().screen));
  double h0 = r.w.h;
  r.press(UP); r.run(1500);
  check(r.w.duty() > 0.1 && supplySwitchOn() && r.w.h > h0 + 3, "the desk does not move after the overcurrent fault");
  r.release(UP); r.run(600);
  printf("  current over %.1f A: driver supply and PWM cut %u ms later (peak %.1f A), fault on screen\n",
         MOTOR_CURRENT_LIMIT_MA / 1000.0, tCut - tOver, peak);

  // The interrupt can hit anywhere in the loop, e.g. just before Motor::update(): that must not undo the cut.
  Rig r2(900.0);
  r2.run(400);
  r2.press(UP); r2.run(1500);
  g_isr[n]();                                            // INT6 handler
  r2.motor.update(g_now + 1);
  check(pwmDuty() == 0.0 && !supplySwitchOn(), "Motor::update() undid the interrupt cut (duty %.3f, supply %d)", pwmDuty(), supplySwitchOn());
  r2.run(5);
  check(r2.ctrl.ui().screen == UiModel::Screen::Fault && r2.ctrl.ui().title == UiText::Overcurrent, "alert in the loop: %s", screenName(r2.ctrl.ui().screen));
  r2.release(UP); r2.run(600);
}

static void testCurrentMonitorMissing() {
  printf("\n[INA226 missing / lost / reset] no move without the current protection, recovery\n");
  {
    Rig r(900.0, false, 12345, 1000, true, false);       // INA226 not answering at boot
    r.run(400);
    check(!r.current.present(), "absent INA226 reported present");
    r.press(UP); r.run(600);
    check(r.w.maxDuty == 0.0 && !supplySwitchOn(), "the desk moved or the driver was powered without the INA226");
    check(r.ctrl.ui().screen == UiModel::Screen::Notice && r.ctrl.ui().title == UiText::NoCurrentSense, "Up without the INA226: %s", screenName(r.ctrl.ui().screen));
    r.shot("15_notice_no_current_sense");
    r.tap(P1, 100); r.run(300);
    check(r.w.maxDuty == 0.0, "goto without the INA226");
    r.release(UP); r.run(2000);
    g_ina.present = true;                                // answers again (late power-up, I2C glitch)
    r.run(CURRENT_MONITOR_RETRY_MS + 100);
    check(r.current.present() && g_ina.limit == kInaLimit && g_ina.mask == kInaMask, "INA226 not configured once it answers");
    r.press(UP); r.run(1500);
    check(r.w.duty() > 0.1, "the desk does not move once the INA226 answers");
    r.release(UP); r.run(600);
  }
  {
    Rig r(900.0);
    r.run(400);
    r.press(UP); r.run(1500);
    check(r.w.duty() > 0.2, "should be moving");
    uint32_t t0 = g_now, tStop = 0;
    g_ina.present = false;                               // lost while moving
    for (int i = 0; i < 2000; i++) { r.step(); if (!tStop && r.w.duty() == 0.0) tStop = g_now - t0; }
    check(tStop > 0 && tStop <= CURRENT_MONITOR_RETRY_MS + 20, "motor stopped %u ms after the INA226 stopped answering", tStop);
    check(r.ctrl.ui().screen == UiModel::Screen::Fault && r.ctrl.ui().title == UiText::NoCurrentSense, "INA226 lost: %s", screenName(r.ctrl.ui().screen));
    r.release(UP);
    g_ina.present = true;
    r.run(FAULT_MIN_DISPLAY_MS + CURRENT_MONITOR_RETRY_MS + 200);
    check(r.ctrl.ui().screen == UiModel::Screen::Main, "fault not cleared once the INA226 answers: %s", screenName(r.ctrl.ui().screen));
    printf("  INA226 lost while moving: motor stopped after %u ms\n", tStop);
  }
  {
    Rig r(900.0);
    r.run(400);
    g_ina.powerOnReset();                                // INA226 brown-out: alert settings back to the defaults
    r.run(CURRENT_MONITOR_RETRY_MS + 100);
    check(g_ina.config == kInaConfig && g_ina.cal == kInaCal && g_ina.limit == kInaLimit && g_ina.mask == kInaMask,
          "INA226 settings not restored after its reset");
  }
}

static void testButtonsAndEeprom() {
  printf("\n[buttons / eeprom] bounce immunity, corrupted EEPROM\n");
  {
    Rig r(900.0);
    r.run(300);
    r.w.setButton(P1, true, 12);                          // 12 ms of chatter on the press...
    r.run(80);
    r.w.setButton(P1, false, 12);                         // ...and on the release
    r.run(100);
    check(r.ctrl.ui().screen == UiModel::Screen::Notice, "chattering tap not recognised (screen %s)", screenName(r.ctrl.ui().screen));
    check(r.w.maxDuty == 0.0, "chatter moved the desk");
  }
  {
    Rig r(900.0);
    r.run(300);
    uint32_t t0 = g_now; bool seen = false;
    r.w.setButton(UP, true, 0); r.run(8); r.w.setButton(UP, false, 0);   // an 8 ms glitch is not a press
    for (int i = 0; i < 200; i++) { r.step(); if (r.hmi->buttons() & (1 << UP)) seen = true; }
    check(!seen && r.w.maxDuty == 0.0, "an 8 ms glitch was reported as a press");
    (void)t0;
  }
  {
    Rig r(900.0);
    r.run(300); r.tap(P1, 2300); r.run(2500);
    check(r.presets.isSet(0), "setup: preset not saved");
    EEPROM.mem[PRESET_EEPROM_ADDR + 3] ^= 0x10;           // flip one bit of the stored data
    Presets p; p.begin();
    check(!p.isSet(0), "corrupted EEPROM image (CRC mismatch) must be rejected");
  }
}

static void testSlowLoop() {
  printf("\n[slow loop] the same behaviour with a 20 ms motherboard loop\n");
  Rig r(1000.0);
  r.loopStepMs = 20;
  r.run(500);
  r.tap(P1, 2400); r.run(2800);
  uint16_t target = r.presets.get(0);
  check(r.presets.isSet(0), "preset not saved with a slow loop");
  r.press(DOWN); r.run(3000); r.release(DOWN); r.run(1000);
  r.tap(P1, 150); r.run(15000);
  check(std::fabs(r.w.h - target) <= 5.0, "slow loop goto error %.1f mm", r.w.h - target);
  check(r.linkDrops() == 0, "the link dropped %u times with a 20 ms loop", r.linkDrops());
  printf("  slow loop goto error %.2f mm\n", r.w.h - target);
}

static void testNoSensorNotices() {
  printf("\n[no sensor at start] notices instead of motion\n");
  Rig r(900.0);
  r.run(300);
  g_tofPresent = false;
  r.run(1500);
  check(!r.ctrl.ui().heightValid, "height should be flagged invalid");
  r.press(UP); r.run(600);
  check(r.w.maxDuty == 0.0, "desk moved without a height measurement");
  check(r.ctrl.ui().screen == UiModel::Screen::Notice, "Up with no sensor: %s", screenName(r.ctrl.ui().screen));
  r.release(UP); r.run(2000);
  r.press(P1); r.run(2300);
  check(!r.presets.isSet(0), "preset saved without a height");
  r.release(P1); r.run(300);
  r.tap(P1, 100); r.run(300);
  check(r.w.maxDuty == 0.0, "goto without a height");
}

static void testRollover() {
  printf("\n[millis rollover] a goto that crosses the 32-bit millis() wrap\n");
  Rig r(1000.0, false, 777, 0xFFFFFFFFu - 4000u);
  r.run(300);
  r.tap(P1, 2300); r.run(2200);                          // millis() wraps during this save
  check(r.presets.isSet(0), "preset not saved across the rollover");
  r.press(DOWN); r.run(7000); r.release(DOWN); r.run(900);
  r.tap(P1, 100); r.run(15000);
  double err = r.w.h - r.presets.get(0);
  check(std::fabs(err) <= 4.0, "error after the rollover %.1f mm", err);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen %s", screenName(r.ctrl.ui().screen));
  check(r.linkDrops() == 0, "the link dropped %u times across the rollover", r.linkDrops());
  printf("  millis() now %u, goto error %.2f mm\n", (unsigned)g_now, err);
}

static void testMonteCarlo() {
  printf("\n[monte carlo] 40 gotos, random start, seeds and noise: accuracy and false faults\n");
  double sum = 0, sumSq = 0, worst = 0; int n = 0, faults = 0;
  for (int i = 0; i < 40; i++) {
    double sigma = (i % 4 == 0) ? 3.0 : (i % 4 == 1 ? 2.0 : 1.2);
    Rig r(1000.0, false, 1000 + i);
    r.w.noiseSigma = sigma;
    r.run(300);
    r.tap(P1, 2300); r.run(2200);
    if (!r.presets.isSet(0)) { faults++; continue; }
    double target = r.presets.get(0);
    bool up = (i % 2) == 0;
    r.press(up ? UP : DOWN); r.run(2500 + (i % 5) * 1500); r.release(up ? UP : DOWN); r.run(900);
    r.tap(P1, 100); r.run(16000);
    bool fault = r.ctrl.ui().screen == UiModel::Screen::Fault;
    for (const Sample& s : r.trace) if (s.screen == UiModel::Screen::Fault) fault = true;
    if (fault) { faults++; printf("  run %d (sigma %.1f): FAULT\n", i, sigma); continue; }
    double err = r.w.h - target;
    sum += err; sumSq += err * err; if (std::fabs(err) > worst) worst = std::fabs(err); n++;
  }
  double mean = sum / n, sd = std::sqrt(sumSq / n - mean * mean);
  check(faults == 0, "%d runs ended in a fault", faults);
  check(worst <= 7.0, "worst final error %.1f mm", worst);
  printf("  %d runs: mean error %+.2f mm, sd %.2f mm, worst %.2f mm, false faults %d\n", n, mean, sd, worst, faults);
}

static void testWatchdogGuard() {
  printf("\n[watchdog] kick() must not fight the USB core's 120 ms bootloader reset\n");
  g_wdrCount = 0;
  watchdog::begin();
  check((WDTCSR & _BV(WDE)) && (WDTCSR & _BV(WDIE)), "watchdog should run in interrupt + reset mode");
  watchdog::kick(); watchdog::kick();
  check(g_wdrCount == 2, "kicks not executed (%u)", g_wdrCount);
  uint8_t mine = WDTCSR;
  wdt_enable(WDTO_120MS);                                // what the Arduino USB core does after a 1200 bps touch
  unsigned before = g_wdrCount;
  watchdog::kick(); watchdog::kick(); watchdog::kick();
  check(g_wdrCount == before, "kick() kept resetting a watchdog that was re-programmed by the USB core");
  WDTCSR = mine;
  watchdog::kick();
  check(g_wdrCount == before + 1, "kick() did not resume after the original configuration came back");
  WDTCSR &= (uint8_t)~_BV(WDIE);
  watchdog::kick();
  check(WDTCSR & _BV(WDIE), "kick() must re-arm the interrupt stage");
}

static void testGaugeScreens() {
  printf("\n[ui] screenshots of the remaining screens\n");
  Rig r(1042.0);
  r.run(400);
  r.shot("10_idle_104cm");
  g_tofPresent = false; r.run(1500);
  r.shot("11_idle_no_height");
}

// ------------------------------------------------------------------------------------------------
//  Link: cord, polarity, data pair, noise
// ------------------------------------------------------------------------------------------------
static void testProtocol() {
  printf("\n[protocol] frame codec: round trip, CRC, resynchronisation, out-of-range fields\n");
  UiModel m; m.screen = UiModel::Screen::SaveSuccess; m.arrow = UiModel::Arrow::Down; m.heightMm = -12345; m.heightValid = true;
  m.presetNumber = 3; m.gaugePercent = 77; m.savedHeightMm = 1234; m.title = UiText::NoLink; m.subtitle = UiText::CheckCable;
  uint8_t payload[hmi::kStatePayloadSize]; payload[0] = 42; hmi::packUi(m, payload + 1);
  uint8_t frame[hmi::kMaxFrame];
  uint8_t n = hmi::encodeFrame(hmi::kTypeState, payload, sizeof(payload), frame);
  hmi::FrameParser p;
  uint8_t noise[] = {0x00, hmi::kSync, 0x33, 0xFF, hmi::kSync, hmi::kSync};     // junk with false sync bytes first
  bool ok = false;
  for (uint8_t b : noise) ok |= p.feed(b);
  for (uint8_t i = 0; i < n; i++) ok = p.feed(frame[i]);
  UiModel back;
  check(ok && p.type() == hmi::kTypeState && p.payload()[0] == 42 && hmi::unpackUi(p.payload() + 1, back), "frame not decoded after junk");
  check(back.screen == m.screen && back.arrow == m.arrow && back.heightMm == m.heightMm && back.heightValid && back.presetNumber == 3 &&
        back.gaugePercent == 77 && back.savedHeightMm == 1234 && back.title == UiText::NoLink && back.subtitle == UiText::CheckCable, "UiModel round trip");
  unsigned undetected = 0;
  for (uint8_t i = 1; i < n; i++) for (uint8_t bit = 0; bit < 8; bit++) {
    uint8_t f2[hmi::kMaxFrame]; memcpy(f2, frame, n); f2[i] ^= (uint8_t)(1 << bit);
    hmi::FrameParser q; bool got = false;
    for (uint8_t k = 0; k < n; k++) got = q.feed(f2[k]);
    if (got) undetected++;
  }
  check(undetected == 0, "%u single-bit errors went through the CRC", undetected);
  uint8_t bad[hmi::kUiPackedSize]; hmi::packUi(m, bad); bad[0] = 99;
  check(!hmi::unpackUi(bad, back), "an unknown screen number was accepted");
  hmi::packUi(m, bad); bad[9] = (uint8_t)UiText::Count;
  check(!hmi::unpackUi(bad, back), "an unknown text number was accepted");
}

static void testUnplugWhileMoving() {
  printf("\n[cord pulled out while moving] motor cut, no message, everything back when it is plugged in again\n");
  Rig r(900.0);
  r.run(400);
  r.press(UP); r.run(1500);
  check(r.w.duty() > 0.2, "should be moving");
  uint32_t tOut = g_now, tStop = 0;
  r.plug(false);
  for (int i = 0; i < 300; i++) { r.step(); if (!tStop && r.w.duty() == 0.0) tStop = g_now - tOut; }
  check(tStop > 0 && tStop <= kLinkLossStopMs, "motor stopped %u ms after the cord was pulled (limit %u)", tStop, kLinkLossStopMs);
  r.run(3000);                                           // Up still held: nobody can see it
  check(r.w.duty() == 0.0, "motor restarted while the cord is out");
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "no message for an unpowered HMI board (screen %s)", screenName(r.ctrl.ui().screen));

  uint32_t tIn = g_now, tLink = 0, tDrawn = 0;
  bool faultDrawn = false;
  r.plug(true);                                          // back in while Up is still held
  for (int i = 0; i < 2500; i++) {
    r.step();
    if (!tLink && r.link.connected()) tLink = g_now - tIn;
    if (!tDrawn && (int32_t)(g_now - r.panelReadyAt) >= 0 && r.lcd.coverage() > 0.999) tDrawn = g_now - tIn;
    if (r.lcd.faultDiscShown()) faultDrawn = true;
  }
  check(tLink > 0 && tLink <= 2 * HMI_POLARITY_SWAP_MS, "link up %u ms after the cord came back", tLink);
  check(tDrawn > 0 && tDrawn <= kPanelInitMs + 300, "screen fully drawn %u ms after the cord came back", tDrawn);
  check(!faultDrawn, "a fault screen was drawn after the reconnection");
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen after the reconnection: %s", screenName(r.ctrl.ui().screen));
  check(r.maxDutyBetween(tIn, g_now) == 0.0, "the desk moved while Up was still held across the reconnection");
  r.shot("12_after_reconnection");
  r.release(UP); r.run(300);
  r.press(UP); r.run(1500);
  check(r.w.duty() > 0.1, "the desk does not move after the reconnection");
  r.release(UP); r.run(600);
  printf("  motor cut %u ms after the cord was pulled; cord back: link after %u ms, screen drawn after %u ms\n", tStop, tLink, tDrawn);
}

static void testBootWithoutCord() {
  printf("\n[boot without the cord] nothing moves, everything comes up when it is plugged in\n");
  Rig r(900.0, false, 12345, 1000, false);
  r.run(2000);
  check(!r.link.connected() && r.w.maxDuty == 0.0, "no HMI board: the link must be down and the desk still");
  r.plug(true);
  bool faultDrawn = false;
  for (int i = 0; i < 3000; i++) { r.step(); if (r.lcd.faultDiscShown()) faultDrawn = true; }
  check(!faultDrawn, "a fault screen was drawn once the cord was plugged in");
  check(r.link.connected(), "link not up once the cord is in");
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen once the cord is in: %s", screenName(r.ctrl.ui().screen));
  check(r.lcd.coverage() > 0.999, "only %.1f %% of the panel drawn", 100 * r.lcd.coverage());
  r.press(UP); r.run(1500);
  check(r.w.duty() > 0.1, "the desk does not move once the cord is in");
  r.release(UP); r.run(600);
}

static void testReversedCord() {
  printf("\n[reversed cord] A and B swapped: the HMI board finds the polarity by itself\n");
  for (int phase = 0; phase < 2; phase++) {
    Rig r(900.0, false, 3000 + phase, 1000, false);
    g_bus.swapped = true;
    if (phase == 1) r.run(1500);                         // motherboard already polling when the cord goes in
    uint32_t t0 = g_now, tLink = 0;
    r.plug(true);
    for (int i = 0; i < 1500; i++) { r.step(); if (!tLink && r.link.connected()) tLink = g_now - t0; }
    check(tLink > 0 && tLink <= 3 * HMI_POLARITY_SWAP_MS, "link up %u ms after a reversed cord was plugged in", tLink);
    check(r.hmi && r.hmi->inverted(), "the HMI board should have inverted its UART");
    check(r.linkDrops() <= 1, "the link dropped %u times once found", r.linkDrops());
    r.press(UP); r.run(1500);
    check(r.w.duty() > 0.1, "buttons do not work through a reversed cord");
    r.release(UP); r.run(600);
    check(r.lcd.coverage() > 0.999 && !r.lcd.faultDiscShown(), "screen not normal through a reversed cord");
    printf("  %s: link up after %u ms\n", phase ? "cord plugged into a running desk" : "power-up with a reversed cord", tLink);
  }
}

static void testDataPairCut() {
  printf("\n[data pair cut, HMI board still powered] motor cut, the HMI board shows NO LINK, recovery\n");
  Rig r(900.0);
  r.run(400);
  r.press(DOWN); r.run(1500);
  check(r.w.duty() > 0.2, "should be moving");
  uint32_t tCut = g_now, tStop = 0, tNoLink = 0;
  g_bus.dataCut = true;
  for (int i = 0; i < 1500; i++) {
    r.step();
    if (!tStop && r.w.duty() == 0.0) tStop = g_now - tCut;
    if (!tNoLink && r.lcd.faultDiscShown()) tNoLink = g_now - tCut;
  }
  check(tStop > 0 && tStop <= kLinkLossStopMs, "motor stopped %u ms after the data pair was cut", tStop);
  check(tNoLink >= HMI_NO_LINK_MS - HMI_POLL_PERIOD_MS && tNoLink <= HMI_NO_LINK_MS + 100, "NO LINK shown %u ms after the cut", tNoLink);
  r.shot("13_no_link");
  g_bus.dataCut = false;                                 // Down still held
  uint32_t tBack = g_now, tLink = 0;
  for (int i = 0; i < 1500; i++) { r.step(); if (!tLink && r.link.connected()) tLink = g_now - tBack; }
  check(tLink > 0 && tLink <= 3 * HMI_POLARITY_SWAP_MS, "link back %u ms after the repair", tLink);
  check(!r.lcd.faultDiscShown() && r.ctrl.ui().screen == UiModel::Screen::Main, "screen after the repair: %s", screenName(r.ctrl.ui().screen));
  check(r.maxDutyBetween(tBack, g_now) == 0.0, "the desk moved while Down was still held across the outage");
  r.release(DOWN); r.run(300);
  r.press(DOWN); r.run(1200);
  check(r.w.duty() > 0.1, "the desk does not move after the repair");
  r.release(DOWN); r.run(600);
  printf("  motor cut after %u ms, NO LINK after %u ms, link back %u ms after the repair\n", tStop, tNoLink, tLink);
}

static void testNoisyLink() {
  printf("\n[noisy link] 1 byte in 500 corrupted: CRC catches it, no false stop, gotos unaffected\n");
  Rig r(1000.0);
  g_bus.byteErrorRate = 0.002;
  r.run(300);
  r.tap(P1, 2300); r.run(2500);
  uint16_t target = r.presets.get(0);
  r.press(DOWN); r.run(6000); r.release(DOWN); r.run(900);
  r.tap(P1, 100); r.run(15000);
  double err = r.w.h - target;
  check(r.presets.isSet(0) && std::fabs(err) <= 4.0, "goto error %.1f mm over a noisy link", err);
  check(r.linkDrops() == 0, "the link dropped %u times (false stops)", r.linkDrops());
  double answered = (double)r.link.replies() / r.link.polls();
  check(answered > 0.9, "only %.1f %% of the polls answered", 100 * answered);
  check(g_bus.corrupted > 20, "the noise was not exercised (%u bytes)", g_bus.corrupted);
  printf("  %u corrupted bytes, %.2f %% of the polls answered, goto error %.2f mm\n", g_bus.corrupted, 100 * answered, err);
}

static void testUnplugSlowLoop() {
  printf("\n[cord pulled out, 20 ms motherboard loop] stop latency\n");
  Rig r(900.0);
  r.loopStepMs = 20;
  r.run(400);
  r.press(UP); r.run(2000);
  uint32_t tOut = g_now, tStop = 0;
  r.plug(false);
  for (int i = 0; i < 300; i++) { r.step(); if (!tStop && r.w.duty() == 0.0) tStop = g_now - tOut; }
  check(tStop > 0 && tStop <= kLinkLossStopMs + 40, "motor stopped %u ms after the cord was pulled (20 ms loop)", tStop);
  printf("  motor cut %u ms after the cord was pulled\n", tStop);
}

int main() {
  printf("Desktopia simulation – motherboard (ATmega32U4) + HMI board (RP2040) over RS-485\n");
  testProtocol();
  testBoot();
  testManualUp();
  testManualDown();
  testPresetSave();
  testGoto();
  testGotoCancelAndEmpty();
  testGotoStopUpDown();
  testLimits();
  testSensorLoss();
  testFaults();
  testMotorSupply();
  testOvercurrent();
  testCurrentMonitorMissing();
  testButtonsAndEeprom();
  testSlowLoop();
  testNoSensorNotices();
  testRollover();
  testMonteCarlo();
  testWatchdogGuard();
  testGaugeScreens();
  testUnplugWhileMoving();
  testBootWithoutCord();
  testReversedCord();
  testDataPairCut();
  testNoisyLink();
  testUnplugSlowLoop();
  printf("\nworst HMI display frame: ~%.1f ms of SPI at %lu MHz (%llu bytes) on screen %s – drawn on core 1, the link never waits\n",
         g_worstFrameMs, (unsigned long)(DISPLAY_SPI_HZ / 1000000UL), (unsigned long long)g_worstFrameBytes, g_worstFrameScreen);
  printf("\n==== %d checks, %d failed ====\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
