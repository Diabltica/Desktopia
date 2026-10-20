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
| 24 V input | J7 (5.08 mm screw terminal, 24 A), D2 SMBJ28A (600 W TVS), C2 (220 µF 50 V bulk), C25 (10 µF 50 V ceramic), TP1 | J7 carries the full motor current. D2 clamps motor and hot-plug spikes below the 40 V absolute maximum of U1 and U9 |
| 24 V → 5 V | U1 TPS5430 (3 A buck), L1 22 µH (1.8 A, shielded), D1 SS34 (3 A), C1 10 nF (boot), C4 + C26 (2 × 47 µF ceramic), R3 / R4 (10k / 3.3k → 4.92 V), C27 / R27 / C28 / C29 (compensation) | 1.5 A available. ENA left open, see *Known issues* |
| Motor current | R25 2 mΩ 3 W shunt, U9 INA226 (I2C 0x40), C20, R26, C30 | ±41 A full scale; alert on PE6 / INT6 (R26 4.7k pull-up, C30 100 pF against motor spikes) |
| MCU | U3 ATmega32U4 @ 16 MHz (Y1, C8 / C9), RST1 reset button, R5 reset pull-up | Caterina bootloader, USB |
| USB | J3 USB-C (G-Switch GT-USB-7010ASV), R8 / R9 22 Ω, R15 / R16 5.1k on CC | VBUS only goes to the MCU's VBUS sense pin: USB does **not** power the board |
| ISP | J2 2×3 header (not assembled) | blank chip: bootloader + fuses with a USBasp |
| Height sensor | U6 VL53L1CB, U4 XC6206P282 (2.8 V LDO), Q1 / Q2 / Q3 BSS138 (5 V ↔ 2.8 V level shifters: SCL, SDA, GPIO1), R17-R20, R30 | XSHUT tied high (R19). GPIO1 (data ready) reaches PB4 / PCINT4 as `TOF_INT` |
| Motor driver | J4 (24 V out to the driver), J5 (DIR, PWM), R28 / R29 (10k pull-downs on PWM / DIR) | external Cytron MD10C R3. The pull-downs keep the motor stopped while the MCU is in reset, in the bootloader or blank |
| HMI link | J6 RJ11 6P6C, U8 SP485EEN (RS-485), F1 500 mA PTC, R22 / R23 4.7k bias, R24 120 Ω (not assembled), C23 / C24 | see the link section of the firmware README |
| Power LED | LED1, R1 (470 Ω, 6 mA), JP1 solder jumper | LED off until JP1 is bridged |

## Connectors

| Ref | Function | Pin-out |
|---|---|---|
| J7 | 24 V supply input | 1 = +24 V, 2 = GND |
| J4 | supply out to the motor driver (after the shunt) | 1 = MOTOR_24V, 2 = GND |
| J5 | motor driver command | 1 = DIR (`Mdir`, PC6), 2 = PWM (`Mpwm`, PB6 / OC1B). The driver's logic ground returns through J4 pin 2 |
| J6 | RJ11 cord to the HMI board | 1 = +5 V, 2 = GND, 3 = A, 4 = B, 5 = GND, 6 = +5 V |
| J3 | USB-C: flashing and debug logs | USB 2.0 full speed |
| J2 | ISP | 1 = MISO, 2 = +5 V, 3 = SCK, 4 = MOSI, 5 = RESET, 6 = GND |

The J6 pin-out is symmetric, so a reversed telephone cord only swaps A and B, never +5 V and GND. The HMI firmware
detects and corrects the swap. Use 6-conductor (6P6C) cords only, and never plug J6 into a telephone wall socket.

J4, J5 and J7 are the same terminal (KF128-5.08-2P, 24 A, 12-22 AWG), so they count as one Extended part. Use at least
22 AWG wire, or ferrules, in J5. J5 has no ground pin: the driver's logic ground is its power ground, which comes back
through J4. A separate ground wire would put part of the motor current in the thin command cable.

## Power

- Input: 24 V, 28 V at most (D2 starts to conduct at 31.1 V; the MD10C itself is limited to 30 V). Size the supply
  for the motor (the MD10C is rated 13 A continuous) plus a few watts for the electronics.
