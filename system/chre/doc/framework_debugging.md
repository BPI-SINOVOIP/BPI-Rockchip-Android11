# Debugging the CHRE Framework

[TOC]

This section lists the methods that can be used to aid in CHRE framework
debugging.

## Logging

The CHRE framework invokes the `LOGx()` macros defined in
`platform/include/chre/platform/log.h` to log information into the system,
for printf-style debugging. This capability is also exposed to nanoapps via
`chreLog()`. Although it may not be strictly required, it is strongly
recommended that the platform implementation directs these log messages to
Android’s logcat system under the “CHRE” tag, where they will be automatically
collected in bug reports. These logs must not wake the applications processor
(AP), so they should be buffered on a best-effort basis and flushed to the AP
opportunistically when it wakes up.

### Log levels

CHRE framework currently supports four levels, namely Error `LOGE()`, Warning
` LOGW()`, Info `LOGI()` and Debug `LOGD()`. These correspond to the
equivalent [logcat
levels](https://source.android.com/setup/contribute/code-style#log-sparingly).
Choosing an appropriate level for logs, and logging only the necessary
information to identify and debug failures can help avoid issues with “log
spam”, such as log output that is difficult to read, or uninteresting “spammy”
logs causing useful log messages to be pushed out of limited buffering space.

### Log level filtering

The CHRE framework currently only supports compile-time log level filtering.
While it’s recommended to leave it at the default setting, the
`CHRE_MINIMUM_LOG_LEVEL` build flag can be defined to one of the values set
in `util/include/chre/util/log_common.h` to control which log levels are
included in the binary.

## Debug Dumps

A debug dump is human-readable text produced on-demand for debugging purposes.
While `LOGx()` is useful for logging events as they happen, the debug dump is
a complementary function typically used to output a snapshot of the framework's
state, history, vital statistics, etc. The debug dump is especially useful for
information that would be too spammy to log on every change, but is useful to
diagnose potential issues. The CHRE framework produces a debug dump when
requested via the Context Hub HAL’s built-in `debug()` method, which is
automatically collected as part of the bug report process, and can be manually
triggered via ADB using the following command:

`adb root && adb shell lshal debug android.hardware.contexthub@1.0::IContexthub/default`

`DebugDumpManager` is the framework module responsible for collecting debug
dumps from the CHRE framework and nanoapps. Refer to the associated
documentation in the source code for details on this process.

## CHRE_ASSERT and CHRE_ASSERT_LOG

`CHRE_ASSERT()` and `CHRE_ASSERT_LOG()` can be used to help catch
programmatic errors during development. However, since their use may have
memory impact, they can be disabled by setting `CHRE_ASSERTIONS_ENABLED` to
false in the Makefile. In general, assertions should be used sparingly - they
are best applied to situations that would lead to potentially unrecoverable
logical errors that are not handled by the code. For comparison, asserting that
a pointer is not null is generally an anti-pattern (though the current CHRE
codebase is not free of this), as dereferencing null would produce a crash
anyways which should have equivalent debuggability as an assertion, among other
reasons.
