*** aside
See also:
[Porting Guide](/doc/porting_guide.md) |
[Build System](/doc/framework_build.md) |
[Debugging](/doc/framework_debugging.md) |
[Testing](/doc/framework_testing.md) |
[Vendor Extensions](/doc/vendor_extensions.md)
***

# CHRE Framework Overview

[TOC]

The CHRE reference implementation (hereafter referred to just as "CHRE" or "the
CHRE framework") is developed primarily in C++11 using a modular object-oriented
approach that separates common code from platform-specific code. CHRE is an
event-based system, so CHRE is built around an event loop which executes nanoapp
code as well as CHRE system callbacks. Per the CHRE API, nanoapps can’t execute
in more than one thread at a time, so CHRE is structured around a single thread
that executes the event loop, although there may be other threads in the system
that support CHRE. The EventLoopManager is a Singleton object which owns the
main state of the CHRE framework, including EventLoop and \*Manager classes for
the various subsystems supported by CHRE.

To get a better understanding of code structure and how it weaves between common
and platform-specific components, it is helpful to trace the flow through a few
example scenarios. Note that this is not meant to be an exhaustive list of
everything that happens in each case (for that, refer to the code itself), but
rather an overview of key points to serve as an introduction.

## Loading a nanoapp via the HAL

There are multiple ways by which a nanoapp can be loaded (see the relevant
section below for details), but this example traces the flow for dynamically
loading a nanoapp that has been passed in via the Context Hub HAL's
`loadNanoapp()` method.

1. The nanoapp binary reaches the HAL implementation, and it is loaded into the
   processor where CHRE is running using a platform-specific method. While the
   path this takes can vary, one common approach is to transmit the binary into
   CHRE via the platform-specific HostLink implementation, then verify its
   digital signature, and parse the binary file format (e.g. ELF) to load and
   link the code.

2. Once the nanoapp code is loaded, the platform code calls
   `EventLoopManager::deferCallback()` to switch context to the main CHRE thread
   (if needed), so it can complete loading and starting the nanoapp.
   `deferCallback()` effectively posts an event to the main event loop which
   does not get delivered to any nanoapps. Instead, the purpose is to invoke the
   supplied callback from the CHRE thread once the event is popped off the
   queue.

3. The (platform-specific) callback finalizes the newly constructed `Nanoapp`
   object as needed, and passes it to `EventLoop::startNanoapp()` - this marks a
   transition from platform-specific to common code.

4. `EventLoop` takes ownership of the `Nanoapp` object (which is a composite of
   common and platform-specific data and functions, as described in the Platform
   Abstractions section), includes it in the collection of loaded nanoapps to
   execute in the main event loop, updates `mCurrentNanoapp` to reference the
   nanoapp it's about to execute, and calls into `PlatformNanoapp::start()`.

5. Since the mechanism of supporting dynamic linkage and position independent
   code can vary by platform, transferring control from the framework to a
   nanoapp is considered part of the platform layer. So
   `PlatformNanoapp::start()` performs any necessary tasks for this, and calls
   into the `nanoappStart()` function defined in the nanoapp binary.

## Invoking a CHRE API from a nanoapp

Let's assume the nanoapp we've loaded in the previous section calls the
`chreSensorConfigure()` CHRE API function within `nanoappStart()`:

1. The nanoapp invokes `chreSensorConfigure()` with parameters to enable the
   accelerometer.

2. The Nanoapp Support Library (NSL) and/or the platform's dynamic linking
   module are responsible for handling the transition of control from the
   nanoapp binary to the CHRE framework. This can vary by platform, but we'll
   assume that control arrives in the `chreSensorConfigure()` implementation in
   `platform/shared/chre_api_sensor.cc`.

3. `EventLoopManager::validateChreApiCall()` is invoked to confirm that this
   function is being called from the context of a nanoapp being executed within
   the event loop (since associating the API call with a specific nanoapp is a
   requirement of this API and many others, and the majority of the CHRE
   framework code is only safe to execute from within the main CHRE thread), and
   fetch a pointer to the current `Nanoapp` (i.e. it retrieves `mCurrentNanoapp`
   set previosly by `EventLoop`).

4. `SensorManager::setSensorRequest()` (via
   `EventLoopManager::getSensorRequestManager()`) is called to process the
   nanoapp’s request - we transition to common code here.

5. The request is validated and combined with other nanoapp requests for the
   same sensor to determine the effective sensor configuration that should be
   requested from the platform, and the nanoapp is registered to receive
   broadcast accelerometer sensor events.

6. `SensorRequestManager` calls into `PlatformSensorManager::configureSensor()`,
   which performs the necessary operations to actually configure the
   accelerometer to collect data.

7. Assuming success, the return value propagates back up to the nanoapp, and it
   continues executing.

## Passing an event to a nanoapp

Following the example from above, let's follow the case where an accelerometer
sample has been generated and is delivered to the nanoapp for processing.

1. Starting in platform-specific code, likely in a different thread, the
   accelerometer sample is received from the underlying sensor framework - this
   typically happens in a different thread than the main CHRE thread, and within
   the fully platform-specific `PlatformSensorManagerBase` class.

2. As needed, memory is allocated to store the sample while it is being
   processed, and the data is converted into the CHRE format: `struct
   chreSensorThreeAxisData`.

3. `SensorRequestManager::handleSensorDataEvent()` is invoked (common code) to
   distribute the data to nanoapps.

4. `SensorRequestManager` calls into `EventLoop` to post an event containing the
   sensor data to all nanoapps registered for the broadcast event type
   associated with accelerometer data, and sets `sensorDataEventFree()` as the
   callback invoked after the system is done processing the event.

5. `EventLoop` adds this event to its event queue and signals the CHRE thread.

6. Now, within the context of the CHRE thread, once the event loop pops this
   event off of its queue in `EventLoop::run()`, the `nanoappHandleEvent()`
   function is invoked (via `PlatformNanoapp`, as with `nanoappStart`) for each
   nanoapp that should receive the event.

7. Once the event has been processed by each nanoapp, the free callback
   (`sensorDataEventFree()`), is called to release any memory or do other
   necessary cleanup actions now that the event is complete.

## Platform Abstractions

CHRE follows the 'compile time polymorphism' paradigm, to allow for the benefits
of `virtual` functions, while minimizing code size impact on systems with tight
memory constraints.

Each framework module as described in the previous section is represented by a
C++ class in `core/`, which serves as the top-level reference to the module and
defines and implements the common functionality. This common object is then
composed with platform-specific functionality at compile-time. Using the
`SensorRequestManager` class as an example, its role is to manage common
functionality, such as multiplexing sensor requests from all clients into a
single request made to the platform through the `PlatformSensorManager` class,
which in turn is responsible for forwarding that request to the underlying
sensor system.

While `SensorRequestManager` is fully common code, `PlatformSensorManager` is
defined in a common header file (under `platform/include/chre/platform`), but
implemented in a platform-specific source file. In other words, it defines the
interface between common code and platform-specific code.

`PlatformSensorManager` inherits from `PlatformSensorManagerBase`, which is
defined in a platform-specific header file, which allows for extending
`PlatformSensorManager` with platform-specific functions and data. This pattern
applies for all `Platform<Module>` classes, which must be implemented for all
platforms that support the given module.

Selection of which `PlatformSensorManager` and `PlatformSensorManagerBase`
implementation is instantiated is controlled by the build system, by setting the
appropriate include path and source files. This includes the path used to
resolve include directives appearing in common code but referencing
platform-specific headers, like `#include
"chre/target_platform/platform_sensor_manager_base.h"`.

To ensure compatibility across all platforms, common code is restricted in how
it interacts with platform-specific code - it must always go through a common
interface with platform-specific implementation, as described above. However,
platform-specific code is less restricted, and can refer to common code, as well
as other platform code directly.

## Coding conventions

This project follows the [Google-wide style guide for C++
code](https://google.github.io/styleguide/cppguide.html), with the exception of
Android naming conventions for methods and variables. This means 2 space
indents, camelCase method names, an mPrefix on class members and so on.  Style
rules that are not specified in the Android style guide are inherited from
Google. Additionally, this project uses clang-format for automatic code
formatting.

This project uses C++11, but with two main caveats:

1. General considerations for using C++ in an embedded environment apply. This
   means avoiding language features that can impose runtime overhead, due to the
   relative scarcity of memory and CPU resources, and power considerations.
   Examples include RTTI, exceptions, overuse of dynamic memory allocation, etc.
   Refer to existing literature on this topic including this [Technical Report
   on C++ Performance](http://www.open-std.org/jtc1/sc22/wg21/docs/TR18015.pdf)
   and so on.

2. Full support of the C++ standard library is generally not expected to be
   extensive or widespread in the embedded environments where this code will
   run. This means things like <thread> and <mutex> should not be used, in
   favor of simple platform abstractions that can be implemented directly with
   less effort (potentially using those libraries if they are known to be
   available).
