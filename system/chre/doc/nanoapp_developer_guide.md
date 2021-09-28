# Nanoapp Developer Guide

[TOC]

Since CHRE is an open platform, anyone can write nanoapps. However, deploying to
a device requires cooperation of the device manufacturer, because CHRE is a
security-sensitive trusted environment, similar to other low-level firmware, and
it typically has tight resource constraints. This section assumes you have a
basic understanding of what a nanoapp is (if not, see the Nanoapp Overview
section), and provides some simple instructions to help you get started with
developing your own nanoapp.

## Getting Started

When starting a new nanoapp, it’s helpful to start with the skeleton of an
existing nanoapp. The simplest example can be found at `apps/hello_world`. Start
by copying this folder to the location where you will develop the nanoapp - it
can be outside of the `system/chre` project, for example in a vendor-specific
Git repository in the `vendor/` folder of the Android tree.

If you don’t plan to use this nanoapp as a *static nanoapp* (see the Nanoapp
Overview for details), remove the `hello_world.mk` file and delete the code
blocks wrapped in `#ifdef CHRE_NANOAPP_INTERNAL`. Rename the remaining files to
match your nanoapp.

### Picking a Nanoapp ID

Nanoapps are uniquely identified by a 64-bit number. The most significant 5
bytes of this number are the vendor identifier, and the remaining bytes identify
the nanoapp within the vendor’s namespace. The vendor identifier is usually
devised from an ASCII representation of the vendor’s name, for example Google
uses 0x476F6F676C (“Googl”). The remaining portion of the ID is typically just
an incrementing value for each nanoapp.

Refer to `system/chre/chre_api/include/chre_api/chre/common.h` and
`util/include/chre/util/nanoapp/app_id.h` for some examples and utilities.

Be sure to pick a unique nanoapp ID when creating a new nanoapp.

### Picking a Language

CHRE guarantees support for nanoapps written in C99 or C++11, though not all
standard library functions are supported (see below for details). For a
device-specific nanoapp, additional programming languages/versions *may* be
supported, but this can impact portability.

### Building the Nanoapp Binary

While it’s possible to build a nanoapp with a different build system, just as it
is for the CHRE framework, it’s recommended to use the common build system
included in this project, as it makes it easy to support a variety of target
platforms. The rest of this section assumes you are using the CHRE build system
to create a non-static nanoapp.

Update the `Makefile` in your nanoapp’s directory to:

* Define nanoapp metadata, including:
   * `NANOAPP_NAME`: sets the output filename of the binary
   * `NANOAPP_ID`: 64-bit identifier, in hexadecimal format
   * `NANOAPP_VERSION`: 32-bit version, in hexadecimal format (see versioning
     section below)
   * `NANOAPP_NAME_STRING`, `NANOAPP_VENDOR_STRING`: human-readable strings for
     the name of the nanoapp and vendor, respectively
   * `NANOAPP_IS_SYSTEM_NANOAPP`: 0 or 1 (see Nanoapp Overview)
* Populate `COMMON_SRCS` with the C or C++ source files to compile
* Populate `COMMON_CFLAGS` with compiler flags, like additional include paths
* Include any additional `.mk` files for vendor extensions, etc. before `app.mk`

Refer to `build/nanoapp/app.mk` for full details.

The nanoapp can then be built using a command like ``OPT_LEVEL=s make
<build_target> -j`nproc` `` (see the CHRE Framework Build System section for
details on build targets), which will produce build artifacts at
`out/<build_target>/<nanoapp_name>.*`.

### Loading onto a Device

Exact steps to load a nanoapp binary can vary by device, but for developing a
preloaded nanoapp, this typically involves the following steps:

* Perform any needed post-processing of the nanoapp binary to permit it to be
  loaded (such as signing with a development or production key)
* Write the binary to the device’s storage (for example, using `adb push`)
* Update `preloaded_nanoapps.json` or other configuration as needed, so that
  CHRE knows to load the new nanoapp
* Restart CHRE to reload all nanoapps, including the new one

## Nanoapp Versioning

