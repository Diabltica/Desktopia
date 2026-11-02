# Desktopia – HMI board firmware (RP2040)

Draws the round GC9A01 screen and reads the 5 buttons; talks to the motherboard over RS-485 through the RJ11 cord.
The motherboard decides everything: this board shows what it is told and reports the button levels.

```bash
pio run                 # .pio/build/hmi/firmware.uf2
pio run -t upload       # hold BOOT (SW1) while plugging the USB-C cable the first time
```

- Core 0: link (`hmi_app.cpp`) – answers every poll within ~0.2 ms, detects a reversed cord (A/B swapped) by itself,
  shows **NO LINK** when nothing valid arrives for 0.5 s.
- Core 1: screen at 20 MHz SPI. Animated pixel art (`scene.cpp`: up / down, save, go to a preset, asleep) drawn into
  a 60 × 60 frame of 4 × 4 blocks (`canvas.cpp`) that only sends the blocks that changed; boot, notice and fault
  screens in plain text (`display.cpp`). Panel driver: `gc9a01.cpp`. Preview: [../README.md](../README.md).
- Settings: `include/hmi_config.h`. Pinout: `include/hmi_pins.h` (from `hardware/desktopia_HMI/desktopia_HMI.kicad_sch`).
- Hardware: [../../hardware/desktopia_HMI/README.md](../../hardware/desktopia_HMI/README.md).
- Protocol: `../shared/hmi_link/hmi_protocol.h`. Full documentation and the simulation of both boards: [../README.md](../README.md).
