# Desktopia – standing desk firmware

Firmware for the "Desktopia – motherboard" board: ATmega32U4 (16 MHz, 5 V), Arduino framework, PlatformIO.
Drives a motor (DIR + PWM), reads the height with a VL53L1CB, reads 5 buttons through a TCA9554 and shows
the state on a round GC9A01 screen.

![UI preview](docs/ui_preview.png)

*(Preview produced by the real display code, rendered pixel by pixel by the emulator in `sim/`.)*

## Operation

| Action | Result |
|---|---|
| **Up / Down** held | the desk moves as long as the button is held; ramped start (*soft start*), green / orange arrow on screen |
| **Preset 1-3, short press** (< 400 ms) | the desk goes to the saved height, slowing down progressively over the last 80 mm |
| **Preset 1-3, long press** | after 400 ms a circular gauge fills up; at 2 s: saved to EEPROM, then *SUCCESS* screen |
| release before the gauge is full | nothing is saved, nothing moves |
| any button during an automatic move | stop (300 ms ramp), the press is ignored until released |
| Up + Down together | nothing moves until both are released |

PWM capped at **40 %**, direction **1 = up / 0 = down** (change in `config.h`).

## File layout

```
include/board_pins.h     pinout extracted from the schematic (Arduino numbers of the "leonardo" variant)
include/config.h         ALL settings, overridable with -DNAME=value in platformio.ini
src/main.cpp             setup() / loop()
src/io_expander.*        TCA9554 driver
src/buttons.*            debounce + short / long press
src/tof_sensor.*         VL53L1CB via the ST driver (STM32duino VL53L1X lib) + median filter
src/motor.*              DIR + Timer1 PWM, soft start / soft stop, direction-change protection
src/presets.*            internal EEPROM (magic + version + CRC-8)
src/gc9a01.*             compact screen driver (init taken from the Adafruit lib, 5×7 font)
src/display.*            UI (height, arrow, gauge, messages) – only redraws what changes
src/desk_controller.*    state machine, speed profile, safety checks
src/watchdog.*           hardware watchdog: the interrupt cuts the PWM, the reset follows
src/ui_model.h           what the controller passes to the screen
sim/                     PC simulation bench (optional, see below)
```

## Pinout (extracted from `desktopia.kicad_sch`)

| Function | Schematic net | MCU | Arduino |
|---|---|---|---|
| Motor DIR | `Mdir` (J5-1) | PC6 | D5 |
| Motor PWM | `Mpwm` (J5-2) | PB6 / OC1B | D10 |
| Screen CS / DC / RST | `CS_MBside` / `DC_MBside` / `RST_MBside` | PB0 / PB4 / PB5 | D17 / D8 / D9 |
| Screen SCK / MOSI | `SCK` / `MOSI` | PB1 / PB2 | hardware SPI |
| I2C SDA / SCL | `SDA` / `SCL` | PD1 / PD0 | Wire |
| TCA9554 /INT | `HMI_INT` | PE6 | D7 (internal pull-up, unused) |
| ToF GPIO1 | `GPIO1_VL53` | PD2 | D0 (unused) |
| Motor current | `Imotor` | PF0 | ignored (sensor not fitted) |

I2C: TCA9554 at `0x20` (A0-A2 to ground), VL53L1CB at `0x29` (`0x52` in 8-bit notation).
Buttons: SW1…SW5 = P0…P4 of the TCA9554, **active high** (switch to +5VA, 10 kΩ pull-down on the motherboard side).
Default mapping: SW1 = Up, SW2 = Down, SW3-SW5 = Preset 1-3 (`BTN_BIT_*` in `config.h`).

## Build and flash

```bash
pio run                                   # build (env desktopia)
pio run -t upload                         # USB, Caterina bootloader (double-press RESET if the port does not show up)
pio run -e desktopia_debug -t upload      # same + logs on the USB serial port (115200 baud, TX only)
pio run -e desktopia_motor_test -t upload # motor bench test, height sensor bypassed (see below)
pio device monitor
pio run -e desktopia_isp -t bootloader    # blank chip: burns bootloader + fuses via J2 (USBasp)
```

The `desktopia_isp` environment (USBasp programmer on J2) is used for the first flash and for recovery; if the bootloader is gone, burn it again with `-t bootloader`.

Only external dependency: the ST driver (`stm32duino/VL53L1X` 2.0.1, ULD), pinned to its tag in `platformio.ini`.
Size: ≈ 23.3 KB of flash (≈ 81 % of the 28 KB available next to the bootloader), ≈ 0.75 KB of RAM.

## Main settings (`include/config.h`)

