#!/bin/bash

set -e

#export SUPLA_DEVICE_PATH=~/supla-device
cd extras/examples/linux
if [ ! -d "build" ]; then
  mkdir build
fi
cd build
cmake ..
make -j10

