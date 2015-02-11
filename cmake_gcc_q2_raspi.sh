#!/usr/bin/env bash
# Assumes Raspberry Pi 'tools' in ~/github/raspberrypi, Raspberry Pi sysroot in ~/rpi-root
RASPI_TOOL=~/github/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin RASPI_ROOT=~/rpi-root SKIP_NATIVE=1 ./cmake_gcc_q2.sh -DCMAKE_BUILD_TYPE=RelWithDebInfo

