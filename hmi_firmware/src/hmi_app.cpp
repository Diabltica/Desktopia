#include "hmi_app.h"
#include "hmi_hw.h"
#include "hmi_pins.h"

namespace {
// Button pins in the order of the frame bits hmi::kBitUp .. hmi::kBitPreset3
constexpr uint8_t kButtonPins[5] = {hmipins::BTN_UP, hmipins::BTN_DOWN, hmipins::BTN_PRESET1, hmipins::BTN_PRESET2,
                                    hmipins::BTN_PRESET3};

UiModel splashModel() {                          // before the first frame: the motherboard may still be booting
  UiModel m;
  m.screen = UiModel::Screen::Boot;
  m.title = UiText::Desktopia;
  m.subtitle = UiText::Starting;
  return m;
}

UiModel noLinkModel() {                          // powered (so the cord is there) but nothing valid comes through
  UiModel m;
  m.screen = UiModel::Screen::Fault;
  m.title = UiText::NoLink;
  m.subtitle = UiText::CheckCable;
  return m;
}
}  // namespace

// ---------------------------------------------------------------------------------------------
//  Core 0: link + buttons
// ---------------------------------------------------------------------------------------------
void HmiApp::beginLink(uint32_t nowMs) {
  pinMode(hmipins::LINK_DE, OUTPUT);
  digitalWrite(hmipins::LINK_DE, LOW);           // DE and /RE tied: LOW = listen
  for (uint8_t i = 0; i < 5; i++) pinMode(kButtonPins[i], INPUT_PULLUP);
  delayMicroseconds(50);                         // pull-ups charge the button lines
  for (uint8_t i = 0; i < 5; i++) {              // a button held while the board powers up is reported as pressed
    if (digitalRead(kButtonPins[i]) == LOW) _stable |= (uint8_t)(1u << i);   // from the very first answer: never
  }                                              // a fake release the motherboard could take for a new press
  hmiHwSetLinkInverted(false);
  _lastFrame = nowMs;
  _lastSwap = nowMs;
  publish(splashModel());
}

void HmiApp::sampleButtons(uint32_t nowMs) {
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t bit = (uint8_t)(1u << i);
    bool pressed = digitalRead(kButtonPins[i]) == LOW;
    if (pressed == ((_stable & bit) != 0)) {
      _pending &= (uint8_t)~bit;                 // back to the reported level: a bounce, not a change
    } else if (!(_pending & bit)) {
      _pending |= bit;
      _since[i] = nowMs;
    } else if ((uint32_t)(nowMs - _since[i]) >= HMI_DEBOUNCE_MS) {
      _stable ^= bit;
      _pending &= (uint8_t)~bit;
    }
  }
}

void HmiApp::answer(uint8_t seq) {
  uint8_t payload[hmi::kButtonsPayloadSize] = {seq, _stable, hmi::kVersion};
  uint8_t frame[hmi::kMaxFrame];
  uint8_t n = hmi::encodeFrame(hmi::kTypeButtons, payload, sizeof(payload), frame);
  delayMicroseconds(HMI_TURNAROUND_US);
  digitalWrite(hmipins::LINK_DE, HIGH);
  _port.write(frame, n);
  _port.flush();                                 // returns once the last stop bit is out
  digitalWrite(hmipins::LINK_DE, LOW);
}

void HmiApp::updateLink(uint32_t nowMs) {
  sampleButtons(nowMs);

  while (_port.available()) {
    if (!_rx.feed((uint8_t)_port.read())) continue;
    if (_rx.type() != hmi::kTypeState || _rx.length() < hmi::kStatePayloadSize) continue;
    UiModel m;
    if (!hmi::unpackUi(_rx.payload() + 1, m)) continue;
    answer(_rx.payload()[0]);
    _received = m;
    _everLinked = true;
    _lastFrame = nowMs;
    _framesOk++;
  }

  // Nothing valid for a while: maybe A and B are swapped (standard telephone cords are reversed).
  // Inverting both UART pads makes a swapped pair look straight. Only the master talks first, so trying
  // the wrong polarity never puts anything on the bus.
  if ((uint32_t)(nowMs - _lastFrame) >= HMI_POLARITY_SWAP_MS && (uint32_t)(nowMs - _lastSwap) >= HMI_POLARITY_SWAP_MS) {
    _inverted = !_inverted;
    hmiHwSetLinkInverted(_inverted);
    _lastSwap = nowMs;
    while (_port.available()) _port.read();      // garbage received with the wrong polarity
    _rx.reset();
  }

  if (!_everLinked) publish(splashModel());
  else if ((uint32_t)(nowMs - _lastFrame) >= HMI_NO_LINK_MS) publish(noLinkModel());
  else publish(_received);
}

void HmiApp::publish(const UiModel& m) {
  hmiHwLock();
  _shared = m;
  hmiHwUnlock();
}

// ---------------------------------------------------------------------------------------------
//  Core 1: screen
// ---------------------------------------------------------------------------------------------
void HmiApp::beginDisplay() { _display.begin(); }

void HmiApp::updateDisplay(uint32_t nowMs) {
  hmiHwLock();
  UiModel m = _shared;
  hmiHwUnlock();
  _display.render(m, nowMs);
}
