## 3.12\. TV Input Framework

The [Android Television Input Framework (TIF)](
http://source.android.com/devices/tv/index.html) simplifies the delivery of live
content to Android Television devices. TIF provides a standard API to create
input modules that control Android Television devices.

If device implementations support TIF, they:

*    [C-1-1] MUST declare the platform feature `android.software.live_tv`.
*    [C-1-2] MUST support all TIF APIs such that an application which uses
these APIs and the [third-party TIF-based inputs](https://source.android.com/devices/tv/index.html#third-party_input_example)
service can be installed and used on the device.