| Parameter | Default | Purpose |
|---|---|---|
| `MOTOR_DIR_UP_LEVEL` / `MOTOR_DIR_DOWN_LEVEL` | HIGH / LOW | DIR level to go up / down |
| `MOTOR_PWM_MAX_PERCENT` | 40 | absolute duty-cycle ceiling |
| `MOTOR_PWM_MIN_PERCENT` | 20 | smallest PWM that still moves the desk (end of approach) – **to be tuned** |
| `MOTOR_PWM_FREQ_HZ` | 8000 | PWM frequency, 20 kHz max (MD10C R3); lower = cooler driver, more whine (see below) |
| `MOTOR_SOFT_START_MS` / `MOTOR_SOFT_STOP_MS` | 1500 / 300 | 0→max and max→0 ramps |
| `APPROACH_DISTANCE_MM` / `TARGET_TOLERANCE_MM` | 80 / 2 | start of slowdown / stop within ± |
| `TOF_HEIGHT_SIGN` / `TOF_HEIGHT_OFFSET_MM` | +1 / 0 | height = OFFSET + SIGN × measured distance |
| `HEIGHT_MIN_MM` / `HEIGHT_MAX_MM` | 700 / 1200 | software end stops: **must be set inside the real travel** |
| `PRESET_SHORT_PRESS_MAX_MS` / `PRESET_LONG_PRESS_MS` | 400 / 2000 | press thresholds |
| `DISPLAY_ROTATION` | 1 | screen orientation (0-3) |
| `TOF_BYPASS` | 0 | 1 = height sensor ignored for motor control, **bench test only** (see below) |

## Motor bench test without the sensor (`TOF_BYPASS`)

To test the motor commands (DIR, PWM, ramps) **without the mechanism coupled to the desk** and without needing a
working distance sensor, build with `-DTOF_BYPASS=1`. A ready-made environment does it, with the PWM limited to 25 %:

```bash
pio run -e desktopia_motor_test -t upload
```

Behaviour in this mode:

- The boot screen shows **SENSOR BYPASS** and the compiler prints a warning.
- **Up / Down** held: the motor runs at `MOTOR_PWM_MAX_PERCENT` (with soft start / soft stop) as long as the button is
  held, whatever the height. Releasing the button, or pressing both, stops it.
- **No end stops, no stall / wrong-way / sensor-loss detection**: only the 60 s move timeout and the watchdog remain.
- Presets are disabled (short and long press show *BYPASS – presets off*); the EEPROM is not touched.
- The ToF keeps running: if it is connected, its reading is still displayed (`---` otherwise), which helps check
  `TOF_HEIGHT_SIGN` by hand.

> ⚠️ Never use this firmware with the motor coupled to the desk: nothing stops the motor at the mechanical end stops.
> Flash the normal `desktopia` environment again once the test is done.

## Motor driver heating (Cytron MD10C R3)

Ratings: 13 A continuous, 30 A peak (10 s), PWM up to 20 kHz. The board has no heatsink and no current limit.

The driver's losses come from two sources:

