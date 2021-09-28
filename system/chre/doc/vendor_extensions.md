# Extending CHRE with Vendor-specific Functionality

[TOC]

The CHRE framework is easily extensible with no modifications to the core
framework. Depending on the goals of the new API, one or more of the following
steps must be performed. At a high-level, to add a new vendor-specific API to
CHRE, one must:

1. Define new APIs in a header that can be referenced by both platform CHRE
   framework code and vendor-specific nanoapps.

2. Expose the new APIs from the framework to nanoapps, and connect them to a new
   module to provide the desired functionality

3. Integrate the new module with existing CHRE framework features, e.g. the
   event subsystem, to provide complete functionality that fits within the
   existing CHRE conventions

It's best to refer to existing standard CHRE API feature areas, such as
`chre/wifi.h` and `WifiRequestManager`, and follow a similar design where
possible.

## Defining the API

To prevent collision with future common CHRE API definitions, vendor extensions
must not use the plain ‘chre’ prefix followed by a capitalized letter. Instead,
it’s recommended to prefix the APIs with the vendor’s name as lowercase. For
example, if your company name is XYZ Semiconductor and you’re defining a new
‘widget’ API, it’s recommended to use a naming scheme like
`chrexyzWidget<FunctionName>()`, and included indirectly via `#include
<chre_xyz.h>` or directly via `<chre_xyz/widget.h>`. The equivalent C++
namespace would be `::chre::xyz`.

There are reserved ranges for vendor/implementation-specific event types
(starting from `CHRE_EVENT_INTERNAL_EXTENDED_FIRST_EVENT`), and other cases
where vendors may wish or need to define a custom value in an existing field. To
prevent collision with future versions of the CHRE API, vendor extensions must
only use values within vendor-reserved ranges. If you would like to add a new
value to an existing field for a vendor extension and a vendor-reserved range
does not already exist, please reach out to the CHRE team for guidance -
solutions may involve creating a new reserved range in the common CHRE API, or
providing advice on a different method of defining the API.

Vendors can only add on to the CHRE API - existing APIs must not be changed. Do
not modify core CHRE definitions, for example by adding on fields to common
structures, re-using event types, repurposing fields that are reserved for
future use, etc.

It’s recommended that any vendor extensions consider compatibility when
designing it - see the Compatibility section for API design guidelines.

If this API is intended to be open-sourced, it should be added to
`platform/<platform_name>/extensions/include`. Otherwise, it’s suggested that
the API be placed outside of the CHRE tree, in a separate Git project under
`vendor/` in the Android tree, to avoid potential conflicts when upgrading to a
new version of CHRE.

### Build Customization

As part of the CHRE framework build system, the `CHRE_VARIANT_MK_INCLUDES`
environment variable can be used to inject an external `.mk` file into the
top-level build without any source code changes in the system/chre project.
Alternatively, if open sourcing, the `platform.mk` file should contain the
additions needed to support the new vendor API. Refer to the CHRE framework
build documentation for further details.

To expose the new functionality to nanoapps, it’s recommended to create a single
`.mk` file that adds the necessary `COMMON_CFLAGS` entries (and potentially
other build configuration). For example, create a `chrexyz.mk` file which
nanoapps should include in their Makefile prior to including
`$(CHRE_PREFIX)/build/nanoapp/app.mk`.

## Threading Model

Interactions with a nanoapp always happen from within the CHRE thread that runs
the EventLoop, so vendor extension code does not need to worry about race
conditions due to multiple nanoapps calling into APIs, and likewise nanoapps do
not need to worry about race conditions in its callbacks/handlers. However, it
is common for a platform module to receive data in a callback on another thread.
In that case, it is recommended to use `EventLoopManager::deferCallback()` to
pass the incoming data to the CHRE thread for processing, as opposed to using
mutexes or other synchronization primitives, to avoid multithreading-related
issues that can arise in rare conditions. Further, note that most of the core
CHRE functionality is only safe to call from within the CHRE thread (other than
posting an event, or methods that are explicitly marked as thread-safe).

## Initialization

Since the new API will not be part of the core framework, it won’t be attached
to `EventLoopManager` or initialized as part of `chre::init()` or
`EventLoopManagerSingleton::get()->lateInit()`, since vendor-extension APIs are
by definition not part of the common code. Instead, a separate singleton object
should be created, for example `chre::xyz::VendorExtensionManager`, and
platform-specific initialization code should invoke any necessary initialization
**after** `chre::init` is called, but **before** loading any static nanoapps or
invoking `EventLoop::run()` to ensure that nanoapps don’t begin interacting with
the API before its state is ready.

## Handling Nanoapp API Calls

Calls from a nanoapp into the CHRE framework first arrive in platform-specific
code (refer to the Framework Overview documentation for details). The first step
once an API call reaches the framework is usually to call
`EventLoopManager::validateChreApiCall(__func__)`. This fetches a pointer to the
`Nanoapp` object associated with the nanoapp that invoked the API, which will
fail if the API is called outside of the EventLoop thread context (see the
Threading Model above). From this point, the vendor extension singleton should
be used to invoke the appropriate functionality.

## Sending Events to Nanoapps

Vendor extension APIs that need to pass data to a nanoapp asynchronously should
use the event susbsystem, using the vendor-reserved event type range (starting
at `CHRE_EVENT_INTERNAL_EXTENDED_FIRST_EVENT` and extending to
`CHRE_EVENT_INTERNAL_LAST_EVENT`). Event types for a given vendor extension
should be globally unique and stable over time.

Synchronous API calls that can potentially block for periods greater than a few
milliseconds are discouraged, as these can prevent other nanoapps from
executing, and/or cause the pending event queue to grow excessively during
periods of high activity. Refer to the GNSS and WWAN APIs for design patterns
related to passing data to a nanoapp asynchronously, using custom event payloads
and/or `chreAsyncResult`.

Events can either be unicast to a nanoapp identified by its instance ID
(`Nanoapp::getInstanceId()`), or broadcast to all nanoapps registered for the
given event type - see `Nanoapp::registerForBroadcastEvent()` and
`Nanoapp::unregisterForBroadcastEvent()`.

Use `EventLoop::postEventOrDie()` or `EventLoop::postLowPriorityEventOrFree()`
(via `EventLoopManagerSingleton::get()->getEventLoop()`) to pass events to
nanoapps, depending on what error handling is desired in the case that the event
cannot be posted to the queue. Any memory referenced by `eventData` must not be
modified until `freeCallback` is invoked.
