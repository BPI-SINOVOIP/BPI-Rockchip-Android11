*** aside
See also: [Nanoapp Developer Guide](/doc/nanoapp_developer_guide.md) |
[Interacting with Nanoapps](/doc/nanoapp_clients.md)
***

# Nanoapp Overview

[TOC]

Nanoapps are applications written in C or C++ which run in CHRE, to leverage
low-power hardware. Typically, nanoapps integrate with a component on the
Android side, such as a privileged APK, in order to provide complete end-to-end
functionality of the target feature. This Android-side component is referred to
as the nanoapp’s “client”. Since nanoapps are not limited to the same power
constraints that Android apps are, they can process inputs, like sensor data,
much more frequently while the device’s screen is off.

Nanoapps are abstracted from the underlying platform details by the CHRE API,
which is standardized across all CHRE implementations. This means that a nanoapp
is *code compatible* across devices - its source code does not need to be
changed to run on different hardware, but it *may* need to be recompiled. The
CHRE API also provides binary compatibility guarantees across minor versions, so
a nanoapp does not need to be recompiled to run on a device that exposes a newer
or older version of the CHRE API. These properties help provide for the maximum
reuse of code across devices.

Due to system security and resource constraints of the platforms that CHRE
targets, only device OEMs and their trusted partners are able to create
nanoapps. In other words, the system only runs nanoapps that possess a digital
signature that is trusted in advance by the device manufacturer, and APKs must
hold a special privileged/same-signature permission (`ACCESS_CONTEXT_HUB`) to be
able to interact with nanoapps and the Context Hub in general. However, this
does not mean that third-party APKs cannot benefit from CHRE - nanoapps can be
used to power APIs available for use by any Android app.

## Methods for Loading a Nanoapp

While nanoapps are nominally dynamically loadable modules, they can be loaded
into a device through a few methods, each of which has pros and cons elaborated
below.

### Static Nanoapps

Static nanoapps are, as the name suggests, statically compiled into the CHRE
framework binary. Static nanoapps are automatically initialized after the CHRE
framework completes its initialization during the boot process.

Static nanoapps typically aren’t used in production, because this monolithic
approach has downsides in terms of version control, updatability, etc., but it
can be useful during CHRE development and bring-up of new devices, especially
before dynamic loading functionality is enabled. For example, the FeatureWorld
nanoapps (described later) are typically built as static nanoapps.

Static nanoapps are typically unconditionally compiled as part of the framework
build (via `apps/apps.mk`), but then stripped out by the linker if unreferenced
(using the `--gc-sections` option, or equivalent). Static nanoapps are
referenced only if their initialization function appears in the
`kStaticNanoappList` array, which by default is empty, but can be overridden by
the device variant makefile, as in `variant/simulator/` for example.

Some boilerplate is needed to enable nanoapp to be built as a static nanoapp -
see the code wrapped in `#ifdef CHRE_NANOAPP_INTERNAL` in
`apps/hello_world/hello_world.cc`.

### Preloaded Nanoapps

Preloaded nanoapps are built as a separate binary from the CHRE framework, but
included in the vendor partition of an overall device image (hence they are
“preloaded” onto the device). The binaries associated with a preloaded nanoapp
(usually a `.so` and a `.napp_header` file) are checked in to the Android tree
as a “prebuilt” binary, and integrated into the Android build system so as to
appear in the resulting device image, for example using `$(BUILD_PREBUILT)` in
Android.mk, or `prebuilt_dsp` or `prebuilt_firmware` in Android.bp.

While the mechanism for loading prebuilt nanoapps is platform-specific, the CHRE
framework generally follows these steps at boot time:

1. When CHRE starts up, the CHRE daemon process running on the AP reads the
   configuration file `/vendor/etc/chre/preloaded_nanoapps.json`, which contains
   the list of nanoapps that should be automatically loaded.

2. For each nanoapp in the JSON file, the CHRE daemon reads the `.napp_header`
   from storage, and sends a message to CHRE requesting it to load the nanoapp.

3. The platform layer of the CHRE framework handles the requests by loading,
   authenticating, linking, and starting the nanoapp.

4. CHRE initialization proceeds (it is important for all preloaded nanoapps to
   be included at the first moment list query command can be processed, to
   avoid race conditions leading to clients believing that a preloaded nanoapp
   is missing).

This path is most commonly used to deploy nanoapps to production, as the entire
device software can be validated together without external dependencies, while
also preserving the ability to update nanoapps independent from other components
in the system.