While not strictly enforced, nanoapps are recommended to follow the convention
of the CHRE framework and use [Semantic Versioning](http://semver.org). In the
case of a nanoapp, the key versioned “API” is considered the interface between
the client and nanoapp. Nanoapp versions are represented as a 32-bit integer,
where the most significant byte represents the major version, followed by one
byte for the minor version, and two bytes for the patch version.

## Using the CHRE API

The CHRE API is the key interface between each nanoapp and the underlying
system. Refer to the extensive API documentation in the header files at
`chre_api/include`, as well as usage of the APIs by sample nanoapps. The CHRE
API is normally included via `#include <chre.h>`.

## Utility Libraries

Some source and header files under `util` are specifically designed to aid in
nanoapp development, and others were initially created for use in the framework
but can be leveraged by nanoapps as well. In general, any source and header file
there that does **not** include a header from `chre/platform` (part of the
internal CHRE framework implementation details) may be used by a nanoapp, and
files within a subdirectory called `nanoapp` are specifically targeted for use
by nanoapps.

This includes `util/include/chre/util/nanoapp/log.h` (meant to be included via
`#include “chre/util/nanoapp/log.h”`), which provides macros like `LOGD` which
can be conditionally compiled, include a configurable prefix to help identify
the sender, and suppress double promotion warnings.

The utilities library also includes a number of container classes, which are
meant to mimic the C++ standard library, but with a lightweight, CHRE-compatible
implementation. This includes:

* `chre::DynamicVector`: an `std::vector` analogue
* `chre::FixedSizeVector`: accessed like `std::vector`, but only uses statically
  allocated memory
* `chre::ArrayQueue`: can be used as a circular buffer
* `chre::UniquePtr`: an `std::unique_ptr` analogue
* `chre::Optional`: an analogue to `std::optional` from C++17
* `chre::Singleton`: a container for a statically allocated object with explicit
  initialization and deinitialization (e.g. enables construction of a global
  object to be deferred until `nanoappStart()`)

## Interacting with the Host

Nanoapps can interact with one or more clients on the host (applications
processor) through a flexible binary message-passing interface. For simple
interactions in cases where the lowest memory footprint is desired, using only
the built-in message type field with no additional payload, or passing
hand-rolled packed C-style structures (e.g. using Java’s ByteBuffer on the
client side) can work, though this approach can be error-prone. Using a
well-defined serialization format, such as Protocol Buffers (see the Using
NanoPB section below) or FlatBuffers, is usually a better choice.

There are a few common tips to keep in mind when interacting with the host:

1. Nanoapp binaries are usually updated independently from client code - watch
out for compatibility issues arising from changes to the messaging protocol, and
use a serialization format like Protocol Buffers if possible.

2. Nanoapp messages to the host always wake it up if it’s asleep. If this is not
required, nanoapps are encouraged to batch their messages and opportunistically
send when the host wakes up for another reason (see
`chreConfigureHostSleepStateEvents()`).

3. After calling `chreSendMessageToHostEndpoint()`, ownership of the memory
associated with the message payload is assigned to the framework. Do not modify
it until the free callback is invoked.

4. Nanoapp messaging should be unicast, unless broadcast messaging is strictly
necessary. Design the messaging protocol such that the client initiates
communication, and save the host endpoint ID in the nanoapp to use when sending
replies.

## Interacting with Other Nanoapps

While most nanoapps are only concerned with providing functionality for a single
client on the host, it is possible for a nanoapp to provide services to other
nanoapps within CHRE. Similar to how nanoapps communicate with the host by
passing *messages*, nanoapps can communicate with one another by passing
*events* with arbitrary binary payload. Event IDs starting in the range
`CHRE_EVENT_FIRST_USER_VALUE` are reserved for this purpose.

Typically a nanoapp creates a *nanoapp client library* which other nanoapps can
include, which presents a simple, expressive API, and handles the implementation
details of passing events to the target nanoapp, and interpreting incoming
messages.

Refer to the functions defined in `chre/event.h` for more details.

## Using TensorFlow Lite for Microcontrollers

Many nanoapps use machine learning techniques to accomplish their functionality.
The CHRE build system has built-in support for integrating [TensorFlow Lite for
Microcontrollers](https://www.tensorflow.org/lite/microcontrollers) (TFLM) into
a nanoapp. Sync the TFLM sources, set `TFLM_PATH`, and define `USE_TFLM=true` in
your Makefile - see `apps/tflm_demo/README` for details and an example nanoapp.

## Using Nanopb

The CHRE build system has integrated support for using
[Nanopb](https://jpa.kapsi.fi/nanopb/) to provide support for [Protocol
Buffers](https://developers.google.com/protocol-buffers) in a nanoapp. To
integrate this into your nanoapp’s Makefile, first install and configure
dependencies:

* Sync the Nanopb source tree (e.g. from a release on GitHub), and define the
  `NANOPB_PREFIX` environment variable to its path
* Download and install the protobuf compiler `protoc` and make it available in
  your `$PATH`, or set the `PROTOC` environment variable

Then in your nanoapp’s Makefile, populate `NANOPB_SRCS` with the desired
`.proto` file(s). That’s it! Though some additional options/parameters are
available - see `build/nanopb.mk` for details.

## Nanoapp Development Best Practices

Even though CHRE aims to provide an environment for low-power and low-latency
contextual signal processing, these two are often conflicting goals. In
addition, CHRE is usually implemented in a resource-constrained environment with
limited memory available.

As it requires collaboration from all nanoapps to optimize their resource usage
for CHRE to live up to its promises, some best practices are provided here as
guidance for nanoapp development.

### Memory Efficiency

#### Avoid dynamic heap allocations where possible

As CHRE is designed in a resource-constrained environment, there is no guarantee
runtime memory allocation will succeed. In addition, dynamic heap allocations
make it difficult to estimate the memory usage in advance. Developers are
therefore encouraged to use static allocations where possible.

#### Be careful of stack usage

Unlike Linux’s default stack of 8MB that scales dynamically, CHRE only has a
fixed stack of limited size (8KB is typical). Ensure you keep any allocations to
an absolute minimum and any large allocations should go out of scope prior to
navigating deeper into a stack.

#### Prefer in-place algorithms

Prefer in-place algorithms over out-of-place ones where efficiency allows to
minimize additional memory requirements.

### Power Efficiency

#### Be opportunistic when possible

Examples include:

* If the host is asleep and doesn’t need to act on a nanoapp message
  immediately, buffer until it wakes up for another reason.
* Make a WiFi on-demand scan request only if the WiFi scan monitor doesn’t
  provide a scan result in time.

#### Batch data at the source where possible

By batching data at the source, it reduces the data delivery frequency and helps
keep CHRE asleep and improve power efficiency. Clients should make data requests
with the longest batch interval that still meets the latency requirement.
Examples include:

* Make a sensor data request with the longest ``latency`` possible.
* Make an audio data request with the longest ``deliveryInterval`` possible.

### Standard Library Usage

CHRE implementations are only required to support a subset of the standard C and
C++ libraries, as well as language features requiring run-time support. This
list is carefully considered to ensure memory usage and implementation
complexity are minimized. Following these principles, some features are
explicitly excluded due to their memory and/or extensive OS-level dependencies,
and others because they are supplanted by more suitable CHRE-specific APIs.
While not meant to be an exhaustive list and some platforms may differ, the
following standard library features are not meant to be used by nanoapps:

* C++ exceptions and run-time type information (RTTI)
* Standard library multi-threading support, including C++11 headers `<thread>`,
  `<mutex>`, `<atomic>`, `<future>`, etc.
* C and C++ Standard Input/Output libraries
* C++ Standard Template Library (STL)
* C++ Standard Regular Expressions library
* Dynamic memory allocation (`malloc`, `calloc`, `realloc`, `free`), and
  libraries that inherently use dynamic allocation, such as `std::unique_ptr`
* Localization and Unicode character support
* Date and time libraries
* Functions that modify normal program flow, including `<setjmp.h>`,
  `<signal.h>`, `abort`, `std::terminate`, etc.
* Accessing the host environment, including `system`, `getenv`, etc.
* POSIX or other libraries not included in the C99 or C++11 language standards

In many cases, equivalent functionality is available from CHRE API functions
and/or utility libraries. For example, `chreLog` may be used for debug logging,
where a more traditional program might use `printf`.

## Debugging

Similar to the framework debugging methods, each has its nanoapp counterpart to
support nanoapp debugging through the framework. Please see the Framework
Debugging section for reference/context.

### Logging

CHRE API `chreLog()` logs information into the system as part of the CHRE logs.
Normally this appears in logcat, but some platforms may route it to a different
logging system (a future version of the CHRE API is expected to make logcat
logging mandatory).

Nanoapps are encouraged to `#include "chre/util/nanoapp/log.h"` and use the
`LOGx()` macros defined therein, which requires these additional steps:

* Define `LOG_TAG` to a short, human-readable identifier for your nanoapp, as
  this gets prepended to logs
* Define `NANOAPP_MINIMUM_LOG_LEVEL` to a `CHRE_LOG_LEVEL_\*` value in your
  Makefile for compile time log level filtering - it’s recommended to use
  `CHRE_LOG_LEVEL_DEBUG` for development, and `CHRE_LOG_LEVEL_INFO` for release

See also the Framework Debugging section for more general guidance on logging in
CHRE.

### Debug Dump

When running on CHRE v1.4+, nanoapps can also append information to the CHRE
framework debug dump. Nanoapps interested in using this capability should call
`chreConfigureDebugDumpEvent(true)` in `nanoappStart()`, then when
`CHRE_EVENT_DEBUG_DUMP` is received in `nanoappHandleEvent()`, use
`chreDebugDumpLog()` to write human-readable output to the debug dump, which
appears in bug reports under the Context Hub HAL debug section. In the reference
CHRE framework implementation, nanoapp debug dumps have the nanoapp name and ID
automatically prepended, for example:

```
Nanoapp debug dumps:

 DebugDumpWorld 0x0123456789000011:
  Debug event count: 2
  Total dwell time: 92 us
```

Refer to the associated CHRE API documentation and Framework Debugging section
for more information.

### CHRE_ASSERT

To help catch programming errors or other unexpected conditions, nanoapps can
use the `CHRE_ASSERT` macro provided by `#include "chre/util/nanoapp/assert.h"`.
Keep in mind that if one nanoapp encounters an assertion failure, it most likely
will cause a reset of the processor where CHRE is running, impacting other
functionality (though this can vary by platform). Therefore, assertions are only
recommended to be used during development. Define the `CHRE_ASSERTIONS_ENABLED`
variable in your Makefile to `false` to disable assertions at compile time.
