# CABC testing
*[go/cros-cabc-test][go-link]*

## Description

*CABC* or *Content adaptive backlight control* is the technique that will adjust
panel backlight brightness according to the content. With a bright image showing
on screen, this won't help saving much power. However, when showing darker image
on screen, instead of using the same amount of backlight to show that image,
panel will adjust that image to a brighter version of it and lower the backlight
brightness to make the perception image similar but use lower power.

## CABC verification autotest

There are 2 version of this test.
* `power_Display.cabc` : run at default brightness level (usually 80 nit).
* `power_Display.cabc_max_brightness` : run at full brightness.

The test makes Chrome display the following pattern for 20 seconds with
white pattern between each pattern.

* Greyscale level from 0% to 50% with 10% interval.
* Checker pattern with black and greyscale 100% to 50% with 10% interval.

| Type      | Pattern 1   | Pattern 2  | Pattern 3  | Pattern 4  | Pattern 5  | Pattern 6  |
|-----------|-------------|------------|------------|------------|------------|------------|
| Greyscale | ![0%][g0]   | ![10%][g1] | ![20%][g2] | ![30%][g3] | ![40%][g4] | ![50%][g5] |
| Checker   | ![100%][c0] | ![90%][c1] | ![80%][c2] | ![70%][c3] | ![60%][c4] | ![50%][c5] |

[go-link]: http://goto.google.com/cros-cabc-test
[g0]: png/black.png
[g1]: png/grey10.png
[g2]: png/grey20.png
[g3]: png/grey30.png
[g4]: png/grey40.png
[g5]: png/grey50.png
[c0]: png/checker1.png
[c1]: png/checker90.png
[c2]: png/checker80.png
[c3]: png/checker70.png
[c4]: png/checker60.png
[c5]: png/checker50.png

