How to generate dex metadata (.dm) with profile for testing
===========================================================
```
adb shell profman --generate-test-profile=/data/local/tmp/primary.prof
adb pull /data/local/tmp/primary.prof
zip foo.dm primary.prof
```
