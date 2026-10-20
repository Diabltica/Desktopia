# Desktopia – standing desk firmware

Two boards, two firmwares, linked by an RJ11 cord (project overview: [../README.md](../README.md)):

- **Motherboard** (this folder): ATmega32U4 (16 MHz, 5 V), Arduino framework, PlatformIO. Drives the motor (DIR + PWM),
  reads the height with a VL53L1CB, runs the state machine and decides what the screen shows.
- **HMI board** (`../hmi_firmware/`): RP2040, arduino-pico core. Draws the round GC9A01 screen, reads the 5 buttons and
  answers the motherboard over RS-485.
- **Shared** (`../shared/hmi_link/`): the link protocol and the screen model, compiled into both.

![UI preview](docs/ui_preview.png)

*(Preview produced by the real display code, rendered pixel by pixel by the emulator in `sim/`.)*

## Operation

| Action | Result |
|---|---|
| **Up / Down** held | the desk moves as long as the button is held; ramped start (*soft start*), green / orange arrow on screen |
| **Preset 1-3, short press** (< 400 ms) | the desk goes to the saved height, slowing down progressively over the last 80 mm |
| **Preset 1-3, long press** | after 400 ms a circular gauge fills up; at 2 s: saved to EEPROM, then *SUCCESS* screen |
| release before the gauge is full | nothing is saved, nothing moves |
| **Up + Down** (or any button) during an automatic move | stop (300 ms ramp); nothing starts again until every button is released |
| Up + Down together | nothing moves until both are released |
| RJ11 cord pulled out | the motor stops at once (≈ 40 ms); no message (the HMI board has no power without the cord) |

PWM capped at **60 %**, direction **1 = up / 0 = down** (change in `config.h`).

## File layout

```
firmware/                        motherboard (ATmega32U4)
  include/board_pins.h           pinout extracted from the schematic (Arduino numbers of the "leonardo" variant)
  include/config.h               ALL motherboard settings, overridable with -DNAME=value in platformio.ini
  src/main.cpp                   setup() / loop()
  src/hmi_link.*                 RS-485 master: screen content out, button levels in
  src/buttons.*                  debounce + short / long press
  src/tof_sensor.*               VL53L1CB via the ST driver (STM32duino VL53L1X lib) + median filter
  src/motor.*                    DIR + Timer1 PWM, soft start / soft stop, direction-change protection
  src/presets.*                  internal EEPROM (magic + version + CRC-8)
  src/desk_controller.*          state machine, speed profile, safety checks
  src/watchdog.*                 hardware watchdog: the interrupt cuts the PWM, the reset follows
  sim/                           PC simulation bench of BOTH boards (see below)
hmi_firmware/                    HMI board (RP2040)
  include/hmi_pins.h             pinout extracted from desktopia_HMI.kicad_sch
  include/hmi_config.h           HMI settings (link timing, debounce, screen)
  src/main.cpp                   core 0: link + buttons, core 1: screen; RP2040 watchdog
  src/hmi_app.*                  link slave, A/B polarity detection, NO LINK screen, hand-over to core 1
  src/display.*  src/gc9a01.*    UI and panel driver (moved from the motherboard, unchanged screens)
  src/ui_text.*                  the strings behind the text numbers sent by the motherboard
shared/hmi_link/                 hmi_protocol.* (frames, CRC, UiModel packing), ui_model.h
```

## The link (RS-485 over an RJ11 cord)

The 12-pin FFC is replaced by a **6-conductor RJ11 cord (6P6C, sometimes sold as "RJ12")**. The motherboard has an
RJ11 jack, the cord is soldered to the HMI board (it sits at 45° under the desk edge, see its README). Each board has an
RS-485 transceiver: SP485EEN (5 V) on the motherboard, SP3485EN (3.3 V) on the HMI board; same
pin-out, ±15 kV ESD on A/B, both JLCPCB *Basic* parts.

| Pin | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| Signal | +5V | GND | A | B | GND | +5V |

- **Why RS-485**: the data travel as a differential pair next to a 20 A PWM motor. The common-mode rejection and the
  ±7 V common-mode range of RS-485 make a 2 m cord a non-issue; plain UART levels would pick up the motor noise.
