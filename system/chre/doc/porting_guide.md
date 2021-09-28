# CHRE Framework Porting Guide

[TOC]

CHRE achieves portability and extensibility across platforms by defining
interfaces that the platform needs to implement. These interfaces provide
dependencies for the common CHRE code that are necessarily platform-specific.
Additionally, platform code calls into common code to ferry events from
underlying subsystems to nanoapps.

This section gives an overview of the steps one should take to add support for a
new platform in the CHRE reference implementation.

## Directory Structure

CHRE platform code can be broadly categorized as follows.

### Platform Interfaces

Files under `platform/include` serve as the interface between common code in
`core/` and other platform-specific code in `platform/<platform_name>`.  These
files are considered common and should not be modified for the sake of
supporting an individual platform.

### Shared Platform Code

Located in `platform/shared/`, the code here is part of the platform layer’s
responsibilities, but is not necessarily specific to only one platform. In other
words, this code is likely to be re-used by multiple platforms, but it is not
strictly necessary for a given platform to use it.

### Platform-specific Code

Files under `platform/<platform_name>` are specific to the underlying software
of a given platform, for example the APIs which are used to access functionality
like sensors, the operating system, etc. To permit code reuse, the CHRE platform
layer for a given device may be composed of files from multiple
`<platform_name>` folders, for example if the same sensor framework is supported
across multiple OSes, there may be one folder that provides the components that
are specific to just the OS.

Platform-specific code can be further subdivided into:

* **Source files**: normally included at
  `platform/<platform_name>/<file_name>.cc`, but may appear in a subdirectory

* **Headers which are includable by common code**: these are placed at
  `platform/<platform_name>/include/chre/target_platform/<file_name>.h`, and are
  included by *Platform Interfaces* found in `platform/include` and provide
  inline or base class definitions, such as `mutex_base_impl.h` and
  `platform_sensor_base.h` respectively, or required macros

* **Fully platform-specific headers**: these typically appear at
  `platform/<platform_name>/include/chre/platform/<platform_name/<file_name>.h`
  and may only be included by other platform-specific code

## Open Sourcing

