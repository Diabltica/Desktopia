#!/bin/bash
# Banc de simulation sur PC (Linux / WSL, g++). Compile les sources RÉELLES de ../src contre des mocks
# (cœur Arduino, registres AVR, TCA9554, VL53L1CB bruité, bureau simulé, écran GC9A01 émulé) et déroule les scénarios.
# Les captures d'écran émulées sont écrites en PPM dans sim/shots/.
set -e
cd "$(dirname "$0")"
mkdir -p shots
g++ -std=gnu++11 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -DF_CPU=16000000L \
    -Imocks -I../include -I../src \
    sim.cpp ../src/{buttons,io_expander,motor,presets,tof_sensor,display,gc9a01,desk_controller,watchdog}.cpp \
    -o sim
./sim