- **The pin-out is symmetric**. Standard telephone cords are *reversed* (pin 1 ↔ 6, 2 ↔ 5, 3 ↔ 4): with this pin-out a
  reversed cord can never swap +5V and GND, it only swaps A and B. The HMI board detects it by itself. When no valid frame
  has arrived for 150 ms, it inverts its UART pads (`gpio_set_inover` / `gpio_set_outover`); the link is up again
  within ~150 ms. Straight or reversed cords both work, with nothing to configure.
- 4-conductor cords (6P4C) do **not** work: they carry neither pin 1 nor pin 6.
- **Never plug the motherboard jack into a telephone wall socket** (ringing voltage).

**Protocol** (`shared/hmi_link/hmi_protocol.h`): 250 000 baud 8N1 (exact on both MCUs), half duplex, the motherboard is
the only master. Every 10 ms it sends a *State* frame (the screen content: screen, arrow, height, gauge, text numbers;
17 bytes). The HMI board answers within ~0.2 ms with a *Buttons* frame (debounced levels; 8 bytes). Frames:
`0xA5 | type | len | payload | CRC-16/CCITT`. A frame with a bad CRC is dropped; the next poll comes 10 ms later.

**When the link fails**:

| Situation | Motherboard | HMI board |
|---|---|---|
| cord pulled out | no answer for 40 ms → buttons unknown → motor stopped (≈ 40 ms) | unpowered |
| cord back in | link up after the HMI boot; buttons trusted again after 100 ms of stable link, and only once every button is released | splash screen, then the normal screen |
| data pair broken, power OK | same as cord pulled out | **NO LINK – check the cable** after 0.5 s |
| corrupted bytes | the frame is dropped, the next poll comes 10 ms later | same |

No fault screen is shown for a lost link: without the cord the HMI board has no power, so there is nobody to show it
to, and once the cord is back a message would only state the obvious.

## Pinout

Motherboard (`hardware/desktopia/desktopia.kicad_sch`):

| Function | Schematic net | MCU | Arduino |
|---|---|---|---|
| Motor DIR | `Mdir` (J5-1) | PC6 | D5 |
| Motor PWM | `Mpwm` (J5-2) | PB6 / OC1B | D10 |
| RS-485 RO → RX | `RS485_RO` (U8-1) | PD2 / RXD1 | D0 (Serial1) |
| RS-485 DI ← TX | `RS485_DI` (U8-4) | PD3 / TXD1 | D1 (Serial1) |
| RS-485 DE + /RE | `RS485_DE` (U8-2, U8-3) | PD4 | D4 |
| I2C SDA / SCL | `SDA` / `SCL` | PD1 / PD0 | Wire |
| ISP | `SCK` / `MOSI` / `MISO` | PB1 / PB2 / PB3 | J2 only |
| Motor current alert | `I_ALERT` (U9-3, INA226) | PE6 / INT6 | D7, not used yet |

Free: PB0, PB4, PB5, PB7, PC7, PD5-PD7, PF0, PF1, PF4-PF7. The ToF GPIO1 only goes to test point TP14.
I2C: VL53L1CB at `0x29` (`0x52` in 8-bit notation) behind the TXS0104E; INA226 current monitor U9 at `0x40`
(A0 = A1 = GND) on the 5 V side. It measures the motor supply across the 2 mΩ shunt R25 (±41 A full scale, 1.25 mA/bit
with a calibration of 2048); this firmware does not read it yet.
The 5 V sent to the HMI board goes through the PTC F1 (500 mA). R22 / R23 (4.7 kΩ) bias the bus when nobody drives
it; R24 (120 Ω termination) is not fitted (only for cords over ~5 m; then R22 / R23 = 680 Ω).

HMI board (`hardware/desktopia_HMI/desktopia_HMI.kicad_sch`, RP2040):

