Building the Chromium-based WebView in AOSP is no longer supported or required.
WebView can now be built entirely from the Chromium source code.

Docs on how to build WebView from Chromium for use in AOSP are available here:
https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/aosp-system-integration.md

For questions about building WebView, please contact our mailing list:
https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev

---

The prebuilt APKs provided here are built from Chromium upstream sources; you
should check the commit message to see the version number for a particular
prebuilt. The version number is formatted like "12.0.3456.789" and matches the
tag in the Chromium repository it was built from.

If you want to build your own WebView, you should generally build the latest
stable version, not the version published here: newer versions have important
security and stability improvements.
