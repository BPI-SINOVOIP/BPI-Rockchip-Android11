Run on Android with

1) Build the tests.
2) adb install <tree root>/out/target/product/<name>/testcases/CRTests/arn64/CRTests.apk
3) adb shell am instrument -w com.hp.creals.tests/android.test.InstrumentationTestRunner

Depending on the device, the last step may take a while.
(CRTest is quick, SlowCRTest is not, especially not the final trig function
test.)

Note that Random seeds are not set.  Hence repeated runs should improve
coverage at the cost of reproducibility.  Failing arguments should however
be printed.

We expect that this test is much too nondeterministic to be usable for any kind
of performance evaluation.  Please don't try.
