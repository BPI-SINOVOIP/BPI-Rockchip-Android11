# Trade Federation (TF / Tradefed)

TF is a test harness used to drive Android automated testing. It runs on test hosts
and monitors the connected devices, handling test scheduling & execution and device
management.

Other test harnesses like Compatibility Test Suite (CTS) and Vendor Test Suite
(VTS) use TF as a basis and extend it for their particular needs.

Building TF:
  * source build/envsetup.sh
  * tapas tradefed-all
  * make -j8

More information at:
https://source.android.com/devices/tech/test_infra/tradefed/

See more details about Tradefed Architecture at:
https://source.android.com/devices/tech/test_infra/tradefed/architecture

If you are a tests writer you should start looking in the test_framework/
component which contains everything needed to write a tests in Tradefed.
