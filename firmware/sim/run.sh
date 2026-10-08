#!/bin/bash
# PC simulation bench (Linux / WSL, g++). Compiles the REAL sources from ../src against mocks
# (Arduino core, AVR registers, TCA9554, noisy VL53L1CB, simulated desk, emulated GC9A01 screen) and runs the scenarios.
# The emulated screenshots are written as PPM to sim/shots/.
set -e
cd "$(dirname "$0")"
mkdir -p shots
g++ -std=gnu++11 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -DF_CPU=16000000L \
    -Imocks -I../include -I../src \
    sim.cpp ../src/{buttons,io_expander,motor,presets,tof_sensor,display,gc9a01,desk_controller,watchdog}.cpp \
    -o sim
./sim
