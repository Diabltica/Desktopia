#include "ui_text.h"

namespace {
// Same order as enum UiText (shared/hmi_link/ui_model.h). Upper case = title (scale 3), lower case = subtitle.
const char* const kTexts[] = {
  "",
  "DESKTOPIA",
  "starting...",
  "SENSOR BYPASS",
  "NO SENSOR",
  "height unknown",
  "LIMIT",
  "max height",
  "min height",
  "BYPASS",
  "presets off",
  "NO PRESET",
  "hold to save",
  "not saved",
  "SAVE ERROR",
  "EEPROM",
  "SENSOR LOST",
  "ToF not answering",
  "WRONG WAY",
  "check DIR / SIGN",
  "STALLED",
  "desk not moving",
  "TIMEOUT",
  "move took too long",
  "WATCHDOG",
  "loop was stuck",
  "NO LINK",
  "check the cable",
  "OVERCURRENT",
  "motor current high",
  "NO CURRENT SENSE",
  "INA226 not answering",
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == (unsigned)UiText::Count, "one string per UiText entry");
}  // namespace

const char* uiText(UiText t) {
  unsigned i = (unsigned)t;
  return i < (unsigned)UiText::Count ? kTexts[i] : "";
}