| Function | Net | GPIO |
|---|---|---|
| RS-485 DI / RO / DE + /RE | `LINK_TX` / `LINK_RX` / `LINK_DE` | GP0 (UART0 TX) / GP1 (UART0 RX) / GP2 |
| SW1 Up / SW2 Down | `BTN_UP` / `BTN_DOWN` | GP11 / GP12 |
| SW3-SW5 Preset 1-3 | `BTN_PRESET1..3` | GP13 / GP14 / GP15 |
| Screen CS / SCK / MOSI / DC / RST | `LCD_*` | GP17 / GP18 / GP19 / GP20 / GP21 (SPI0) |
| SWD | `SWCLK` / `SWDIO` | TP7 / TP8 (+ RUN on TP9, GND on TP10) |

Buttons switch to GND, with the RP2040 internal pull-ups, and are debounced on the HMI board (15 ms). The screen is still
powered from the 5 V (the module regulates its own 3.3 V); its logic is now 3.3 V from the RP2040.
Power: the 5 V comes from the cord, a 3.3 V LDO (XC6206P332) feeds the RP2040, the flash (W25Q128JV, 16 MB; the firmware uses 2 MB) and the
transceiver. The USB-C (through Schottky D1) only powers the board while it is flashed on the bench.

## Build and flash

Motherboard:

```bash
pio run                                   # build (env desktopia)
pio run -t upload                         # USB, Caterina bootloader (double-press RESET if the port does not show up)
pio run -e desktopia_debug -t upload      # same + logs on the USB serial port (115200 baud, TX only)
pio run -e desktopia_motor_test -t upload # motor bench test, height sensor bypassed (see below)
pio device monitor
pio run -e desktopia_isp -t bootloader    # blank chip: burns bootloader + fuses via J2 (USBasp)
```

HMI board (from `../hmi_firmware/`, the RP2040 platform and toolchain are downloaded on the first build):

```bash
pio run                                   # build (env hmi): .pio/build/hmi/firmware.uf2
pio run -t upload                         # board in bootloader mode, see below
```

First flash of a new HMI board: hold **BOOT** (SW6), plug the USB-C cable, release BOOT. The board shows up as a USB
drive (RPI-RP2), then run `pio run -t upload` or copy `firmware.uf2` onto the drive. Later updates: the same, or SWD
through TP7 / TP8 with a debug probe.

Dependencies: the ST driver (`stm32duino/VL53L1X` 2.0.1, ULD), pinned to its tag; Max Gerhardt's
`platform-raspberrypi` with Earle Philhower's arduino-pico core for the HMI board.
Size: motherboard ≈ 18.6 KB of flash (65 % of the 28 KB next to the bootloader), ≈ 0.9 KB of RAM;
HMI board ≈ 48 KB of flash, ≈ 9 KB of RAM.

## Main settings

Motherboard (`include/config.h`):

| Parameter | Default | Purpose |
|---|---|---|
| `MOTOR_DIR_UP_LEVEL` / `MOTOR_DIR_DOWN_LEVEL` | HIGH / LOW | DIR level to go up / down |
| `MOTOR_PWM_MAX_PERCENT` | 60 | absolute duty-cycle ceiling |
| `MOTOR_PWM_MIN_PERCENT` | 10 | smallest PWM that still moves the desk (end of approach) – **to be tuned** |
| `MOTOR_PWM_FREQ_HZ` | 4000 | PWM frequency, 20 kHz max (MD10C R3); lower = cooler driver, more whine (see below) |
| `MOTOR_SOFT_START_MS` / `MOTOR_SOFT_STOP_MS` | 1500 / 300 | 0→max and max→0 ramps |
| `APPROACH_DISTANCE_MM` / `TARGET_TOLERANCE_MM` | 80 / 2 | start of slowdown / stop within ± |
| `TOF_HEIGHT_SIGN` / `TOF_HEIGHT_OFFSET_MM` | +1 / 0 | height = OFFSET + SIGN × measured distance |
| `HEIGHT_MIN_MM` / `HEIGHT_MAX_MM` | 640 / 1200 | software end stops: **must be set inside the real travel** |
| `PRESET_SHORT_PRESS_MAX_MS` / `PRESET_LONG_PRESS_MS` | 400 / 2000 | press thresholds |
| `HMI_POLL_PERIOD_MS` | 10 | one exchange with the HMI board every 10 ms |
| `HMI_LINK_TIMEOUT_MS` / `HMI_LINK_SETTLE_MS` | 40 / 100 | link declared down / trusted again |
| `TOF_BYPASS` | 0 | 1 = height sensor ignored for motor control, **bench test only** (see below) |