- Motor path, sized with margin: J7 / J4 terminals 24 A, R25 3 W (0.34 W at 13 A, 1.8 W at the MD10C's 30 A peak),
  C2 220 µF 50 V bulk, D2 600 W TVS. Give J7 → R25 → J4 and the ground return copper for 15 A or more.
- 5 V rail (TPS5430): MCU, transceiver, INA226, and the HMI board through F1 (500 mA PTC). L1 (1.8 A saturation) and
  D1 (3 A) allow 1.5 A, about twice the worst case (HMI at the PTC limit + this board).
- The 5 V output uses ceramic capacitors, so the TPS5430 needs the external compensation of TI's SLVA237 (C27, R27, C28,
  C29). A model of the loop (TPS5430 type III network + LC filter + this network) gives a 14 to 27 kHz crossover and
  more than 60° of phase margin from 0.1 to 1.5 A, with ±25 % on the output capacitance.
- 2.8 V rail (XC6206P282, ±2 %): VL53L1CB, its pull-ups and the gates of Q1 / Q2. 2.8 V is the sensor's typical
  supply (recommended 2.6 to 3.5 V, 3.6 V absolute maximum, datasheet DS12385). That leaves 0.7 V of headroom under the
  maximum; 3.3 V would only leave 0.13 V in the worst case.

## I2C bus

| Device | Address | Side |
|---|---|---|
| INA226 (U9) | 0x40 (A0 = A1 = GND) | 5 V |
| VL53L1CB (U6) | 0x29 (0x52 in 8-bit form) | 2.8 V, behind Q1 / Q2 |

SDA = PD1, SCL = PD0. Pull-ups: 3.3k R7 / R6 to 5 V, and R17 / R18 to 2.8 V on the sensor side (`SCL_2V8` / `SDA_2V8`).
Q1 / Q2 are the classic MOSFET level shifter (NXP AN10441): gate on +2V8, source on the sensor side, drain on the 5 V
side. The BSS138 (Vgs(th) 1.5 V max) turns fully on with 2.8 V on its gate, so a low from the MCU reaches the sensor at
about 0.2 V, under its 0.6 V VIL. A 2N7002 (Vgs(th) up to 2.5 V) would only pass it through its body diode, around 0.8 V.
At 400 kHz the 3.3k pull-ups give about 220 ns of rise time, under the 300 ns of the fast-mode specification (ST
suggests 3.6k up to 90 pF).

Q3 is the same circuit on the VL53L1CB GPIO1 (data-ready interrupt, R20 10k pull-up to 2.8 V). It gives `TOF_INT` on
PB4 (D8, pin-change interrupt PCINT4), with R30 10k pull-up to 5 V. The MCU sees a full 5 V high level instead of
2.8 V, which is under the ~3 V input threshold of the ATmega. The firmware polls the sensor over I2C and does not use
`TOF_INT` yet.

The ATmega32U4 has five external interrupts, all taken. INT0 / INT1 (PD0 / PD1) are the I2C and INT2 / INT3
(PD2 / PD3) the RS-485 UART; both are fixed by the hardware. INT6 (PE6) is kept for the INA226 alert (`I_ALERT`).
`TOF_INT` therefore uses a pin-change interrupt. It is also a hardware interrupt, with the same latency, but
`attachInterrupt()` does not cover it: configure PCICR / PCMSK0, or simply read PB4. PB0, PB5 and PB7 are free and
keep their pin-change interrupts (PCINT0 / 5 / 7).
The INA226 settings are R<sub>shunt</sub> = 2 mΩ, current LSB = 1.25 mA, calibration = 2048. It reads 0.34 W in the
shunt at 13 A.

## Test points

| TP | Net | TP | Net |
|---|---|---|---|
| TP1 | +24V | TP12 | SCL (5 V side) |
| TP7 | RESET | TP13 | SDA (5 V side) |
| TP9 | GND | TP15 / TP16 / TP17 | SCL / SDA / GPIO1 on the 2.8 V side |
| TP10 | +5V | TP18 | VL53L1CB XSHUT |
| TP11 | +2V8 | | |

## Assembly (JLCPCB)

Every assembled part has its LCSC number in the `LCSC` and `jlcpcb PartNumber` fields. JLCPCB charges a loading fee
(about $3 per order) for each Extended part number. Basic parts have none, and Preferred parts have none in Economic
PCBA.

