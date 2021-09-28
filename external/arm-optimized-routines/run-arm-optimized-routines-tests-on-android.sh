#!/bin/bash

# Copy the tests across.
adb sync

if tty -s; then
  green="\033[1;32m"
  red="\033[1;31m"
  plain="\033[0m"
else
  green=""
  red=""
  plain=""
fi

failures=0

check_failure() {
  if [ $? -eq 0 ]; then
    echo -e "${green}[PASS]${plain}"
  else
    failures=$(($failures+1))
    echo -e "${red}[FAIL]${plain}"
  fi
}

# Run the 32-bit tests.
adb shell /data/nativetest/mathtest/mathtest /data/nativetest/mathtest/math/test/testcases/directed/*
check_failure

# TODO: these tests are currently a bloodbath.
#adb shell 'cp /data/nativetest/ulp/math/test/runulp.sh /data/nativetest/ulp/ && sh /data/nativetest/ulp/runulp.sh'
#check_failure

# Run the 64-bit tests.
adb shell /data/nativetest64/mathtest/mathtest /data/nativetest64/mathtest/math/test/testcases/directed/*
check_failure

# TODO: these tests are currently a bloodbath.
#adb shell 'cp /data/nativetest64/ulp/math/test/runulp.sh /data/nativetest64/ulp/ && sh /data/nativetest64/ulp/runulp.sh'
#check_failure

echo
echo "_________________________________________________________________________"
echo
if [ $failures -ne 0 ]; then
  echo -e "${red}FAILED${plain}: $failures"
fi
exit $failures
