# Wifi Unit Tests
This package contains unit tests for the android wifi service based on the
[Android Testing Support Library](http://developer.android.com/tools/testing-support-library/index.html).
The test cases are built using the [JUnit](http://junit.org/) and [Mockito](http://mockito.org/)
libraries.

## Running Tests
The easiest way to run tests is simply run

```
atest com.android.server.wifi
```

## Adding Tests
Tests can be added by adding classes to the src directory. JUnit4 style test cases can
be written by simply annotating test methods with `org.junit.Test`.

## Debugging Tests
If you are trying to debug why tests are not doing what you expected, you can add android log
statements and use logcat to view them. The beginning and end of every tests is automatically logged
with the tag `TestRunner`.

## Code Coverage
If you would like to collect code coverage information you can run the `coverage.sh` script located
in this directory. It will rebuild parts of your tree with coverage enabled and then run the tests,
similar to runtests.sh. If you have multiple devices connected to your machine make sure to set the
`ANDROID_SERIAL` environment variable before running the script. You must supply an output directory
for results. By default the results are generated as a set of HTML pages. For example, you can use
the following from the root out your source tree to generate results in the wifi_coverage directory

```
frameworks/opt/net/wifi/tests/wifitests/coverage.sh wifi_coverage
```
