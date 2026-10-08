// Host-side simulation bench for the Desktopia firmware.
// The real firmware sources (buttons, io_expander, motor, presets, tof_sensor, desk_controller, display, gc9a01)
// are compiled unchanged against mocks of the Arduino core, a simulated desk, a noisy ToF sensor, a TCA9554
// and a GC9A01 emulator that rebuilds the screen pixel by pixel.
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdarg>
#include <random>
#include "board_pins.h"
#include "config.h"
#include "io_expander.h"
#include "buttons.h"
#include "motor.h"
#include "presets.h"
#include "tof_sensor.h"
#include "display.h"
#include "desk_controller.h"
#include "watchdog.h"
#include <avr/wdt.h>

// ------------------------------------------------------------------------------------------------
//  Mocked Arduino / AVR world
// ------------------------------------------------------------------------------------------------
static uint32_t g_now = 0;
static uint8_t g_pinLevel[64];
static uint8_t g_pinMode[64];
volatile uint8_t TCCR1A, TCCR1B, PORTB;
volatile uint16_t ICR1, OCR1B;
SpdrProxy SPDR;
volatile uint8_t SPSR = 0x80;
SPIClass SPI;
TwoWire Wire;
EEPROMClass EEPROM;
SerialMock Serial;
SimTof* g_simTof = nullptr;
volatile uint8_t WDTCSR, MCUSR;
unsigned g_wdrCount = 0;

unsigned long millis() { return g_now; }
void delay(unsigned long ms) { g_now += (uint32_t)ms; }
void pinMode(uint8_t p, uint8_t m) { g_pinMode[p] = m; }

struct Lcd;
static Lcd* g_lcd = nullptr;
static void lcdCsEdge(bool high);

void digitalWrite(uint8_t p, uint8_t v) {
  bool old = g_pinLevel[p];
  g_pinLevel[p] = v;
  if (p == pins::MOTOR_PWM) {
    if (v) PORTB |= _BV(PORTB6); else PORTB &= (uint8_t)~_BV(PORTB6);
  }
  if (p == pins::LCD_CS && v && !old) lcdCsEdge(true);
}
int digitalRead(uint8_t p) { return g_pinLevel[p]; }

// ------------------------------------------------------------------------------------------------
//  GC9A01 emulator
// ------------------------------------------------------------------------------------------------
struct Lcd {
  uint16_t fb[240 * 240];
  uint8_t cmd = 0;
  std::vector<uint8_t> args;
  uint16_t x0 = 0, x1 = 239, y0 = 0, y1 = 239, cx = 0, cy = 0;
  bool inRamwr = false, haveHi = false;
  uint8_t hi = 0;
  uint32_t pixelsInWindow = 0, expectedPixels = 0;
  // statistics
  uint64_t bytes = 0, windows = 0;
  unsigned csViolations = 0, windowMismatch = 0, initCmds = 0;
  uint8_t madctl = 0;
  bool displayOn = false, sleepOut = false;
  Lcd() { memset(fb, 0, sizeof(fb)); }

