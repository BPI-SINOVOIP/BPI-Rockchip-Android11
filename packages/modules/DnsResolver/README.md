## Logging

This code uses LOG(X) for logging. Log levels are VERBOSE,DEBUG,INFO,WARNING and ERROR.
The default setting is WARNING and logs relate to WARNING and ERROR will be shown. If
you want to enable the DEBUG level logs, using following command.
adb shell service call dnsresolver 10 i32 1
VERBOSE   0
DEBUG     1
INFO      2
WARNING   3
ERROR     4
Verbose resolver logs could contain PII -- do NOT enable in production builds.

