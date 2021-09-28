# Design for Compatibility

[TOC]

Compatibility is an important attribute of CHRE, which is accomplished through a
combination of thoughtful API and framework design. When we refer to
compatibility within the scope of CHRE, there are two main categories:

* **Code compatibility**, which means that a nanoapp can be recompiled to run on
  a new platform without needing any code changes. CHRE provides this
  cross-device compatibility for all nanoapps which are written in a supported
  programming language (C99 or C++11), and reference only the standard CHRE APIs
  and mandatory standard library elements (or have these standard library
  functions statically linked into their binary).

* **Binary compatibility**, which means that a nanoapp binary which has been
  compiled against a particular version of the CHRE API can run on a CHRE
  framework implementation which was compiled against a different version of the
  API. This is also called *cross-version compatibility*. Note that this does
  *not* mean that a nanoapp compiled against one version of the CHRE API can be
  compiled against a different version of the CHRE API without compiler errors -
  although rare, compile-time breakages are permitted with sufficient
  justification, since nanoapp developers can update their code at the time they
  migrate to the new API version.

This section provides an overview of the mechanisms used to ensure
compatibility.

## CHRE API

The CHRE API is a native C API that defines the interface between a nanoapp and
any underlying CHRE implementation to provide cross-platform and cross-version
compatibility. It is designed to be supportable even in very memory-constrained
environments (total system memory in the hundreds of kilobytes range), and is
thoroughly documented to clearly indicate the intended behavior.

