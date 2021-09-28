#!/bin/bash

# usage (From root directory): ./scripts/run-user-switch-perf.sh <target user>

# Runs user switch, collects a trace and prints out a summary of user switch
# metrics - time taken by user start, user switch and user unlock and top 5
# most expensive service in each category

TargetUser=$1

adb shell atrace -o /sdcard/atrace-ss.txt -t 10 ss &
sleep 2
adb shell am switch-user $TargetUser
wait
sleep 2
adb pull /sdcard/atrace-ss.txt /tmp
sleep 1
./gradlew :trebuchet:user-switch-analyzer:run --args="/tmp/atrace-ss.txt -u $TargetUser"
