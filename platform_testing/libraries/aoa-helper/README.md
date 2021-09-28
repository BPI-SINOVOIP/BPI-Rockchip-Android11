# AOA Helper
Java utility which allows a host computer to act as a [USB host to an Android device](https://developer.android.com/guide/topics/connectivity/usb/) using [Android Open Accessory Protocol 2.0](https://source.android.com/devices/accessories/aoa2). The host can then send commands (e.g. clicks, swipes, keystrokes, and more) to a connected device without the need for ADB.

## Usage
Connect to a device using its serial number.
```
AoaDeviceManager manager = new AoaDeviceManager();
AoaDevice device = manager.getDevice("SERIAL");
```

Perform gestures using coordinates (`0 <= x <= 360` and `0 <= y <= 640`).
```
device.click(new Point(0, 0));
device.scroll(new Point(0, 0), new Point(360, 640));
```

Write alphanumeric text, or press key combinations using [USB HID usages](https://source.android.com/devices/input/keyboard-devices).
```
device.write("hello world");
device.key(0x52, 0x52, 0x51, 0x51, 0x50, 0x4F, 0x50, 0x4F, 0x05, 0x04);
```

Press the power `device.wakeUp()`, home `device.goHome()`, or back `device.goBack()` buttons.

## Testing
Run the unit tests using `atest aoa-helper-test --host`.