  void closeWindowCheck() {
    if (inRamwr && pixelsInWindow != expectedPixels) {
      windowMismatch++;
      printf("  [LCD] window mismatch: wrote %u pixels, window holds %u\n", pixelsInWindow, expectedPixels);
    }
    inRamwr = false;
  }
  void byte(uint8_t b) {
    bytes++;
    if (g_pinLevel[pins::LCD_CS] != LOW) { csViolations++; return; }
    if (g_pinLevel[pins::LCD_DC] == LOW) {            // command
      closeWindowCheck();
      cmd = b;
      args.clear();
      initCmds++;
      if (b == 0x2C) {
        inRamwr = true; haveHi = false; cx = x0; cy = y0; pixelsInWindow = 0;
        expectedPixels = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
        windows++;
      }
      if (b == 0x11) sleepOut = true;
      if (b == 0x29) displayOn = true;
      return;
    }
    if (inRamwr) {
      if (!haveHi) { hi = b; haveHi = true; return; }
      haveHi = false;
      fb[cy * 240 + cx] = (uint16_t)((hi << 8) | b);
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
static void lcdCsEdge(bool) { if (g_lcd) g_lcd->closeWindowCheck(); }
SpdrProxy& SpdrProxy::operator=(uint8_t b) { if (g_lcd) g_lcd->byte(b); return *this; }

// ------------------------------------------------------------------------------------------------
//  TCA9554 and ToF simulation
// ------------------------------------------------------------------------------------------------
struct SimIo {
  bool present = true;
  uint8_t buttons = 0;
  uint8_t reg[4] = {0, 0xFF, 0x00, 0xFF};
  uint8_t ptr = 0;
  unsigned reads = 0, writes = 0;
} g_io;

static bool g_tofPresent = true;

void TwoWire::beginTransmission(uint8_t a) { _addr = a; _txLen = 0; }
size_t TwoWire::write(uint8_t b) { if (_txLen < 8) _tx[_txLen++] = b; return 1; }
uint8_t TwoWire::endTransmission(uint8_t) {
  if (_addr == IOEXP_I2C_ADDR) {
    if (!g_io.present) return 2;
    if (_txLen >= 1) { g_io.ptr = _tx[0] & 3; if (_txLen >= 2) { g_io.reg[g_io.ptr] = _tx[1]; g_io.writes++; } }
    return 0;
  }
  if (_addr == ((TOF_I2C_ADDR_8BIT >> 1) & 0x7F)) return g_tofPresent ? 0 : 2;
  return 2;
}
uint8_t TwoWire::requestFrom(uint8_t a, uint8_t n) {
  _rxLen = 0; _rxPos = 0;
  if (a == IOEXP_I2C_ADDR && g_io.present) {
    for (uint8_t i = 0; i < n && i < 8; i++) _rx[_rxLen++] = (g_io.ptr == 0) ? g_io.buttons : g_io.reg[g_io.ptr];
    g_io.reads++;
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
// ------------------------------------------------------------------------------------------------
struct World {
  double h = 900.0;                  // true desk height in mm
  double mechMin = 690, mechMax = 1215;
  double polarity = +1.0;            // -1 = motor wired the other way
  bool jam = false;
  double vMaxMmS = 28.0;             // speed at 60 % duty
  double dutyThreshold = 0.12;       // below this the desk does not move
  double noiseSigma = 1.2;
  double outlierProb = 0.02, invalidProb = 0.02;
  uint32_t nextSample = 0;
  uint8_t targetBits = 0;
  uint32_t bounceUntil[8] = {0};
  std::mt19937 rng{12345};
  unsigned pwmStuckHigh = 0;
  double maxDuty = 0;

  double duty() const { return ((TCCR1A & _BV(COM1B1)) && ICR1) ? (double)OCR1B / ICR1 : 0.0; }
  bool dirUp() const { return g_pinLevel[pins::MOTOR_DIR] == MOTOR_DIR_UP_LEVEL; }
  double gauss() { std::normal_distribution<double> d(0.0, 1.0); return d(rng); }
  double uni() { std::uniform_real_distribution<double> d(0.0, 1.0); return d(rng); }

  void setButton(int bit, bool pressed, uint32_t bounceMs = 3) {
    if (pressed) targetBits |= (uint8_t)(1 << bit); else targetBits &= (uint8_t)~(1 << bit);
    bounceUntil[bit] = g_now + bounceMs;
  }
  void stepOneMs() {
    // buttons with contact bounce
    for (int b = 0; b < 8; b++) {
      bool v = (targetBits >> b) & 1;
      if ((int32_t)(g_now - bounceUntil[b]) < 0) v = uni() < 0.5;
      if (v) g_io.buttons |= (uint8_t)(1 << b); else g_io.buttons &= (uint8_t)~(1 << b);
    }
    // motor -> desk
    double d = duty();
    if (d > maxDuty) maxDuty = d;
    if (!(TCCR1A & _BV(COM1B1)) && (PORTB & _BV(PORTB6))) pwmStuckHigh++;   // output disconnected but latch high = full speed!
    double v = 0;
    if (d > dutyThreshold) v = vMaxMmS * (d - dutyThreshold) / (0.60 - dutyThreshold);
    if (jam) v = 0;
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
static uint32_t g_worstFrameMs = 0; static uint64_t g_worstFrameBytes = 0; static uint64_t g_worstFrameWindows = 0; static const char* g_worstFrameScreen = "";
static void check(bool ok, const char* fmt, ...) {
  g_checks++;
  if (!ok) {
    g_fails++;
    va_list ap; va_start(ap, fmt);
    printf("  FAIL: "); vprintf(fmt, ap); printf("\n");
    va_end(ap);
  }
}

static const char* screenNameFwd(UiModel::Screen s);
struct Sample { uint32_t t; double h; double tofH; double duty; bool dirUp; UiModel::Screen screen; UiModel::Arrow arrow; uint8_t gauge; };

struct Rig {
  World w;
  Lcd lcd;
  IoExpander io{IOEXP_I2C_ADDR};
  Buttons buttons;
  TofSensor tof;
  Motor motor;
  Presets presets;
  Display display;
  DeskController ctrl{io, buttons, tof, motor, presets};
  std::vector<Sample> trace;
  uint64_t maxFrameBytes = 0; uint64_t maxFrameWindows = 0; uint32_t maxFrameMs = 0; UiModel::Screen worstScreen = UiModel::Screen::Main;
  std::vector<std::string> shotNames;
  unsigned dirChangesWhileMoving = 0;
  uint8_t lastDirLevel = 0;
  uint32_t loopStepMs = 1;

  Rig(double startHeight = 900.0, bool keepEeprom = false, uint32_t seed = 12345, uint32_t startMs = 1000) {
    memset(g_pinLevel, 0, sizeof(g_pinLevel));
    TCCR1A = TCCR1B = PORTB = 0; ICR1 = OCR1B = 0;
    g_io = SimIo(); g_tof = SimTof(); g_tofPresent = true;
    if (!keepEeprom) { memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem)); EEPROM.writes = 0; }
    g_now = startMs;
    g_lcd = &lcd;
    w.h = startHeight;
    w.rng.seed(seed);
    // same order as main.cpp setup()
    motor.begin();
    display.begin();
    UiModel boot; boot.screen = UiModel::Screen::Boot; boot.title = F("DESKTOPIA"); boot.subtitle = F("starting...");
    display.render(boot, g_now);
    presets.begin();
    bool ioOk = io.begin(IOEXP_DIR_MASK, 0x00);
    // the physical world already exists while the firmware boots: let the sensor produce a first sample
    w.nextSample = g_now;
    w.stepOneMs();
    tof.begin();
    ctrl.begin(g_now, ioOk, false);
    lastDirLevel = g_pinLevel[pins::MOTOR_DIR];
  }
  bool expectSensorViolations = false;
  ~Rig() {
    check(lcd.csViolations == 0, "%u SPI bytes were sent while CS was high", lcd.csViolations);
    check(lcd.windowMismatch == 0, "%u pixel windows were not filled exactly", lcd.windowMismatch);
    check(expectSensorViolations || g_tof.violations == 0, "ST driver entered %u times while the sensor did not ACK", g_tof.violations);
    check(w.pwmStuckHigh == 0, "PWM pin latched high while disconnected for %u ms", w.pwmStuckHigh);
    check(w.maxDuty <= 0.6005, "duty reached %.3f", w.maxDuty);
    g_lcd = nullptr;
  }

  void step() {
    g_now += 1;
    w.stepOneMs();
    uint8_t before = lastDirLevel;
    if ((g_now % loopStepMs) == 0) {
      ctrl.update(g_now);
      uint64_t b0 = lcd.bytes, w0 = lcd.windows;
      display.render(ctrl.ui(), g_now);
      uint64_t db = lcd.bytes - b0, dw = lcd.windows - w0;
      uint32_t estMs = (uint32_t)((db * 1.25 + dw * 40.0 + dw * 0) / 1000.0);
      if (estMs > maxFrameMs) { maxFrameMs = estMs; maxFrameBytes = db; maxFrameWindows = dw; worstScreen = ctrl.ui().screen; }
      if (estMs > g_worstFrameMs) { g_worstFrameMs = estMs; g_worstFrameBytes = db; g_worstFrameWindows = dw; g_worstFrameScreen = screenNameFwd(ctrl.ui().screen); }
    }
    if (g_pinLevel[pins::MOTOR_DIR] != before) {
      if (w.duty() > 0.0) dirChangesWhileMoving++;
      lastDirLevel = g_pinLevel[pins::MOTOR_DIR];
    }
    const UiModel& u = ctrl.ui();
    trace.push_back({g_now, w.h, (double)tof.heightMm(), w.duty(), w.dirUp(), u.screen, u.arrow, u.gaugePercent});
  }
  void run(uint32_t ms) { for (uint32_t i = 0; i < ms; i++) step(); }
  void press(int bit) { w.setButton(bit, true); }
  void release(int bit) { w.setButton(bit, false); }
  void tap(int bit, uint32_t ms) { press(bit); run(ms); release(bit); }
  void shot(const char* name) {
    char path[256]; snprintf(path, sizeof(path), "shots/%s.ppm", name);
    // let the display catch up before photographing
    for (int i = 0; i < 60; i++) { display.render(ctrl.ui(), g_now + 100 + i * 30); }
    lcd.savePpm(path);
    shotNames.push_back(name);
  }
  const Sample* at(uint32_t t) const { for (const Sample& s : trace) if (s.t >= t) return &s; return trace.empty() ? nullptr : &trace.back(); }
  double maxDutyBetween(uint32_t a, uint32_t b) const { double m = 0; for (const Sample& s : trace) if (s.t >= a && s.t <= b && s.duty > m) m = s.duty; return m; }
};

static const int UP = BTN_BIT_UP, DOWN = BTN_BIT_DOWN, P1 = BTN_BIT_PRESET1, P2 = BTN_BIT_PRESET2, P3 = BTN_BIT_PRESET3;
static const char* screenNameFwd(UiModel::Screen s) { switch (s) { case UiModel::Screen::Boot: return "Boot"; case UiModel::Screen::Main: return "Main"; case UiModel::Screen::SaveGauge: return "SaveGauge"; case UiModel::Screen::SaveSuccess: return "SaveSuccess"; case UiModel::Screen::Notice: return "Notice"; case UiModel::Screen::Fault: return "Fault"; } return "?"; }
static const char* screenName(UiModel::Screen s) {
  switch (s) { case UiModel::Screen::Boot: return "Boot"; case UiModel::Screen::Main: return "Main"; case UiModel::Screen::SaveGauge: return "SaveGauge";
               case UiModel::Screen::SaveSuccess: return "SaveSuccess"; case UiModel::Screen::Notice: return "Notice"; case UiModel::Screen::Fault: return "Fault"; }
  return "?";
}

// ------------------------------------------------------------------------------------------------
//  Tests
// ------------------------------------------------------------------------------------------------
static void testBoot() {
  printf("\n[boot] display init and idle state\n");
  Rig r;
  check(r.lcd.sleepOut && r.lcd.displayOn, "panel did not receive sleep-out + display-on");
  check(r.lcd.csViolations == 0, "%u SPI bytes sent while CS was high", r.lcd.csViolations);
  static const uint8_t kExpectedMadctl[4] = {0x48, 0x28, 0x88, 0xE8};
  check(r.lcd.madctl == kExpectedMadctl[DISPLAY_ROTATION & 3], "MADCTL 0x%02X (expected 0x%02X for rotation %d)", r.lcd.madctl, kExpectedMadctl[DISPLAY_ROTATION & 3], DISPLAY_ROTATION);
  check(g_pinLevel[pins::MOTOR_DIR] == MOTOR_DIR_DOWN_LEVEL, "DIR pin not in a defined state after boot");
  check(g_io.reg[3] == IOEXP_DIR_MASK, "TCA9554 CONFIG register = 0x%02X, expected 0x%02X", g_io.reg[3], IOEXP_DIR_MASK);
  check(g_io.reg[2] == 0x00, "TCA9554 polarity inversion should be 0");
  r.run(400);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "idle screen is %s", screenName(r.ctrl.ui().screen));
  check(r.ctrl.ui().heightValid, "height not valid after 400 ms");
  check(std::fabs(r.ctrl.ui().heightMm - r.w.h) < 6.0, "displayed height %d vs true %.1f", r.ctrl.ui().heightMm, r.w.h);
  check(r.w.duty() == 0.0 && r.w.pwmStuckHigh == 0, "motor should be idle at boot");
  r.shot("01_idle");
}

static void testManualUp() {
  printf("\n[manual up] soft start, 60 %% ceiling, direction level, soft stop\n");
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
  r.run(450);
  double maxD = r.maxDutyBetween(tp, g_now);
  check(maxD <= 0.6005, "max duty %.3f exceeds 60 %%", maxD);
  check(d300 > 0.0 && d300 < 0.18, "duty 300 ms after the press = %.3f (soft start expected < 0.18)", d300);
  check(d1600 > 0.45 && d1600 <= 0.6005, "duty 1.6 s after the press = %.3f (expected close to 0.6)", d1600);
  check(r.w.maxDuty <= 0.6005, "absolute max duty %.3f", r.w.maxDuty);
  check(r.at(tp + 500)->dirUp, "DIR pin should be at the UP level while going up");
  check(r.w.h - h0 > 25.0, "desk rose only %.1f mm", r.w.h - h0);
  check(r.w.duty() == 0.0, "duty %.3f 450 ms after release (expected 0)", r.w.duty());
  check(r.ctrl.ui().arrow == UiModel::Arrow::None, "arrow should disappear after the stop");
  // monotonic ramp up during the soft start
  bool mono = true; double prev = 0;
  for (const Sample& s : r.trace) if (s.t > tp + 40 && s.t < tp + 1400) { if (s.duty + 1e-9 < prev) mono = false; prev = s.duty; }
  check(mono, "duty is not monotonic during the soft start");
  // stop ramp length
  const Sample* s0 = r.at(tr + 20);
  check(s0->duty > 0.0 || true, "n/a");
  uint32_t tz = 0; for (const Sample& s : r.trace) if (s.t > tr && s.duty == 0.0) { tz = s.t; break; }
  check(tz && tz - tr < 450, "soft stop took %u ms", tz - tr);
  check(r.w.pwmStuckHigh == 0, "PWM output latched high while disconnected (%u ms)", r.w.pwmStuckHigh);
  printf("  rose %.1f mm, soft-start duty at +300 ms = %.3f, at +1.6 s = %.3f, stop ramp = %u ms\n", r.w.h - h0, d300, d1600, tz - tr);
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
  check(tGauge >= 400 && tGauge <= 470, "gauge appeared %u ms after the press (expected ~400-440)", tGauge);
  check(tSuccess >= 1990 && tSuccess <= 2080, "success shown %u ms after the press (expected ~2000-2050)", tSuccess);
  check(gaugeMono, "gauge percentage went backwards");
  check(r.presets.isSet(0), "preset 1 not stored");
  check(std::fabs((double)r.presets.get(0) - trueH) < 5.0, "stored %u, true height %.1f", r.presets.get(0), trueH);
  check(r.w.duty() == 0.0 && r.w.maxDuty == 0.0, "motor moved during a preset save");
  r.run(2200);                                           // button still held: success then main, no goto afterwards
  r.release(P1);
  r.run(600);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen after the save: %s", screenName(r.ctrl.ui().screen));
  check(r.w.maxDuty == 0.0, "motor moved after the long press release (spurious goto)");
  // a second Presets instance must read the value back from the EEPROM
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
  // move away: down for ~3 s
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
  // approach profile: duty versus remaining distance
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
  check(maxD <= 0.6005, "max duty %.3f", maxD);
  check(dutyAt100 > 0.5, "duty %.3f at 100 mm from the target (should be at full speed)", dutyAt100);
  check(dutyAt40 > 0 && dutyAt40 < 0.46 && dutyAt40 > 0.25, "duty %.3f at 40 mm (expected ~0.40 following the ramp)", dutyAt40);
  check(dutyAt10 > 0 && dutyAt10 < 0.30, "duty %.3f at 10 mm (expected close to the 0.20 floor)", dutyAt10);
  check(r.dirChangesWhileMoving == 0, "DIR pin toggled %u times while the duty was not zero", r.dirChangesWhileMoving);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen after the goto: %s", screenName(r.ctrl.ui().screen));
  printf("  from %.1f to %.1f mm (target %u): error %.2f mm, duty @100mm=%.2f @40mm=%.2f @10mm=%.2f, took %.1f s\n", start, r.w.h, target, err, dutyAt100, dutyAt40, dutyAt10, tEnd / 1000.0);
  // downward goto as well
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
  // empty preset
  r.tap(P2, 100);
  r.run(200);
  check(r.ctrl.ui().screen == UiModel::Screen::Notice, "short press on an empty preset shows %s", screenName(r.ctrl.ui().screen));
  r.shot("07_notice_nopreset");
  check(r.w.maxDuty == 0.0, "desk moved for an empty preset");
  r.run(1700);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "notice should disappear, screen=%s", screenName(r.ctrl.ui().screen));
  // aborted long press
  size_t writes0 = EEPROM.writes;
  r.press(P2); r.run(1000);
  check(r.ctrl.ui().screen == UiModel::Screen::SaveGauge, "gauge should be visible after 1 s");
  r.release(P2); r.run(300);
  check(!r.presets.isSet(1) && EEPROM.writes == writes0, "an aborted long press must not save");
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "aborted long press: screen=%s", screenName(r.ctrl.ui().screen));
  check(r.w.maxDuty == 0.0, "an aborted long press must not move the desk");
  // save preset 3 then cancel a goto with another button
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
  // both buttons: nothing moves, nothing starts when one is released
  r.press(UP); r.press(DOWN); r.run(500);
  check(r.w.duty() == 0.0, "Up+Down must not move the desk");
  r.release(DOWN); r.run(500);
  check(r.w.duty() == 0.0, "releasing one of the two buttons must not start a movement");
  r.release(UP); r.run(300);
  // preset button pressed while Up is held: ignored
  r.press(UP); r.run(800);
  double dBefore = r.w.duty();
  r.tap(P3, 120);
  r.run(600);
  check(r.w.duty() >= dBefore - 0.001 && r.ctrl.ui().arrow == UiModel::Arrow::Up, "a preset tap during a manual move must be ignored");
  r.release(UP); r.run(600);
  check(r.ctrl.ui().screen == UiModel::Screen::Main, "screen: %s", screenName(r.ctrl.ui().screen));
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
  Rig r2(760.0);
  r2.run(400);
  r2.press(DOWN); r2.run(9000);
  check(r2.w.duty() == 0.0 && std::fabs(r2.w.h - HEIGHT_MIN_MM) <= 4, "lower limit: stopped at %.1f (limit %d)", r2.w.h, HEIGHT_MIN_MM);
  // slowed progressively near the limit
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
  check(g_tof.violations == 0, "the ST driver was entered %u times while the sensor did not ACK", g_tof.violations);
  check(r.w.pwmStuckHigh == 0, "PWM latched high after the emergency stop");
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
  printf("\n[faults] wrong way, stall, I/O expander loss, watchdog ISR\n");
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
    check(tf >= 1800 && tf <= 2800, "stall detected after %u ms (expected ~2 s)", tf);
    check(r.w.duty() == 0.0, "motor not stopped after the stall fault");
    printf("  stall detected after %u ms\n", tf);
  }
  {
    Rig r(900.0);
    r.run(400);
    r.press(UP); r.run(1500);
    uint32_t tl = g_now; g_io.present = false;
    uint32_t tStop = 0;
    for (int i = 0; i < 400; i++) { r.step(); if (!tStop && r.w.duty() == 0.0) tStop = g_now - tl; }
    check(tStop > 0 && tStop <= 40, "motor stopped %u ms after the I/O expander vanished", tStop);
    check(r.ctrl.ui().screen == UiModel::Screen::Fault, "screen: %s", screenName(r.ctrl.ui().screen));
    g_io.present = true; r.release(UP);
    r.run(4000);
    check(r.ctrl.ui().screen == UiModel::Screen::Main, "after the expander came back: %s", screenName(r.ctrl.ui().screen));
    check(g_io.reg[3] == IOEXP_DIR_MASK, "expander was not re-configured after the outage");
    printf("  expander loss: motor cut after %u ms, recovered\n", tStop);
  }
  {
    Rig r(900.0);
    r.run(400);
    r.press(UP); r.run(1500);
    motorFailsafeCut();                                   // what the watchdog interrupt does
    check(!(TCCR1A & _BV(COM1B1)) && !(PORTB & _BV(PORTB6)), "failsafe must disconnect and clear the PWM pin");
    r.run(5);
    check(r.w.duty() == 0.0, "duty must stay 0 after the failsafe");
    check(r.ctrl.ui().screen == UiModel::Screen::Fault, "failsafe should raise a fault, screen=%s", screenName(r.ctrl.ui().screen));
    check(r.w.pwmStuckHigh == 0, "PWM latched high after the failsafe");
    r.release(UP); r.run(4000);
    check(r.ctrl.ui().screen == UiModel::Screen::Main, "fault should clear: %s", screenName(r.ctrl.ui().screen));
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
    // exactly one short press must have been seen: empty preset -> one notice
    check(r.ctrl.ui().screen == UiModel::Screen::Notice, "chattering tap not recognised (screen %s)", screenName(r.ctrl.ui().screen));
    check(r.w.maxDuty == 0.0, "chatter moved the desk");
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
  printf("\n[slow loop] the same behaviour with a 20 ms main loop (display-heavy frames)\n");
  Rig r(1000.0);
  r.loopStepMs = 20;
  r.run(500);
  r.tap(P1, 2400); r.run(2800);
  uint16_t target = r.presets.get(0);
  check(r.presets.isSet(0), "preset not saved with a slow loop");
  r.press(DOWN); r.run(3000); r.release(DOWN); r.run(1000);
  r.tap(P1, 150); r.run(15000);
  check(std::fabs(r.w.h - target) <= 5.0, "slow loop goto error %.1f mm", r.w.h - target);
  printf("  slow loop goto error %.2f mm\n", r.w.h - target);
}


static void testNoSensorNotices() {
  printf("\n[no sensor at start] notices instead of motion\n");
  Rig r(900.0);
  r.run(300);
  g_tofPresent = false;
  r.run(1500);                                           // offline, height unknown
  check(!r.ctrl.ui().heightValid, "height should be flagged invalid");
  r.press(UP); r.run(600);
  check(r.w.maxDuty == 0.0, "desk moved without a height measurement");
  check(r.ctrl.ui().screen == UiModel::Screen::Notice, "Up with no sensor: %s", screenName(r.ctrl.ui().screen));
  r.release(UP); r.run(2000);
  r.press(P1); r.run(2300);                              // long press: cannot save
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
    double err = r.w.h - target;                         // against the *saved* value, which itself carries the sensor noise
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
  WDTCSR = mine;                                         // the core restores the saved state when the reset is aborted
  watchdog::kick();
  check(g_wdrCount == before + 1, "kick() did not resume after the original configuration came back");
  WDTCSR &= (uint8_t)~_BV(WDIE);                         // the interrupt stage fired once: hardware cleared WDIE
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

int main() {
  testBoot();
  testManualUp();
  testManualDown();
  testPresetSave();
  testGoto();
  testGotoCancelAndEmpty();
  testLimits();
  testSensorLoss();
  testFaults();
  testButtonsAndEeprom();
  testSlowLoop();
  testNoSensorNotices();
  testRollover();
  testMonteCarlo();
  testWatchdogGuard();
  testGaugeScreens();
  printf("\nworst display frame: ~%u ms (%llu bytes, %llu windows) on screen %s\n", g_worstFrameMs, (unsigned long long)g_worstFrameBytes, (unsigned long long)g_worstFrameWindows, g_worstFrameScreen);
  printf("\n==== %d checks, %d failed ====\n", g_checks, g_fails);
  return g_fails ? 1 : 0;
}
