# Desktopia – HMI board firmware (RP2040)

Draws the round GC9A01 screen and reads the 5 buttons; talks to the motherboard over RS-485 through the RJ11 cord.
The motherboard decides everything: this board shows what it is told and reports the button levels.

```bash
pio run                 # .pio/build/hmi/firmware.uf2
pio run -t upload       # hold BOOT (SW6) while plugging the USB-C cable the first time
```

- Core 0: link (`hmi_app.cpp`) – answers every poll within ~0.2 ms, detects a reversed cord (A/B swapped) by itself,
  shows **NO LINK** when nothing valid arrives for 0.5 s.
- Core 1: screen (`display.cpp`, `gc9a01.cpp`) – the same screens as before, drawn at 20 MHz SPI.
- Settings: `include/hmi_config.h`. Pinout: `include/hmi_pins.h` (from `hardware/desktopia_HMI.kicad_sch`).
- Protocol: `../shared/hmi_link/hmi_protocol.h`. Full documentation and the simulation of both boards: `../firmware/README.md`.
