How to add a test for a new HID device
======================================

    1. Connect the device of interest to Android
    2. Open adb shell
    3. Go to /sys/kernel/debug/hid/0005:0B05:4500.000F
       Here "0005:0B05:4500.000F" is just an example, it will be different for each device.
       Just print the /sys/kernel/debug/hid directory to see what it is for you.
       This identifier will also change each time you reconnect the same physical device to Android.
    4. `cat rdesc` will print the descriptor of this device
    5. `cat events` will print the events that the device is producing
       Once you cat the events, generate some events (by hand) on the device.
       This will show you the hid reports that the device produces.

To observe the MotionEvents that Android receives in response to the hid reports, write a small
app that would override `dispatchGenericMotionEvent` and `dispatchKeyEvent` of an activity.
There, print all of the event data that has changed. For MotionEvents, ensure to look at the
historical data as well, since multiple reports could get batched into a single MotionEvent.
