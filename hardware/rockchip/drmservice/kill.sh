#!/bin/sh
PID=`adb shell ps | grep drmservice | awk '{print $2}'`
adb shell kill -9 $PID
