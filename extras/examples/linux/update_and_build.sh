#!/bin/bash

# It is better to not edit this file. Create local copy and edit it there.

set -e

#export SUPLA_DEVICE_PATH=~/supla-device
git pull

if [ ! -d "build" ]; then
  mkdir build
fi
cd build
cmake ..
#make -j10
make

