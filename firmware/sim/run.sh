#!/bin/bash
# PC simulation bench (Linux / WSL, g++). Compiles the REAL sources of both boards – motherboard (../src) and
# HMI board (../../hmi_firmware/src), plus the shared link protocol (../../shared/hmi_link) – against mocks
# (Arduino cores, AVR registers, RS-485 bus and cord, noisy VL53L1CB, INA226 + switched driver supply,
# simulated desk, emulated GC9A01 screen)
# and runs the scenarios. The emulated screenshots are written as PPM to sim/shots/.
# SIM_FILM=1 ./run.sh also films the screen animations, picture by picture, into sim/shots/anim_*/.
set -e
cd "$(dirname "$0")"
mkdir -p shots
HMI=../../hmi_firmware
g++ -std=gnu++11 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -DF_CPU=16000000L \
    -Imocks -I../include -I../src -I../../shared/hmi_link -I$HMI/include -I$HMI/src \
    sim.cpp ../src/{buttons,current_monitor,hmi_link,motor,presets,tof_sensor,desk_controller,watchdog}.cpp \
    ../../shared/hmi_link/hmi_protocol.cpp $HMI/src/{hmi_app,display,canvas,scene,gc9a01,ui_text}.cpp \
    -o sim
./sim
