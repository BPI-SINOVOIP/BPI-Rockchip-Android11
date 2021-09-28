# CHRE Framework Build System

[TOC]

The CHRE build system is based on Make, and uses a set of Makefiles that allow
building the CHRE framework for a variety of hardware and software architectures
and variants (e.g. different combinations of CPU and OS/underlying system). It
is also flexible to different build toolchains (though LLVM/Clang or GCC are
recommended), by abstracting out key operations to a few Make or environment
variables. While the CHRE framework source code may be integrated into another
build system, it can be beneficial to leverage the existing build system to
reduce maintenance burden when the CHRE framework is updated. Additionally, it
should be possible to build nanoapps from the CHRE build system (to have
commonality across devices), and the CHRE framework build shares configuration
with the nanoapp build.

By default, the output of the build is linked into both a static archive
`libchre.a` and a shared library `libchre.so`, though generally only one of the
two is used, depending on the target device details.

## Design

The CHRE build system was originally designed around the philosophy that a
vanilla invocation of `make` or `make all` should build any given nanoapp for
all targets. This allows for efficient use of the job scheduler in the Make
build system for multi-threaded builds and also promotes a level of separation
of concerns between targets (this is not enforced by any Make language
construct, merely convention). In practice, the CHRE build system is rarely used
to build multiple targets with one invocation of Make. However, the design goal
is carried forward for the variant support and separation of concerns between
targets.

All variant-specific compiler and linker flags are held under variables that
only apply to their specific target. This encourages developers to avoid leaking
build details between targets. This is important because not all compiler or
linker flags are compatible with all toolchains. For example: if a target uses a
compiler that does not support `-Wdouble-promotion`, but this were to be
enforced for all builds, then by definition this target would not compatible
with the CHRE build. The goal is for the CHRE build to be as flexible as
possible.

### Build Template

The CHRE build system is implemented using template meta-programming techniques.
A build template is used to create Make rules for tasks that are common to all
targets. This includes compiling C/C++/assembly sources, linking, nanoapp header
generation, etc. The rationale behind this approach is to reduce boilerplate
when adding support for a new build target.

The build template is located at `build/build_template.mk`, and is documented
with all the variables used to generate build targets, like `TARGET_CFLAGS`.

## Build Targets (Variants)

Compiling the framework for different devices is done by specifying the build
target when invoking `make`. Conventionally, build targets consist of three
parts: (software) vendor, architecture and variant and follow the
`<vendor>_<arch>_<variant>` pattern. A “vendor” is typically the company that
created the CHRE implementation, which may bring with it some details related to
nanoapp compatibility, for example the Nanoapp Support Library from the same
vendor may be required. The “arch” field refers to the Instruction Set
Architecture (ISA) and related compiler configuration to create a binary for the
target processor. The “variant” is primarily related to the underlying platform
software that the CHRE framework builds on top of, such as the combination of
operating system and other software needed to select the appropriate combination
of code in the `platform/` folder, but can also define other attributes of the
build, such as the target memory region for the binary. If a vendor,
architecture, or variant consist of multiple words or components, then they
should be separated by a hyphen and not an underscore.

For example, if we assume that a fictional company named Aperture developed its
own CHRE framework implementation, targeting a CPU family called Potato, and a
collection of platform software called GladOS/Cake, then a suitable build target
name would be `aperture_potato_glados-cake`.

The build target may optionally have `_debug` appended, which is a common suffix
which enables `-g` and any additional target-specific debug flags.

### Creating a New Build Target

#### Architecture Support

The architecture-specific portion of the build deals with mainly the build
toolchain, and its associated flags.

It is easiest to check if the architecture is currently listed in `build/arch`,
and if it is, _Hooray! You're (almost) done_. It is still worthwhile to quickly
read through to know how the build is layered.

CHRE expects the build toolchain to be exported via Makefile variables,
specifically the compiler (`TARGET_CC`), archiver (`TARGET_AR`), and the linker
(`TARGET_LD`). Architecture specific compiler and linker flags are passed in via
the `TARGET_CFLAGS` and `TARGET_LDFLAGS` respectively. Additional
architecture-specific configuration is possible - refer to existing files under
`build/arch` and `build/build_template.mk` for details.

#### Build Target Makefile

Makefiles for each build target can be found at
`build/variant/<target_name>.mk`. These files are included at the end of the
top-level Makefile, and has the responsibility of collecting arguments for the
build template and invoking it to instantiate build rules. This involves doing
steps including (not an exhaustive listing):

* Setting the target name and platform ID

* Configuring (if needed) and including the apporpriate `build/arch/*.mk` file

* Collecting sources and flags specific to the platform into
  `TARGET_VARIANT_SRCS` and `TARGET_CFLAGS`

* Including `build/build_template.mk` to instantiate the build targets - this
  must be the last step, as the make targets cannot be modified once generated

Refer to existing files under `build/variant` for examples.

## Device Variants

While the build target is primarily concerned with configuring the CHRE build
for a particular chipset, the same chipset can appear in multiple device
models/SKUs, potentially with different peripheral hardware, targeted levels of
feature support, etc. Additionally, a device/chip vendor may wish to provide
additional build customization outside of the Makefiles contained in the
system/chre project. The build system supports configuration at this level via
the device variant makefile, typically named `variant.mk`, which is injected
into the build by setting the `CHRE_VARIANT_MK_INCLUDES` environment variable
when invoking the top-level Makefile. Refer to the file
`variant/android/variant.mk` for an example.

## Platform Sources

The file at `platform/platform.mk` lists sources and flags needed to compile the
CHRE framework for each supported platform. These must be added to Make
variables prefixed with the platform name (for example, `SIM_SRCS` for platform
sources used with the simulator build target), and not `COMMON_SRCS` or other
common variables, to avoid affecting other build targets.

## Build Artifacts

At the end of a successful build, the following are generated in the `out`
directory:

* `<build_target>/libchre.so` and `libchre.a`: the resulting CHRE framework
  binary, built as a dynamic/static library

* `<build_target>_objs/`: Directory with object files and other intermediates

* Depending on the build target, additional intermediates (e.g. `nanopb_gen` for
  files generated for use with NanoPB)