The CHRE API follows [semantic versioning](https://semver.org) principles to
maintain binary compatibility. In short, this means that the minor version is
incremented when new features and changes are introduced in a backwards
compatible way, and the major version is only incremented on a
compatibility-breaking change. One key design goal of the CHRE API is to avoid
major version changes if at all possible, through use of
compatibility-preserving code in the framework and Nanoapp Support Library
(NSL).

Minor version updates to the CHRE API typically occur alongside each Android
release, but the CHRE version and Android version are not intrinsically related.
Nanoapps should be compiled against the latest version to be able to use any
newly added features, though nanoapp binaries are compatible across minor
version changes.

### API Compatibility Design Principles

API design principles applied within CHRE to ensure compatibility include the
following (not an exhaustive list). These are recommended to be followed for any
vendor-specific API extensions as well.

* Functionality must not be removed unless it was optional at the time of
  introduction, for example as indicated by a capabilities flag (an exception
  exists if it has no impact on the regular functionality of a nanoapp, for
  example a feature that only aids in debugging)
* Reserved fields must be set to 0 by the sender and ignored by the recipient
* Fields within a structure must not be reordered - new fields may only be
  introduced by reclaiming reserved fields (preferred), or adding to the end of
  a structure
* When reclaiming a reserved field, the default value of 0 must indicate a
  property that is guaranteed to hold for previous API versions, or “unknown”
* Arguments to a function must not be added or removed - introduce a new
  function instead
* The meaning of constants (e.g. event types) must never be changed, but may be
  deprecated and eventually replaced

## Binary Backward Compatibility and the NSL

This is where we want a nanoapp compiled against e.g. v1.2 to run on a CHRE v1.1
or older implementation. This is done through a combination of runtime feature
discovery, and compatibility behaviors included in the Nanoapp Support Library
(NSL).

Runtime feature discovery involves a nanoapp querying for the support of a
feature (e.g. RTT support indicated in `chreWifiGetCapabilities()`, or querying
for a specific sensor in `chreSensorFindDefault()`), which allows it determine
whether the associated functionality is expected to work. The nanoapp may also
query `chreGetApiVersion()` to find out the version of the CHRE API supported by
the platform it is running on. If a nanoapp has a hard requirement on some
missing functionality, it may choose to return false from `nanoappStart()` to
abort initialization.

However, a CHRE implementation cannot anticipate all future API changes and
automatically provide compatibility. So the NSL serves as a transparent shim
which is compiled into the nanoapp binary to ensure this compatibility. For
example, a nanoapp compiled against v1.2 must be able to reference and call
`chreConfigureHostSleepStateEvents()` when running on a CHRE v1.1 or earlier,
although such a function call would have no effect in that case. Typical dynamic
linking approaches would find an unsatisfied dependency and fail to load the
nanoapp, even if it does not actually call the function, for example by wrapping
it in a condition that first checks the CHRE version. In
`platform/shared/nanoapp/nanoapp_support_lib_dso.cc`, this is supported by
intercepting CHRE API function calls and either calling through to the
underlying platform if it’s supported, or replacing it with stub functionality.

Along similar lines, if new fields are added to the end of a structure without
repurposing a reserved field in an update to the CHRE API, as was the case with
`bearing_accuracy` in `chreGnssLocationEvent`, the nanoapp must be able to
reference the new field without reading uninitialized memory. This is enabled by
the NSL, which can intercept the event, and copy it into the new, larger
structure, and set the new fields to their default values.

Since these NSL compatibility behaviors carry some amount of overhead (even if
very slight), they can be disabled if it is known that a nanoapp will never run
on an older CHRE version. This may be the case for a nanoapp developed for a
specific device, for example. The NSL may also limit its compatibility range
based on knowledge of the API version at which support for given hardware was
introduced. For example, if a new hardware family first added support for the
CHRE framework at API v1.1, then NSL support for v1.0 is unnecessary.

Outside of these cases, the NSL must provide backwards compatibility for at
least 3 previous versions, and is strongly recommended to provide support for
all available versions. This means that if the first API supported by a target
device is v1.0, then a nanoapp compiled against API v1.4 must have NSL support
for v1.1 through v1.4, and should ideally also support v1.0.

## Binary Forward Compatibility and Framework Requirements

Conversely, this is where we want a nanoapp compiled against e.g. v1.1 to run
against CHRE v1.2 or later implementations. The NSL cannot directly provide this
kind of compatibility, so it must be ensured through a combination of careful
CHRE API design, and compatibility behaviors in the CHRE framework.

Similar to how Android apps have a “target SDK” attribute, nanoapps have a
“target API version” which indicates the version of the CHRE API they were
compiled against. The framework can inspect this value and provide compatibility
behavior as needed. For example, `chreGetSensorInfo()` populates memory provided
by the nanoapp with information about a given sensor.  In CHRE API v1.1, this
structure was extended with a new field, `minInterval`. Therefore, the framework
must check if the nanoapp’s target API is v1.1 or later before writing this
field.

To avoid carrying forward compatibility code indefinitely, it is permitted for a
CHRE implementation to reject compatibility with nanoapps compiled against an
API minor version that is 2 or more generations older. For example, a CHRE v1.4
implementation may reject attempts to load a nanoapp compiled against CHRE API
v1.2, but it must ensure compatibility with v1.3. However, providing the full
range of compatibility generally does not require significant effort on behalf
of the CHRE implementation, so this is recommended for maximum flexibility.

## ABI Stability

CHRE does not define a standard Application Binary Interface (ABI) - this is
left as a platform responsibility in order to provide maximum flexibility.
However, CHRE implementations must ensure that binary compatibility is
maintained with nanoapps, by choosing a design that provides this property. For
example, if a syscall-like approach is used (with the help of the NSL) to call
from position-independent nanoapp code into fixed-position CHRE API functions
(e.g. in a statically linked monolithic firmware image), syscall IDs and their
calling conventions must remain stable. It is not acceptable to require all
nanoapps to be recompiled to be able to work with an updated CHRE
implementation.

## CHRE PALs

Since the PAL APIs are largely based on the CHRE APIs, they benefit from many of
the compatibility efforts by default. Overall, binary compatibility in the CHRE
PAL APIs are less involved than the CHRE APIs, because we expect CHRE and CHRE
PAL implementations to be built into the vendor image together, and usually run
at the same version except for limited periods during development.  However, a
PAL implementation can simultaneously support multiple PAL API versions from a
single codebase by adapting its behavior based on the `requestedApiVersion`
parameter in the \*GetApi method, e.g.  `chrePalWifiGetApi()`.

## Deprecation Strategy

In general, nanoapp compilation may be broken in a minor update (given
sufficient justification - this is not a light decision to make, considering the
downstream impact to nanoapp developers), but deprecation of functionality at a
binary level occurs over a minimum of 2 years (minor versions). The general
process for deprecating a function in the CHRE API is as follows:

* In a new minor version `N` of the CHRE API, the function is marked with
  `@deprecated`, with a description of the recommended alternative, and ideally
  the justification for the deprecation, so nanoapp developers know why it's
  important to update.

  * Depending on the severity of impact, the function may also be tagged with a
    compiler attribute to generate a warning (e.g. `CHRE_DEPRECATED`) that may
    be ignored. Or, version `N` or later, an attribute or other method may be
    used to break compilation of nanoapps using the deprecated function, forcing
    them to update. If not considered a high severity issue and compatibility is
    easy to maintain, it is recommended to break compilation only in version
    `N+2` or later.

  * Binary compatibility at this stage must be maintained. For example the NSL
    should map the new functionality to the deprecated function when running on
    CHRE `N-1` or older, or a suitable alternative must be devised. Likewise,
    CHRE must continue to provide the deprecated function to support nanoapps
    built against `N-1`.

* Impacts to binary compatibility on the CHRE side may occur 2 versions after
  the function is made compilation-breaking for nanoapps, since forward
  compatibility is guaranteed for 2 minor versions. If done, the nanoapp must be
  rejected at load time.

* Impacts to binary compatibility on the nanoapp side may occur 4 versions after
  the function is marked deprecated (at `N+4`), since backward compatibility is
  guaranteed for 4 minor versions. If done, the NSL must cause `nanoappStart()`
  to return false on version `N` or older.

For example, if a function is marked deprecated in `N`, and becomes a
compilation-breaking error in `N+2`, then a CHRE implementation at `N+4` may
remove the deprecated functionality only if it rejects a nanoapp built against
`N+1` or older at load time. Likewise, the NSL can remove compatibility code for
the deprecated function at `N+4`. CHRE and NSL implementations must not break
compatibility in a fragmented, unpredictable, or hidden way, for example by
replacing the deprecated function with a stub that does nothing. If it is
possible for CHRE and/or the NSL to detect only nanoapps that use the deprecated
functionality, then it is permissible to block loading of only those nanoapps,
but otherwise this must be a blanket ban of all nanoapps compiled against the
old API version.