Partners who add support for a new platform are recommended to upstream their
code to
[AOSP](https://source.android.com/setup/contribute#contribute-to-the-code).
This helps ensure that details of your platform are considered by the team that
maintains the core framework, so any changes that break compilation are
addressed in a timely fashion, and enables you to receive useful code review
feedback to improve the quality of your CHRE implementation. Please reach out
via your TAM to help organize this effort.

If some parts of a platform’s CHRE implementation must be kept closed source,
then it is recommended to be kept in a separate Git project (under vendor/ in
the Android tree). This vendor-private code can be integrated with the main CHRE
build system through the `CHRE_VARIANT_MK_INCLUDES` variable. See the build
system documentation for more details.

## Recommended Steps for Porting CHRE

When starting to add support for a new platform in the CHRE framework, it’s
recommended to break the task into manageable chunks, to progressively add more
functionality until the full desired feature set is achieved. An existing
platform implementation can be referenced to create empty stubs, and then
proceed to add implementations piece by piece, testing along the way.

CHRE provides various test nanoapps in `apps/` that exercise a particular
feature that the platform provides. These are selectively compiled into the
firmware statically via a `static_nanoapps.cc` source file.

With this in mind, it is recommended to follow this general approach:

1. Create a new platform with only empty stubs, with optional features (like
   `CHRE_GNSS_SUPPORT_ENABLED`) disabled at build-time

2. Work on updating the build system to add a new build target and achieve
   successful compilation and linking (see the build system documentation for
   details)

3. Implement base primitives from `platform/include`, including support for
   mutexes, condition variables, atomics, time, timers, memory allocation, and
   logging

4. Add initialization code to start the CHRE thread

5. Add static nanoapp initialization support (usually this is just a copy/paste
   from another platform)

6. Confirm that the ‘hello world’ nanoapp produces the expected log message (if
   it does, huzzah!)

7. Complete any remaining primitives, like assert

8. Implement host link and the daemon/HAL on the host (AP) side, and validate it
   with a combination of the message world nanoapp and the host-side test code
   found at `host/common/test/chre_test_client.cc`

At this stage, the core functionality has been enabled, and further steps should
include enabling dynamic loading (described in its own section below), and the
desired optional feature areas, like sensors (potentially via their respective
PALs, described in the next section).

## Implementing the Context Hub HAL

The Context Hub HAL (found in the Android tree under
`hardware/interfaces/contexthub`) defines the interface between Android and the
underlying CHRE implementation, but as CHRE is implemented on a different
processor from the HAL, the HAL is mostly responsible for relaying messages to
CHRE. This project includes an implementation of the Context Hub HAL under
`host/hal_generic` which pairs with the CHRE framework reference implementation.
It converts between HAL API calls and serialized flatbuffers messages, using the
host messaging protocol defined under `platform/shared` (platform
implementations are able to choose a different protocol if desired, but would
require a new HAL implementation), and passes the messages to and from the CHRE
daemon over a socket. The CHRE daemon is in turn responsible for communicating
directly with CHRE, including common functionality like relaying messages to and
from nanoapps, as well as device-specific functionality as needed. Some examples
of CHRE functionality that are typically implemented with support from the CHRE
daemon include:

* Loading preloaded nanoapps at startup
* Passing log messages from CHRE into Android logcat
* Determining the offset between `chreGetTime()` and Android’s
  `SystemClock.elapsedRealtimeNanos()` for use with
  `chreGetEstimatedHostTimeOffset()`
* Coordination with the SoundTrigger HAL for audio functionality
* Exposing CHRE functionality to other vendor-specific components (e.g. via
  `chre::SocketClient`)

When adding support for a new platform, a new HAL implementation and/or daemon
implementation on the host side may be required. Refer to code in the `host/`
directory for examples.

## Implementing Optional Feature Areas (e.g. PALs)

CHRE provides groups of functionality called *feature areas* which are
considered optional from the perspective of the CHRE API, but may be required to
support a desired nanoapp. CHRE feature areas include sensors, GNSS, audio, and
others. There are two ways by which this functionality can be exposed to the
common CHRE framework code: via the `Platform<Module>` C++ classes, or the C PAL
(Platform Abstraction Layer) APIs. It may not be necessary to implement all of
the available feature areas, and they can instead be disabled if they won’t be
implemented.

The Platform C++ Classes and PAL APIs have extensive documentation in their
header files, including details on requirements. Please refer to the headers for
precise implementation details.

### Platform C++ Classes vs. PAL APIs

Each feature area includes one or more `Platform<Module>` classes which the
common framework code directly interacts with. These classes may be directly
implemented to provide the given functionality, or the shim to the PAL APIs
included in the `shared` platform directory may be used. PALs provide a C API
which is suitable for use as a binary interface, for example between two dynamic
modules/libraries, and it also allows for the main CHRE to platform-specific
translation to be implemented in C, which may be preferable in some cases.

Note that the PAL APIs are binary-stable, in that it’s possible for the CHRE
framework to work with a module that implements a different minor version of the
PAL API, full backwards compatibility (newer CHRE framework to older PAL) is not
guaranteed, and may not be possible due to behavioral changes in the CHRE API.
While it is possible for a PAL implementation to simultaneously support multiple
versions of the PAL API, it is generally recommended to ensure the PAL API
version matches between the framework and PAL module, unless the source control
benefits of a common PAL binary are desired.

This level of compatibility is not provided for the C++ `Platform<Module>`
classes, as the CHRE framework may introduce changes that break compilation. If
a platform implementation is included in AOSP, then it is possible for the
potential impact to be evaluated and addressed early.

### Disabling Feature Areas

If a feature area is not supported, setting the make variable
`CHRE_<name>_SUPPORT_ENABLED` to false in the variant makefile will avoid
inclusion of common code for that feature area. Note that it must still be
possible for the associated CHRE APIs to be called by nanoapps without crashing
- these functions must return an appropriate response indicating the lack of
  support (refer to `platform/shared/chre_api_<name>.cc` for examples).

### Implementing Platform C++ Classes

As described in the CHRE Framework Overview section, CHRE abstracts common code
from platform-specific code at compile time by inheriting through
`Platform<Module>` and `Platform<Module>Base` classes. Platform-specific code
may retrieve a reference to other objects in CHRE via
`EventLoopManagerSingleton::get()`, which returns a pointer to the
`EventLoopManager` object which contains all core system state. Refer to the
`Platform<Module>` header file found in `platform/include`, and implementation
examples from other platforms for further details.

### Implementing PALs

PAL implementations must only use the callback and system APIs provided in
`open()` to call into the CHRE framework, as the other functions in the CHRE
framework do not have a stable API.

If a PAL implementation is provided as a dynamic module in binary form, it can
be linked into the CHRE framework at build time by adding it to
`TARGET_SO_LATE_LIBS` in the build variant’s makefile - see the build system
documentation for more details.

### PAL Verification

There are several ways to test the PAL implementation beyond manual testing.
Some of them are listed below in increasing order of the amount of checks run by
the tests.

1.  Use the FeatureWorld apps provided under the `apps` directory to exercise
the various PAL APIs and verify the CHRE API requirements are being met

2. Assuming the platform PAL implementations utilize CHPP and can communicate
from the host machine to the target chipset, execute `run_pal_impl_tests.sh` to
run basic sanity checks on the PAL

3.  Execute tests (see Testing section for details)

## Dynamic Loading Support

CHRE requires support for runtime loading and unloading of nanoapp binaries.
There are several advantages to this approach:

* Decouples nanoapp binaries from the underlying system - can maintain and
  deploy a single nanoapp binary across multiple devices, even if they support
  different versions of Android or the CHRE API

* Makes it possible to update nanoapps without requiring a system reboot,
  particularly on platforms where CHRE runs as part of a statically compiled
  firmware

* Enables advanced capabilities, like staged rollouts and targeted A/B testing

While dynamic loading is a responsibility of the platform implementation and may
already be a part of the underlying OS/system capabilities, the CHRE team is
working on a reference implementation for a future release. Please reach out via
your TAM if you are interested in integrating this reference code prior to its
public release.