- Extended: C2 (220 µF bulk), L1, F1, J3 (USB-C), J4 / J5 / J7 (one part number), J6, R25, U4 (2.8 V LDO),
  U6 (VL53L1CB), U9 (INA226). JLCPCB has no Basic power inductor, electrolytic capacitor, fuse, connector, shunt or
  2.8 V regulator.
- Preferred: D2 (TVS), Q1 / Q2 / Q3 (BSS138). Everything else is Basic.
- Not in the BOM: U3 (the ATmega32U4 has *Exclude from BOM* set: solder it yourself or clear the flag), J2 (ISP
  header), R24 (termination), the test points and the mounting holes H1 / H2 (M2).

The October 2026 cost pass went from 12 Extended part numbers to 9 (J3 aside), about $9 less of loading fees per
order:

- U5 TXS0104E → Q1 / Q2 BSS138 (Preferred). C18 / C19, its decoupling capacitors, are gone.
- C4, a 33 µF electrolytic, → 2 × 47 µF ceramic (Basic) with the compensation network. C2 stays the only electrolytic.
- J5, a 3-pin 2.54 mm terminal, → the same 2-pin terminal as J4 / J7.
- J3 GCT USB4105 (out of stock, about $1) → G-Switch GT-USB-7010ASV ($0.09, in stock), now in the BOM.
- R1 + R2 → R1 alone. F1 → a 30 V PTC with more stock.

Parts cost about $0.25 more per board. The motor input and the 5 V rail got more margin (TVS, 220 µF bulk, 24 A
terminals, 1.5 A rail). The motor command lines got pull-downs, the ToF interrupt a level shifter, and the INA226
alert a filter. U4 stays at 2.8 V for the sensor's headroom (see *Power*).

## PCB: what to do before ordering

The PCB has **not** been updated since these schematic changes:

- RJ11 link: FPC_MB1, the TCA9554 (U2), R10-R14, C10, C11 and TP8 are gone; J6, U8, F1, R22-R24, C23 and C24 are new.
- Power input: DC1, U7 (CC6905), R21, C21, C22 and TP19 are gone. J7, U9, R25, R26 and C25 are new. C20 is now the
  INA226 decoupling capacitor.
- Cost pass: U5 (TXS0104E), C18, C19, TP14 and R2 are gone. Q1, Q2, Q3, D2, C26, C27, C28, C29, C30, R27, R28, R29
  and R30 are new. C2, C4, L1, J3 and J5 have new footprints (J3: G-Switch GT-USB-7010ASV, J5: 2-pin 5.08 mm).
  R26 is now 4.7k, with C30 next to PE6. PB4 now carries `TOF_INT`.

Run *Tools → Update PCB from Schematic*, then:

- Place C25 right against VIN / GND of U1 (TI asks for a ceramic within 1 mm). Put D2 and C2 next to J7.
- Keep the buck loop tight: C4 / C26 next to L1 and the ground of D1. Put C27, C29, R27 and C28 next to the VSENSE pin
  of U1, away from the PH node and L1.
- J4 / J5 / J7 keep the `C508128L-2P0G36` footprint (5.08 mm, 1.7 mm holes), which fits the KF128-5.08 terminal.
  Check the body outline against its datasheet.
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
3. **`Mpwm` and `Mdir` pull-downs (fixed in the schematic).** These lines floated during a reset, the bootloader or an
   ISP flash, and the desk could start moving. R28 / R29 (10 kΩ) now hold them low.
4. The INA226 is fitted but the firmware does not read it yet.
5. The 5 V loop compensation (C27 / R27 / C28 / C29) is calculated, not measured. On the first board, check the 5 V
   ripple and a load step, for example 0.1 → 0.6 A.
6. **VL53L1CB I/O mode.** The ST driver (STM32duino VL53L1X 2.0.1) writes 0 to registers 0x2E / 0x2F, which selects the
   1V8 I/O mode. This board pulls SCL / SDA / GPIO1 up to AVDD (2.8 V), and for that the datasheet (DS12385, table 13)
   asks for the 2V8 mode: set bit 0 of 0x2E (I2C) and 0x2F (GPIO1) after `VL53L1X_SensorInit()`. The original design
   had the same mismatch.
