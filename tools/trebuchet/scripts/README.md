This directory contains scripts for automating various tasks.

## `run-startup.sh`

Usage:
```
run-startup.sh <package name> <activity name>
```

This script automatically starts an app on a device connected through adb with
appropriate tracing enabled. It then saves a trace and reports a summary of the
startup behavior using StartupAnalyzerKt. This script requires `adb` to be in
your path and that a phone be connected.
