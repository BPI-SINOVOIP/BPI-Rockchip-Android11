This directory contains ContactsProvider2.apk related CTS tests.

They can be executed with:

$ adb shell am instrument -w -e package android.provider.cts.contacts \
    android.provider.cts/androidx.test.runner.AndroidJUnitRunner
