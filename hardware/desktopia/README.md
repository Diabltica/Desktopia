# Desktopia – motherboard

KiCad 10 project: `desktopia.kicad_sch`, `desktopia.kicad_pcb`. Firmware: [../../firmware/](../../firmware/README.md).

The motherboard sits under the desk top. It takes the 24 V supply and passes it to the motor driver through a current
shunt. It measures the height and controls the motor driver. It also powers the HMI board and talks to it.

## Views

Interactive viewer (KiCanvas, opens the files of the `main` branch in the browser):
[schematic](https://kicanvas.org/?github=https://github.com/Diabltica/Desktopia/blob/main/hardware/desktopia/desktopia.kicad_sch) · [PCB](https://kicanvas.org/?github=https://github.com/Diabltica/Desktopia/blob/main/hardware/desktopia/desktopia.kicad_pcb).

The images below are rendered by [`../render.sh`](../render.sh) and committed by the
[*Hardware images*](../../.github/workflows/hardware-images.yml) workflow on every push that touches `hardware/`.
Click an image to open it full size.

**Schematic**

[![Schematic](images/schematic.svg)](images/schematic.svg)

**PCB**

| Front | Back (mirrored) |
|---|---|
| [![PCB front](images/pcb_front.svg)](images/pcb_front.svg) | [![PCB back](images/pcb_back.svg)](images/pcb_back.svg) |

**3D**

![3D view](images/3d_iso.png)

| Top | Bottom |
|---|---|
| ![3D top](images/3d_front.png) | ![3D bottom](images/3d_back.png) |

## Blocks

| Block | Parts | Notes |
|---|---|---|
| 24 V input | J7 (5.08 mm screw terminal), C2 (4.7 µF bulk), C25 (10 µF 50 V ceramic), TP1 | J7 carries the full motor current |
| 24 V → 5 V | U1 TPS5430 (3 A buck), L1 85 µH, D1 SS14, C1 10 nF (boot), C4 33 µF, R3 / R4 (10k / 3.3k → 4.92 V) | ENA left open, see *Known issues* |
| Motor current | R25 2 mΩ 3 W shunt, U9 INA226 (I2C 0x40), C20, R26 | ±41 A full scale; alert on PE6 |
| MCU | U3 ATmega32U4 @ 16 MHz (Y1, C8 / C9), RST1 reset button, R5 reset pull-up | Caterina bootloader, USB |
| USB | J3 USB-C (GCT USB4105GFA), R8 / R9 22 Ω, R15 / R16 5.1k on CC | VBUS only goes to the MCU's VBUS sense pin: USB does **not** power the board |
| ISP | J2 2×3 header (not assembled) | blank chip: bootloader + fuses with a USBasp |
| Height sensor | U6 VL53L1CB, U4 XC6206P282 (2.8 V LDO), U5 TXS0104E (5 V ↔ 2.8 V I2C), R17-R20 | XSHUT tied high (R19), GPIO1 to TP14 only |
| Motor driver | J4 (24 V out to the driver), J5 (DIR, PWM, GND) | external Cytron MD10C R3 |
| HMI link | J6 RJ11 6P6C, U8 SP485EEN (RS-485), F1 500 mA PTC, R22 / R23 4.7k bias, R24 120 Ω (not assembled), C23 / C24 | see the link section of the firmware README |
| Power LED | LED1, R1 + R2, JP1 solder jumper | LED off until JP1 is bridged |

## Connectors

| Ref | Function | Pin-out |
|---|---|---|
| J7 | 24 V supply input | 1 = +24 V, 2 = GND |
| J4 | supply out to the motor driver (after the shunt) | 1 = MOTOR_24V, 2 = GND |
| J5 | motor driver command | 1 = DIR (`Mdir`, PC6), 2 = PWM (`Mpwm`, PB6 / OC1B), 3 = GND |
| J6 | RJ11 cord to the HMI board | 1 = +5 V, 2 = GND, 3 = A, 4 = B, 5 = GND, 6 = +5 V |
| J3 | USB-C: flashing and debug logs | USB 2.0 full speed |
| J2 | ISP | 1 = MISO, 2 = +5 V, 3 = SCK, 4 = MOSI, 5 = RESET, 6 = GND |

The J6 pin-out is symmetric, so a reversed telephone cord only swaps A and B, never +5 V and GND. The HMI firmware
detects and corrects the swap. Use 6-conductor (6P6C) cords only, and never plug J6 into a telephone wall socket.

## Power

- Input: 24 V. Size the supply for the motor (the MD10C is rated 13 A continuous) plus a few watts for the
  electronics.
- 5 V rail (TPS5430): MCU, transceiver, INA226, level shifter, and the HMI board through F1 (500 mA PTC).
- 2.8 V rail (XC6206P282): VL53L1CB and the low side of the TXS0104E.

## I2C bus

| Device | Address | Side |
|---|---|---|
| INA226 (U9) | 0x40 (A0 = A1 = GND) | 5 V |
| VL53L1CB (U6) | 0x29 (0x52 in 8-bit form) | 2.8 V, behind the TXS0104E |

SDA = PD1, SCL = PD0. Pull-ups: 3.3k R7 / R6 to 5 V, and R17 / R18 to 2.8 V on the sensor side.
The INA226 settings are R<sub>shunt</sub> = 2 mΩ, current LSB = 1.25 mA, calibration = 2048. It reads 0.34 W in the
shunt at 13 A.

## Test points

| TP | Net | TP | Net |
|---|---|---|---|
| TP1 | +24V | TP13 | SDA |
| TP7 | RESET | TP14 | VL53L1CB GPIO1 |
| TP9 | GND | TP15 / TP16 / TP17 | SCL / SDA / GPIO1 on the 2.8 V side of the level shifter |
| TP10 | +5V | TP18 | VL53L1CB XSHUT |
| TP11 | +2V8 | TP12 | SCL |

## Assembly (JLCPCB)

Every assembled part has its LCSC number in the `LCSC` and `jlcpcb PartNumber` fields. Basic parts were chosen
wherever one fits; the Extended ones (INA226, terminals, connectors, ...) each add a loading fee per order.
Not assembled: J2 (ISP header), R24 (termination), the test points and the mounting holes H1 / H2 (M2).

## PCB: what to do before ordering

The PCB has **not** been updated since these schematic changes:

- RJ11 link: FPC_MB1, the TCA9554 (U2), R10-R14, C10, C11 and TP8 are gone; J6, U8, F1, R22-R24, C23 and C24 are new.
- Power input: DC1, U7 (CC6905), R21, C21, C22 and TP19 are gone. J7, U9, R25, R26 and C25 are new. C20 is now the
  INA226 decoupling capacitor.

Run *Tools → Update PCB from Schematic*, then:

- Place C25 right against VIN / GND of U1 (TI asks for a ceramic within 1 mm). C2 stays as bulk.
- Give J7 → R25 → J4 and the GND return copper for 15 A: pours, or tracks at least 5 mm wide in 1 oz copper.
- Route Vin+ / Vin- of the INA226 as a Kelvin pair from the inner side of the R25 pads.
- Check the RJ11 jack (EVERCOM 5301-6P6C, C3097716) against the Amphenol 54601 footprint it uses. Compare the drilling
  with the part's datasheet.

## Known issues and history

1. **TPS5430 ENA (fixed in the schematic).** ENA (pin 5) used to be tied to the 24 V input. Its absolute maximum
   is 7 V, and that burnt the regulator on the first board. ENA is now left open: its internal 1.5 MΩ pull-up keeps
   the regulator on. On a board that met this fault, also check the 5 V parts, since the dead regulator may have
   passed the 24 V through.
2. **Barrel jack (fixed).** DC1 was rated 1.5 A / 12 V and carried the motor current. It is replaced by the J7
   screw terminal.
3. **`Mpwm` and `Mdir` have no pull-down.** These lines float during a reset, the bootloader or an ISP flash.
   Depending on the motor driver, the desk may start moving. Add a 10 kΩ pull-down on `Mpwm` (and `Mdir`), or check
   that the driver has one.
4. The INA226 is fitted but the firmware does not read it yet.