HMI board (`../hmi_firmware/include/hmi_config.h`): `DISPLAY_ROTATION` (1), `DISPLAY_SPI_HZ` (20 MHz),
`HMI_DEBOUNCE_MS` (15), `HMI_POLARITY_SWAP_MS` (150), `HMI_NO_LINK_MS` (500).

## Motor bench test without the sensor (`TOF_BYPASS`)

To test the motor commands (DIR, PWM, ramps) **without the mechanism coupled to the desk** and without needing a
working distance sensor, build with `-DTOF_BYPASS=1`. A ready-made environment does it, with the PWM limited to 25 %:

```bash
pio run -e desktopia_motor_test -t upload
```

Behaviour in this mode:

- After a reset the screen shows **DESKTOPIA – SENSOR BYPASS** for 3 s, and the compiler prints a warning.
- **Up / Down** held: the motor runs at `MOTOR_PWM_MAX_PERCENT` (with soft start / soft stop) as long as the button is
  held, whatever the height. Releasing the button, or pressing both, stops it.
- **No end stops, no stall / wrong-way / sensor-loss detection**: only the 60 s move timeout, the link loss and the
  watchdog remain.
- Presets are disabled (short and long press show *BYPASS – presets off*); the EEPROM is not touched.
- The ToF keeps running: if it is connected, its reading is still displayed (`--.-` otherwise), which helps check
  `TOF_HEIGHT_SIGN` by hand.

> ⚠️ Never use this firmware with the motor coupled to the desk: nothing stops the motor at the mechanical end stops.
> Flash the normal `desktopia` environment again once the test is done.

## Motor driver heating (Cytron MD10C R3)

Ratings: 13 A continuous, 30 A peak (10 s), PWM up to 20 kHz. The board has no heatsink and no current limit.

The driver's losses come from two sources:

- **Switching losses**: at every PWM edge the MOSFETs briefly carry the current and the voltage at the same time. They
  grow **linearly with the PWM frequency**. At 20 kHz (the board's maximum) they are the main source of heat, so the
  default is **4 kHz** (≈ 5× fewer switching losses).
- **Conduction losses**: R<sub>DS(on)</sub> × I². They depend on the motor current, i.e. on the load (lifting the
  desk takes the same torque, so about the same current, whatever the duty). Only a shorter run time reduces them.

Firmware levers, in order of effect:

1. `MOTOR_PWM_FREQ_HZ`: 4000 by default (audible whine). Raising it makes the motor quieter and the driver hotter;
   do not go above 20000 (a `static_assert` refuses it).
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

1. Flash the HMI board on the bench (USB-C, see above): it shows **DESKTOPIA – starting...**, then **NO LINK** after
   0.5 s since there is no motherboard. That proves the screen and the RP2040.
2. **Motor decoupled** from the mechanism (or crank disengaged). First flash of the motherboard with a reduced PWM:
   `build_flags = -DMOTOR_PWM_MAX_PERCENT=25` in the environment being used. Connect the two boards with the RJ11 cord.
3. The screen should show a plausible height that changes when a target is moved in front of the sensor.
   Otherwise: `--.-` = silent sensor (check the 2.8 V rail and the level shifter), absurd value = adjust `TOF_HEIGHT_OFFSET_MM`.
4. Press **Up**: the motor must turn in the "up" direction **and** the displayed height must increase.
   A mismatch is detected after 15 mm of travel: **WRONG WAY** screen. Then fix
   `MOTOR_DIR_UP_LEVEL`/`MOTOR_DIR_DOWN_LEVEL` (motor reversed) or `TOF_HEIGHT_SIGN` (height reversed).
5. Pull the cord out while holding Up: the motor must stop at once.
6. Set `HEIGHT_MIN_MM` / `HEIGHT_MAX_MM` **inside** the mechanical end stops (the firmware slows down before them).
7. Set `MOTOR_PWM_MAX_PERCENT` back to 60, couple the mechanism. If **STALLED** appears at the end of a preset approach,
   the minimum PWM is too low to get the desk moving: increase `MOTOR_PWM_MIN_PERCENT` by 3-5 points.
8. Test saving a preset (long press) then recalling it from another height, in both directions.

## Built-in protections

The INA226 current monitor is fitted but not read yet: everything relies on the ToF and on the link.

- Loss of the ToF (no valid measurement for 400 ms) or of the HMI link (buttons unknown) → immediate stop, no ramp.
- Wrong direction (15 mm the opposite way), stall (3 mm of progress in 1.5 s while the PWM is above the minimum),
  movement longer than 60 s → immediate stop and fault screen; back to normal after 2.5 s if the cause is gone.
- Software end stops `HEIGHT_MIN/MAX_MM` with slowdown; a button held at the end stop does not restart the motor.
- The direction (DIR) only changes when the PWM is at 0, after 400 ms stopped.
- 500 ms watchdog on the motherboard: if the loop freezes, an interrupt cuts the PWM, then the MCU restarts.
  It steps aside when the Arduino core reprograms the watchdog for the automatic reset into the bootloader (USB upload).
- 2 s watchdog on the HMI board, fed only while both cores make progress. A frozen HMI board stops answering, so the
  motherboard stops the motor.
- A button held while the HMI board powers up is reported as pressed from its first answer, so a reconnection can never
  look like a new press.
- I2C bus with timeout (25 ms). The ST driver loops forever on a NACK: it is only called after a successful address ping.
- No pinch or obstacle detection other than the "no more progress" stall.

## Hardware notes

The boards have their own READMEs: [motherboard](../hardware/desktopia/README.md) and
[HMI board](../hardware/desktopia_HMI/README.md). They cover the blocks, the connectors, the JLCPCB parts, the PCB work
still to do and the known issues (TPS5430 ENA, no pull-down on `Mpwm` / `Mdir`).

## Assumptions to confirm

- The screen is a **GC9A01 240×240** (7-pin connector: VCC, GND, SCK, MOSI, DC, CS, RST). Otherwise, only `gc9a01.cpp` changes.
- The screen module accepts 3.3 V logic with a 5 V supply (true of the usual modules, which regulate their own 3.3 V).
- The sensor is a **VL53L1CB** (not "VL51"): ST describes it as a superset of the VL53L1X and the ULD driver is used as is.
  The ID read at startup is not enforced (it is shown in debug mode): to be confirmed on the board.
- Height and end stops are expressed in the quantity your setup measures (direction and offset adjustable, see above).

## What has been verified, and what has not

- **Motherboard build** with PlatformIO (avr-gcc 7.3.0), all environments, `-Wall -Wextra` with no warnings.
- **HMI board build** with PlatformIO (platform-raspberrypi, arduino-pico core), `-Wall -Wextra` with no warnings.
- **PC simulation** (`sim/run.sh`, 628 checks passed) of the real sources of **both** boards against a simulated desk,
  a noisy ToF (outliers and invalid values included), a simulated RS-485 bus and cord, buttons with bounce and an emulated
  GC9A01 screen. Covered: PWM ramps and ceiling, DIR direction, presets (short / long press, EEPROM, gauge), recall accuracy
  (standard deviation ≈ 2.3 mm with 1-3 mm sensor noise), end stops, faults, `millis()` rollover, screen rendering.
  Link scenarios: frame codec (every single-bit error caught), cord pulled out while moving (motor cut after 36 ms), cord back
  in with a button still held (no movement), boot without the cord, reversed cord (link found in ~150 ms), data pair cut
  (NO LINK on the HMI board), 1 corrupted byte in 500 (no false stop), 20 ms motherboard loop.
- **Schematics**: ERC of both projects, and the exported netlists checked pin by pin by script (every link signal; the 195
  motherboard pins outside the change keep exactly their previous connections).
- **Not tested**: on real boards. No trials with the motor driver, the VL53L1CB, the screen module, the transceivers or an
  RJ11 cord. The PCBs are not laid out (see the board READMEs).

## Simulation bench (optional)

```bash
cd sim && ./run.sh     # Linux / WSL, g++; writes emulated screenshots to sim/shots/ (PPM)
./preview.sh           # rebuilds docs/ui_preview.png from those screenshots (ImageMagick)
```