- **Switching losses**: at every PWM edge the MOSFETs briefly carry the current and the voltage at the same time. They
  grow **linearly with the PWM frequency**. At 20 kHz (the board's maximum) they are the main source of heat, so the
  default is now **8 kHz** (≈ 2.5× fewer switching losses).
- **Conduction losses**: R<sub>DS(on)</sub> × I². They depend on the motor current, i.e. on the load (lifting the
  desk takes the same torque, so about the same current, whatever the duty). Only a shorter run time reduces them.

Firmware levers, in order of effect:

1. `MOTOR_PWM_FREQ_HZ`: 8000 by default. Still too hot → 4000–5000 (louder motor whine). Quiet but hot → do not go
   above 20000 (a `static_assert` refuses it).
2. `MOTOR_PWM_MAX_PERCENT`: counter-intuitively, **a higher duty heats the driver less** for the same lift. The current
   is set by the load, not by the duty, but a higher duty finishes the climb sooner, and at 100 % there is no switching at
   all. Keep it as high as the motor and the desired speed allow.
3. `MOTOR_SOFT_START_MS` / `APPROACH_DISTANCE_MM` / `MOTOR_PWM_MIN_PERCENT`: the soft start and the final approach run
   the motor slowly under full load; keep them short / the minimum duty just high enough.

Hardware levers: a stick-on heatsink on the MOSFETs and some air flow, and short, thick motor and supply wires.

**Measuring the current with a multimeter**: on a PWM drive, a DC ammeter shows an *average*.
In the **motor** wire it is the average motor current. In the **supply** wire it is about *duty × motor current*: at 40 %,
8 A on the supply side means ≈ 20 A in the motor, well above the 13 A continuous rating.

## First power-up (do it in this order)

1. **Motor decoupled** from the mechanism (or crank disengaged). First flash with a reduced PWM:
   `build_flags = -DMOTOR_PWM_MAX_PERCENT=25` in the environment being used.
2. The screen should show a plausible height that changes when a target is moved in front of the sensor.
   Otherwise: `---` = silent sensor (see *Hardware notes*), absurd value = adjust `TOF_HEIGHT_OFFSET_MM`.
3. Press **Up**: the motor must turn in the "up" direction **and** the displayed height must increase.
   A mismatch is detected after 15 mm of travel: **WRONG WAY** screen. Then fix
   `MOTOR_DIR_UP_LEVEL`/`MOTOR_DIR_DOWN_LEVEL` (motor reversed) or `TOF_HEIGHT_SIGN` (height reversed).
4. Set `HEIGHT_MIN_MM` / `HEIGHT_MAX_MM` **inside** the mechanical end stops (the firmware slows down before them).
5. Set `MOTOR_PWM_MAX_PERCENT` back to 40, couple the mechanism. If **STALLED** appears at the end of a preset approach,
   the minimum PWM is too low to get the desk moving: increase `MOTOR_PWM_MIN_PERCENT` by 3-5 points.
6. Test saving a preset (long press) then recalling it from another height, in both directions.

## Built-in protections

No current sensing on this board: everything relies on the ToF.

- Loss of the ToF (no valid measurement for 400 ms) or of the TCA9554 (buttons unreadable) → immediate stop, no ramp.
- Wrong direction (15 mm the opposite way), stall (3 mm of progress in 1.5 s while the PWM is above the minimum),
  movement longer than 60 s → immediate stop and fault screen; back to normal after 2.5 s if the cause is gone.
- Software end stops `HEIGHT_MIN/MAX_MM` with slowdown; a button held at the end stop does not restart the motor.
- The direction (DIR) only changes when the PWM is at 0, after 400 ms stopped.
- 500 ms watchdog: if the loop freezes, an interrupt cuts the PWM, then the MCU restarts.
  It steps aside when the Arduino core reprograms the watchdog for the automatic reset into the bootloader (USB upload).
- I2C bus with timeout (25 ms). The ST driver loops forever on a NACK: it is only called after a successful address ping.
- No pinch or obstacle detection other than the "no more progress" stall.

## Hardware notes from the schematic

1. **`Mpwm` and `Mdir` have no pull-down.** During a reset, the bootloader or an ISP flash, these lines float: depending on the
   motor driver, the desk may start moving. Add a pull-down (10 kΩ) on `Mpwm` (and `Mdir`), or check that the driver has one.
2. `HMI_INT` (open-drain output of the TCA9554) has no pull-up: the firmware enables the MCU's one, the line is not used
   (polling every 5 ms).
3. PB0 is the screen CS **and** the RX LED of the Leonardo core: never read the USB serial port (`Serial.read()`), the core
   would toggle CS. The logs (`desktopia_debug`) only use TX.
4. The screen receives the MCU's 5 V signals directly (no level shifting or series resistors): check in the module's
   datasheet that it accepts 5 V logic.

## Assumptions to confirm

- The screen is a **GC9A01 240×240** (7-pin connector: VCC, GND, SCK, MOSI, DC, CS, RST). Otherwise, only `gc9a01.cpp` changes.
- The sensor is a **VL53L1CB** (not "VL51"): ST describes it as a superset of the VL53L1X and the ULD driver is used as is.
  The ID read at startup is not enforced (it is shown in debug mode): to be confirmed on the board.
- Height and end stops are expressed in the quantity your setup measures (direction and offset adjustable, see above).

## What has been verified, and what has not

- **AVR build** with avr-gcc 7.3.0 (the version shipped with PlatformIO) and Arduino AVR core 1.8.6, `-Wall -Wextra` with no warnings.
- **PC simulation** (`sim/run.sh`, 407 checks passed) of the real sources against a simulated desk, a noisy ToF
  (outliers and invalid values included), a simulated TCA9554 with bounce and an emulated GC9A01 screen: PWM ramps and ceiling,
  DIR direction, presets (short / long press, EEPROM, gauge), recall accuracy (standard deviation ≈ 2.4 mm with 1-3 mm sensor noise),
  end stops, faults, `millis()` rollover, screen rendering.
- **Not tested**: on the real board. No trials with the motor driver, the VL53L1CB, or the screen module. PlatformIO itself could
  not be run in the working environment (toolchain download impossible): its `platformio.ini` file was
  validated by its parser and the code compiled with the same GCC 7.3.0 and Arduino AVR core 1.8.6.

## Simulation bench (optional)

```bash
cd sim && ./run.sh     # Linux / WSL, g++; writes emulated screenshots to sim/shots/ (PPM)
```
