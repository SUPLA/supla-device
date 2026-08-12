#!/bin/bash

cpplint --version

EXIT_STATUS=0

echo "🔍 Checking for CRLF line endings..."
CRLF_FILES=$(git grep -Il $'\r' -- \
  . \
  ':!extras/esp-idf/esp-modbus/**' \
  ':!extras/examples/freertos_linux/FreeRTOSConfig.h' || true)

if [ -n "$CRLF_FILES" ]; then
  echo "❌ Found CRLF line endings in project files:"
  echo "$CRLF_FILES"
  exit 1
fi

echo "✅ No CRLF line endings found."

mapfile -d '' -t CPP_FILES < <(
  find ./src -maxdepth 1 -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0
  find ./src/supla ./extras/porting -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0
  find ./extras/examples/esp_idf/main \
    -maxdepth 1 -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0
  find ./extras/examples/freertos_linux -maxdepth 1 -type f -name 'main.cpp' -print0
)

mapfile -d '' -t LINUX_CPP_FILES < <(
  find ./extras/examples/linux -maxdepth 1 -type f \
    \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0
)

cpplint --filter=-build/include_subdir --quiet "${CPP_FILES[@]}" || EXIT_STATUS=$?
cpplint --filter=-build/include_subdir,-build/include_order --quiet \
  "${LINUX_CPP_FILES[@]}" || EXIT_STATUS=$?

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

# List staged C/C++ files, excluding native Linux sources. The printf format
# restriction applies to embedded targets, while PRId64 is valid on Linux.
mapfile -t files < <(
  git diff --name-only --staged --diff-filter=ACM -- \
    '*.c' '*.cpp' '*.h' '*.hpp' \
    ':(exclude)extras/porting/linux/**' \
    ':(exclude)extras/examples/linux/**'
)

# Sprawdzenie dla %ll
for file in "${files[@]}"; do
  matches=$(git diff --cached --unified=0 -- "$file" \
    | grep -E '^\+[^+].*%ll' || true)
  if [ -n "$matches" ]; then
    echo "❌ Found forbidden '%ll' usage in $file:"
    echo "$matches"
    echo ""
    error=1
  fi
done

# Sprawdzenie dla PRIxx64
for file in "${files[@]}"; do
  matches=$(git diff --cached --unified=0 -- "$file" \
    | grep -E '^\+[^+].*PRI.?64' || true)
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
