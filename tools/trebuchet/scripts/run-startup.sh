#!/bin/bash -x

# usage: run-startup.sh <package name> <activity name>

# Runs an Android app, collects a trace and prints out a summary of startup
# metrics.

PACKAGE=$1
ACTIVITY=$2

ADB=adb

# Make sure we use the right adb, etc.
$ADB root

# Stop the app
$ADB shell "am force-stop $PACKAGE"

# Make sure it's compiled for speed
$ADB shell "pm compile -m speed $PACKAGE"

# Clear the page cache
$ADB shell "echo 3 > /proc/sys/vm/drop_caches"

# Start tracing
$ADB shell "atrace -a $PACKAGE -b 32768 --async_start input dalvik view am wm sched freq idle sync irq binder_driver workq hal freq"

# Launch the app
$ADB shell "am start -W -n $PACKAGE/$ACTIVITY"

# Wait a little longer for the app to do whatever it does.
sleep 10

# Capture the trace
$ADB shell "atrace --async_stop -o /sdcard/atrace.trace"

# Get the trace
$ADB pull /sdcard/atrace.trace

# Dump the startup info
./gradlew :trebuchet:startup-analyzer:run --args="`pwd`/atrace.trace"
