# Car rotary service

## Building
```
make CarRotaryController -j64
```

## Enable/disable RotaryService
To enable, run:
```
adb shell settings put secure enabled_accessibility_services com.android.car.rotary/com.android.car.rotary.RotaryService
```
To disable, run:
```
adb shell settings delete secure enabled_accessibility_services
```

## Inject events

### Inject RotaryEvent
To rotate the controller counter-clockwise, run:
```
adb shell cmd car_service inject-rotary
```
For clockwise, run:
```
adb shell cmd car_service inject-rotary -c true
```
To rotate the controller multiple times (100 ms ago and 50 ms ago), run:
```
adb shell cmd car_service inject-rotary -dt 100 50
```

### Inject KeyEvent
To nudge the controller up, run:
```
adb shell cmd car_service inject-key 280
```
Use KeyCode ```280``` for nudge up, ```281``` for nudge down,```282``` for nudge left,```283``` for
nudge right.

To click the controller center button, run:
```
adb shell cmd car_service inject-key 23
```