### Fully Dynamic Nanoapps

At the binary level, a preloaded nanoapp and fully dynamic nanoapp are
identical. The key difference is where they are stored and how they are
initially loaded into CHRE, and potentially how metadata is handled. In most
cases, preloaded nanoapps will use a separate `.napp_header` file with metadata
and `.so` file for the actual binary, a fully dynamic nanoapp has the header
prepended to the binary, and carries the `.napp` file type suffix. In other
words, the command `cat my_nanoapp.napp_header my_nanoapp.so > my_nanoapp.napp`
can be used to create a fully dynamic nanoapp file from these components.

Instead of being stored on the device filesystem, fully dynamic nanoapps can be
loaded at any time after initialization using the
`ContextHubManager.loadNanoApp()` Java API. This allows nanoapps to be
updated/delivered by an APK, outside of a full Android system update (OTA).

This mechanism is used to dynamically load and unload test nanoapps, but can
also be used for production nanoapps.

## Other Nanoapp Types

Some platforms support loading nanoapps into multiple tiers of memory, for
example low-power tightly coupled memory (TCM, usually SRAM), versus a
higher-power but higher-capacity memory bank (such as DRAM). This distinction is
normally made at the build target variant level.

CHRE also supports the concept of a *system nanoapp*, which is a nanoapp whose
purpose is to accomplish some low-level, device-specific functionality that is
purely beneath the HAL level. System nanoapps are therefore hidden from the
nanoapp list at the HAL. This property is controlled by setting the
`NANOAPP_IS_SYSTEM_NANOAPP` variable in the nanoapp Makefile.

## Example AOSP Nanoapps

Some basic nanoapps can be found in the `apps/` folder, which are used for test
purposes, as well as to demonstrate how to use the CHRE APIs.

### FeatureWorld Nanoapps

The *FeatureWorld* nanoapps each exercise a part of the CHRE API, and print
results/output to `chreLog`. An overview of a few of the key FeatureWorld
nanoapps is given below:

* `hello_world`: While not technically a FeatureWorld nanoapp, it’s generally
  the first nanoapp to be tried, and it simply outputs a log message when it
  starts and ends, and upon any event received.

* `message_world`: Exercises host messaging functionality. Typically used in
  conjunction with `host/common/test/chre_test_client.cc` (see
  `sendMessageToNanoapp()` in that file).

* `sensor_world`: Enables sensors and prints the samples it receives. This
  nanoapp is typically customized prior to executing, for example to control
  which sensors it will enable. It also supports a “break it” mode which
  stresses the system by enabling/disabling sensors frequently.

* `host_awake_world`: Used to help validate functionality used for
  opportunistically sending messages to the AP when it is awake.

### Stress Test Nanoapps

These nanoapps help stress test the CHRE framework. They include:

* `audio_stress_test`: Repeatedly enables and disables an audio source,
  verifying that it continues to provide data as expected.

* `sensor_world`: Contains a “break it” mode which repeatedly enables, disables,
  and reconfigures sensors.

* `spammer`: Sends a constant stream of messages and events to stress test the
  queueing system.

* `unload_tester`: Used in conjunction with the spammer nanoapp to verify that
  unloading a nanoapp with pending events/messages completes successfully. Note
  that this nanoapp references internal framework functions (e.g.
  `EventLoopManager::deferCallback()`) to accomplish its functionality, which is
  generally only permissible for testing purposes.

### Power Test

The `power_test` nanoapp is intended to be used in conjunction with special
hardware that directly measures the power usage of the system and/or its
components. This nanoapp is intended to be used with its host-side client,
`chre_power_test_client`, to create some activity at the CHRE API level which
can then be measured. For example, running `./chre_power_test_client wifi enable
5000000000` will configure the `power_test` nanoapp to request a WiFi scan every
5 seconds - the power monitoring equipment can then be used to determine the
power cost of performing a WiFi scan from CHRE. Typically this is done after
unloading all other nanoapps in the system (which can be done via
`./chre_power_test_client unloadall`), and disabling all other functionality, to
get a clean power trace of purely the functionality exercised by the
`power_test` nanoapp.

Refer to `chre_power_test_client.cc` for more details, including a full listing
of all supported commands.

### Nanoapps Used with Java-based Test Suites

Nanoapps under `apps/test` are associated with a test suite, for example Context
Hub Qualification Test Suite (CHQTS), which is used to test that a given device
upholds the requirements of the CHRE API. Much of the host-side Java code
associated with these nanoapps can be found in the `java/` folder.
