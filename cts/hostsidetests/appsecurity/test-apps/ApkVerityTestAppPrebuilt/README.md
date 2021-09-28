How the test works
==================
ApkVerityTestApp is a test helper app to be installed with fs-verity signature
file (.fsv\_sig). In order for this CTS test to run on a release build across
vendors, the signature needs to be verified against a release certificate loaded
to kernel.

How to modify the test helper app
=================================
Modifying the test helper app will also require to sign the apk with a local debug
key. You will also need to point the test to use your local build.

How to load debug key
---------------------
On debuggable build, it can be done by:

```
adb root
adb shell 'mini-keyctl padd asymmetric fsv-play .fs-verity' < fsverity-debug.x509.der
```

On user build, the keyring is closed and doesn't accept extra key. A workaround
is to copy the .der file to /system/etc/security/fsverity. Upon reboot, the
certificate will be loaded to kernel as usual.

How to use the app built locally
--------------------------------
You need to override the prebuilts with the debug build.

1. Build the debug artifacts by `m CtsApkVerityTestDebugFiles`. Copy the output
   to a temporary directory, e.g.

```
(cd $ANDROID_BUILD_TOP && cp `cat
out/soong/.intermediates/cts/hostsidetests/appsecurity/test-apps/ApkVerityTestApp/testdata/CtsApkVerityTestDebugFiles/gen/CtsApkVerityTestDebugFiles.txt`
/tmp/prebuilts/)
```

2. Copy files to create bad app, e.g. in /tmp/prebuilts,

```
cp CtsApkVerityTestApp.apk CtsApkVerityTestApp2.apk
cp CtsApkVerityTestAppSplit.apk.fsv_sig CtsApkVerityTestApp2.apk.fsv_sig
```

3. Rename file names to match the test expectation.
```
for f in CtsApkVerityTestApp*; do echo $f | sed -E 's/([^.]+)\.(.+)/mv & \1Prebuilt.\2/'; done | sh
```

4. Run the test.

```
atest CtsAppSecurityHostTestCases:android.appsecurity.cts.ApkVerityInstallTest
```

How to update the prebuilts
===========================

1. Download android-cts.zip. The current prebuilts are downloaded from the links below.
   TODO(157658439): update the links once we have the correct build target.

```
https://android-build.googleplex.com/builds/submitted/6472922/test_suites_arm64/latest/android-cts.zip
https://android-build.googleplex.com/builds/submitted/6472922/test_suites_x86_64/latest/android-cts.zip
```

2. Extract CtsApkVerityTestApp\*.{apk,dm} and ask the key owner to sign
   (example: b/152753442).
3. Receive the release signature .fsv\_sig.
4. Override CtsApkVerityTestApp2 to create a bad signature.

```
cp CtsApkVerityTestApp.apk CtsApkVerityTestApp2.apk
cp CtsApkVerityTestAppSplit.apk.fsv_sig CtsApkVerityTestApp2.apk.fsv_sig
```

5. Rename to "Prebuilt".

```
for f in CtsApkVerityTestApp*; do echo $f | sed -E 's/([^.]+)\.(.+)/mv & \1Prebuilt.\2/'; done | sh
```

6. Duplicate arm64 prebuilts into arm and arm64, x86\_64 into x86 and x86\_64.
