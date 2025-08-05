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
  exit $EXIT_STATUS
fi

error=0

echo "🔍 Checking for forbidden printf formats (%ll, PRIxx64)..."

# Lista plików staged z rozszerzeniami C/C++
files=$(git diff --name-only --diff-filter=ACM | grep -E '\.(c|cpp|h|hpp)$' || true)

# Sprawdzenie dla %ll
for file in $files; do
  matches=$(grep -En '%ll' "$file" || true)
  if [ -n "$matches" ]; then
    echo "❌ Found forbidden '%ll' usage in $file:"
    echo "$matches"
    echo ""
    error=1
  fi
done

# Sprawdzenie dla PRIxx64
for file in $files; do
  matches=$(grep -En 'PRI.?64' "$file" || true)
  if [ -n "$matches" ]; then
    echo "❌ Found forbidden 'PRIxx64' macro in $file:"
    echo "$matches"
    echo ""
    error=1
  fi
done

if [ $error -eq 1 ]; then
  echo "⛔ Commit blocked due to forbidden printf formats."
  exit 1
fi

echo "✅ No forbidden printf formats found."
exit 0
