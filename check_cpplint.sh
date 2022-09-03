#!/bin/bash

cpplint --filter=-build/include_subdir ./src/*
cpplint --filter=-build/include_subdir --recursive ./src/supla/*
cpplint --filter=-build/include_subdir --recursive ./extras/porting/*
cpplint --filter=-build/include_subdir,-build/include_order ./extras/examples/linux/*
cpplint --filter=-build/include_subdir ./extras/examples/esp8266_rtos/main/*
cpplint --filter=-build/include_subdir ./extras/examples/esp_idf/main/*
cpplint --filter=-build/include_subdir ./extras/examples/freertos_linux/main.cpp
