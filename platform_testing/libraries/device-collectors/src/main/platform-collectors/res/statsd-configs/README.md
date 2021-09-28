# Statsd Configs

This directory hosts binary statsd config protos.

## What they do

A config tells statsd what metrics to collect from the device during a test. For example,
`app-start` will instruct statsd to collect app startup metrics.

## Checking in a config

To check in config(s) for a new set of metrics, follow these steps:

1. Create a directory under this directory for the new metrics (e.g. `app-start`).
2. Put the new config(s) in the subdirectory using the directory name and optionally with additional
suffixes if there are multiple configs related to the overarching metrics, with `.pb` extension.
This ensures that each config has a unique name.
3. Write a README file explaining what the config(s) in the new subdirectory does and put it under
the new subdirectory.

# (Internal only) Creating a config

_This section is subject to change as the Android Metrics team evolve their tools._

To create a config, follow these steps:

1. Follow the
[Add your metrics to a config](http://go/westworld-modulefooding#add-your-metrics-to-a-config)
section (and this section only) in the Android Metrics documentation to create a new config file.
2. Validate the config following the
[Validate and sent a changelist for review](http://go/westworld-modulefooding#validate-and-send-a-changelist-for-review)
section, but skip the sending CL for review part.
3. Build the config parsing utility:
`blaze build -c opt java/com/google/wireless/android/stats/westworld:parse_definitions`
4. Use the utility to create the binary config:
```
blaze-bin/java/com/google/wireless/android/stats/westworld/parse_definitions \
--action=WRITE_STATSD_CONFIG \
--definitions_dir=wireless/android/stats/platform/westworld/public/definitions/westworld/ \
--config_name=<You config's name defined in step 1> \
--enable_string_hash=false \    # Output human-readable package names to the uid map in the report.
--allowed_sources=<Comma separated list of allowed log sources> \
--output_file=<Output path of your config>
```
Common allowed sources include `AID_ROOT`, `AID_SYSTEM`, `AID_RADIO`, `AID_BLUETOOTH`,
`AID_GRAPHICS`, `AID_STATSD` and `AID_INCIDENTD`.

Once the config file is generated, it can be checked in following the steps in the "Checking in a
config" section.
