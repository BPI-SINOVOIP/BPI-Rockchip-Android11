# Context Hub Runtime Environment (CHRE)

This project contains the AOSP reference implementation of the Context Hub
Runtime Environment (CHRE), which is Android’s platform for developing always-on
applications, called *nanoapps*. CHRE runs in a vendor-specific processor that
is independent of the main applications processor that runs Android. This
enables CHRE and its nanoapps to be more power-efficient for use cases that
require frequent background processing of contextual data, like sensor inputs.
Nanoapps are written to the CHRE API, which is standardized across all
platforms, enabling them to be code-compatible across different devices.

The CHRE reference implementation (“CHRE framework”) is designed to be portable
across hardware and software platforms. While Android does not require a
particular implementation (only compliance to the contracts given in the CHRE
API) using the reference implementation helps to reduce the work needed to
support CHRE, while also ensuring more consistent behavior for scenarios that
can be difficult to enforce via testing.

## Navigating the docs

Use the navigation bar at the top and/or the links below to move through the
documentation. Raw files can also be found in the `/doc` folder.

* Documentation related to the CHRE framework:
  * [Framework Overview](/doc/framework_overview.md)
  * [Porting Guide](/doc/porting_guide.md)
  * [Build System](/doc/framework_build.md)
  * [Debugging](/doc/framework_debugging.md)
  * [Testing](/doc/framework_testing.md)
  * [Vendor Extensions](/doc/vendor_extensions.md)
* Documentation related to nanoapps:
  * [Nanoapp Overview](/doc/nanoapp_overview.md)
  * [Developer Guide](/doc/nanoapp_developer_guide.md)
  * [Interacting with Nanoapps](/doc/nanoapp_clients.md)
* General documentation:
  * [Compatibility Design](/doc/compatibility.md)
  * [Contributing](/doc/contributing.md)

The CHRE API and PAL API are also extensively documented using Doxygen syntax.
Run `doxygen` in the same folder as `Doxyfile` to generate a browseable HTML
version of the documentation.

## Navigating the code

This repository (system/chre) contains an assortment of code, structured as
follows:

- ``apps/``: Public nanoapp source code, including sample nanoapps intended to
  showcase how to use the CHRE APIs, and test nanoapps used to validate API
  functionality
- ``build/``: Files related to CHRE’s Makefile-based build system, which
  supports building the CHRE framework and nanoapps using a configurable
  toolchain
- ``chpp/``: Context Hub Peripheral Protocol (CHPP) source code - see the nested
  README and associated documentation for details
- ``chre_api/``: Contains the official definition of the CHRE API (current and
  past versions), which all CHRE implementations must adhere to
- ``core/``: Common CHRE framework code, which is applicable to every platform
  (contrast to ``platform/``)
- ``doc/``: Contains documentation for the CHRE framework and associated topics
- ``external/``: Code developed primarily outside of AOSP which (can be)
  leveraged by CHRE, and is potentially customized for CHRE (e.g. flatbuffers)
- ``host/``: Reference code which supports the CHRE implementation, but runs on
  the applications processor (“host”), for example the Context Hub HAL
- ``java/``: Java test code used in conjunction with test nanoapps
- ``pal/``: The binary-stable Platform Abstraction Layer (PAL) C API definitions
  and tests (these PALs may optionally be used by the platform implementation)
- ``platform/``: Code related to the implementation of CHRE on a particular
  platform/device (compare to ``core/``), divided into sub-folders as follows:
   - ``platform/include``: The interface between common code in ``core/`` and
     platform-specific code implemented elsewhere in ``platform/``
   - ``platform/shared``: Code that may apply to multiple platforms, but is not
     necessarily applicable to _all_ platforms (as in ``core/``). For example,
     multiple platforms may choose to use the binary-stable PAL interface - this
     folder contains source files translating from the C++ platform abstractions
     to the C PAL APIs.
   - ``platform/<platform_name>``: Code specific to the platform indicated by
     ``platform_name``
- ``util/``: Utility code that is not platform-specific, but not part of the
  core framework implementation. Includes code that is usable by nanoapps.
- ``variant/``: Code/configuration that is specific to a particular device (more
  detail on variants can be found below). For example, multiple generations of a
  given chip may use the same platform code, but the code may be configured on a
  per-device basis in the build via ``variant.mk``.

Code related to CHRE also exists in other parts of the Android tree, including:

- ``hardware/interfaces/contexthub/``: The Context Hub HAL definition
- ``frameworks/base/core/java/android/hardware/location/ContextHub*.java``: The
  APIs used by privileged apps to interact with CHRE and nanoapps
- ``frameworks/base/services/core/java/com/android/server/location/ContextHub*.java``:
  The Context Hub service implementation in system server

# Have Questions?

If you’re unable to find the answers you’re looking for in CHRE documentation
or are looking for specific guidance for your platform, device, or nanoapp,
please reach out to the CHRE team via your TAM or through the [Google Issue
Tracker](https://developers.google.com/issue-tracker).
