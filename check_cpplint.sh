#!/bin/bash

EXIT_STATUS=0
cpplint --filter=-build/include_subdir ./src/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --recursive ./src/supla/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir --recursive ./extras/porting/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir,-build/include_order ./extras/examples/linux/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir ./extras/examples/esp8266_rtos/main/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir ./extras/examples/esp_idf/main/* || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir ./extras/examples/freertos_linux/main.cpp || EXIT_STATUS=$?

if [ $EXIT_STATUS -ne 0 ]
then
  echo
  echo "======================="
  echo "| ERRORS: please fix! |"
  echo "======================="
fi

exit $EXIT_STATUS
