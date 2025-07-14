#!/bin/bash

EXIT_STATUS=0
cpplint --filter=-build/include_subdir --quiet ./src/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --quiet --recursive ./src/supla/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --quiet --recursive ./extras/porting/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir,-build/include_order --quiet ./extras/examples/linux/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --quiet ./extras/examples/esp8266_rtos/main/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --quiet ./extras/examples/esp_idf/main/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --quiet ./extras/examples/freertos_linux/main.cpp || EXIT_STATUS=$?

if [ $EXIT_STATUS -ne 0 ]
then
  echo
  echo "======================="
  echo "| ERRORS: please fix! |"
  echo "======================="
fi

exit $EXIT_STATUS
