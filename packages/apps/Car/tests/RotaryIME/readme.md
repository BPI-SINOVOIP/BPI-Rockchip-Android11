# Rotary IME: Sample input method for rotary controller

This is a sample IME for use with a rotary controller. It is intentionally very basic so that it's
easy to understand the code. It doesn't support multiple locales / layouts. It doesn't support
password fields, numeric fields, etc.

## Building
```
make RotaryIME
```

## Installing
```
adb install out/target/product/[hardware]/system/app/RotaryIME/RotaryIME.apk
```

## Using

Once installed, configure the `rotary_input_method` string resource in the
`CarRotaryController` package to refer to this IME:
```
    <string name="rotary_input_method" translatable="false">com.android.car.rotaryime/.RotaryIme</string>
```
Then build and install `CarRotaryController`. There is no need to enable this
IME or select it; the `RotaryService` will select it automatically in rotary mode.
