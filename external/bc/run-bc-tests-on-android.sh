#!/bin/bash

# Copy the tests across.
adb shell rm -rf /data/local/tmp/bc-tests/
adb shell mkdir /data/local/tmp/bc-tests/
adb push tests/ /data/local/tmp/bc-tests/
adb push functions.sh /data/local/tmp/bc-tests/

if tty -s; then
  dash_t="-t"
else
  dash_t=""
fi

exec adb shell $dash_t /data/local/tmp/bc-tests/tests/all.sh bc 0 1 0 1 bc
