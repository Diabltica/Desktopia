# Desktopia – motherboard firmware (ATmega32U4)

Drives the motor (DIR + PWM) and switches its driver's supply, watches the motor current (INA226), reads the height
with a VL53L1CB and runs the state machine. It polls the HMI board over RS-485 through the RJ11 cord every 10 ms:
screen content out, button levels in. Every decision is taken here.

```bash
pio run                                   # build (env desktopia)
pio run -t upload                         # USB, Caterina bootloader (double-press RESET if the port does not show up)
pio run -e desktopia_debug -t upload      # same + logs on the USB serial port (115200 baud, TX only)
pio run -e desktopia_motor_test -t upload # motor bench test, height sensor bypassed: never with the desk coupled
```

- Settings: `include/config.h`, overridable with `-DNAME=value` in `platformio.ini`. Pinout: `include/board_pins.h`
  (from `hardware/desktopia/desktopia.kicad_sch`).
- Hardware: [../../hardware/desktopia/README.md](../../hardware/desktopia/README.md).
- Protocol: `../shared/hmi_link/hmi_protocol.h`. Full documentation (settings, protections, first power-up) and the
  simulation of both boards: [../README.md](../README.md).
