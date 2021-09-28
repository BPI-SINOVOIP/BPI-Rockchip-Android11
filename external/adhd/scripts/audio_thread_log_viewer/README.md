# Audio thread log viewer
It is a tool to draw a time chart from audio thread log. It can make debug
easier.

[TOC]

## Prepare an audio thread log
The easiest way to get audio thread log is typing `cras_test_client --dump_a`
in ChromeOS shell.

The format should be like
```
Audio Thread Event Log:
start at 4434i
2019-07-02T15:30:46.539479158 cras atlog  SET_DEV_WAKE                   dev:7 hw_level:216 sleep:168
2019-07-02T15:30:46.539482658 cras atlog  DEV_SLEEP_TIME                 dev:7 wake: 15:30:46.542974324
2019-07-02T15:30:46.539492991 cras atlog  DEV_SLEEP_TIME                 dev:8 wake: 15:30:46.539358095
2019-07-02T15:30:46.539501241 cras atlog  SLEEP                          sleep:000000000.000000000 longest_wake:001553999
...
```

## Generate an HTML file
```
usage: viewer_c3.py [-h] [-o OUTPUT] [-d] FILE

Draw time chart from audio thread log

positional arguments:
  FILE        The audio thread log file

optional arguments:
  -h, --help  show this help message and exit
  -o OUTPUT   The output HTML file (default: view.html)
  -d          Show debug message (default: False)
```

## View the result
Open the output from vierwe_c3.py by Chrome. There are several functions in
this site:
+ The blue points show the hardware level change of the audio thread logs.
  Click a point can jump to a corresponding line in the log area.
+ There are some options can be selected. It can show the event in the chart
  so that users can easily see when a stream is added, when a stream is fetched
  , and so on.
+ The textarea in the lower right corner can be used to note.
