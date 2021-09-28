IGT GPU Tools
=============

Description
-----------

IGT GPU Tools is a collection of tools for development and testing of the DRM
drivers. There are many macro-level test suites that get used against the
drivers, including xtest, rendercheck, piglit, and oglconform, but failures from
those can be difficult to track down to kernel changes, and many require
complicated build procedures or specific testing environments to get useful
results. Therefore, IGT GPU Tools includes low-level tools and tests
specifically for development and testing of the DRM Drivers.

IGT GPU Tools is split into several sections:

**benchmarks/**

This is a collection of useful microbenchmarks that can be used to tune
DRM code in relevant ways.

The benchmarks require KMS to be enabled.  When run with an X Server
running, they must be run as root to avoid the authentication
requirement.

Note that a few other microbenchmarks are in tests (like gem_gtt_speed).

**tests/**

This is a set of automated tests to run against the DRM to validate
changes. Many of the tests have subtests, which can be listed by using
the --list-subtests command line option and then run using the
--run-subtest option. If --run-subtest is not used, all subtests will
be run. Some tests have futher options and these are detailed by using
the --help option.

The test suite can be run using the run-tests.sh script available in
the scripts directory. Piglit is used to run the tests and can either
be installed from your distribution (if available), or can be
downloaded locally for use with the script by running:

    ./scripts/run-tests.sh -d

run-tests.sh has options for filtering and excluding tests from test
runs:

  -t <regex>      only include tests that match the regular expression
  -x <regex>      exclude tests that match the regular expression

Useful patterns for test filtering are described in the API
documentation and the full list of tests and subtests can be produced
by passing -l to the run-tests.sh script.

Results are written to a JSON file and an HTML summary can also be
created by passing -s to the run-tests.sh script. Further options are
are detailed by using the -h option.


If not using the script, piglit can be obtained from:

    git://anongit.freedesktop.org/piglit

There is no need to build and install piglit if it is only going to be
used for running i-g-t tests.

Set the IGT_TEST_ROOT environment variable to point to the tests
directory, or set the path key in the "igt" section of piglit.conf to
the igt-gpu-tools root directory.

The tests in the i-g-t sources need to have been built already. Then we
can run the testcases with (as usual as root, no other drm clients
running):

    piglit-sources # ./piglit run igt <results-file>

The testlist is built at runtime, so no need to update anything in
piglit when adding new tests. See

    piglit-sources $ ./piglit run -h

for some useful options.

Piglit only runs a default set of tests and is useful for regression
testing. Other tests not run are:
- tests that might hang the gpu, see HANG in Makefile.am
- gem_stress, a stress test suite. Look at the source for all the
  various options.
- testdisplay is only run in the default mode. testdisplay has tons of
  options to test different kms functionality, again read the source for
  the details.

**lib/**

Common helper functions and headers used by the other tools.

**man/**

Manpages, unfortunately rather incomplete.

**tools/**

This is a collection of debugging tools that had previously been
built with the 2D driver but not shipped.  Some distros were hacking
up the 2D build to ship them.  Instead, here's a separate package for
people debugging the driver.

These tools generally must be run as root, except for the ones that just
decode dumps.

**docs/**

Contains the automatically generated igt-gpu-tools libraries
reference documentation in docs/reference/. You need to have the
gtk-doc tools installed and use the "--enable-gtk-doc" configure flag
to generate this API documentation.

To regenerate the html files when updating documentation, use:

    $ ninja -C build igt-gpu-tools-doc

If you've added/changed/removed a symbol or anything else that changes
the overall structure or indexes, this needs to be reflected in
igt-gpu-tools-sections.txt. Entirely new sections will also need to be
added to igt-gpu-tools-docs.xml in the appropriate place.

**include/drm-uapi**

Imported DRM uapi headers from airlied's drm-next branch.
These should be updated all together by executing "make
headers_install" from that branch of the kernel and then
copying the resulting ./usr/include/drm/*.h in and committing
with a note of which commit on airlied's branch was used to
generate them.


Requirements
------------

This is a non-exhaustive list of package dependencies required for building
the default configuration (package names may vary):

	bison
	gtk-doc-tools
	flex
	libcairo2-dev
	libdrm-dev
	libkmod-dev
	libpixman-1-dev
	libpciaccess-dev
	libprocps-dev
	libudev-dev
	libunwind-dev
	liblzma-dev
	libdw-dev
	python-docutils
	x11proto-dri2-dev
	xutils-dev

The following dependencies are required for building chamelium support
(package names may vary):

	libxmlrpc-core-c3-dev
	libudev-dev
	libglib2.0-dev
	libgsl-dev

The following dependencies are requires for building audio support
(package names may vary):

	libasound2-dev
	libgsl-dev

See Dockerfiles.* for package names in different distributions.

Meson build system support
--------------------------

Currently we support both meson and automake as build systems, but meson is the
recommended choice. Oneliner to get started:

    $ mkdir build && meson build && cd build && ninja

Note that meson insist on separate build directories from the source tree.

Running selfchecks for lib/tests and tests/ is done with

    $ ninja -C build test

Note that this doesn't actually run the testcases in tests/: scripts/run-tests.sh
should continue to be used for that.

Documentation is built using

    $ ninja -C build igt-gpu-tools-doc

Note that this needs meson v0.47 or later, earlier versions of meson do not
track depencies correctly for the documentation build and need:

    $ ninja -C build && ninja -C build igt-gpu-tools-doc

Note that there's a setup script similar to ./autogen.sh which creates a
compatibility Makefile with a few useful default targets:

    $ ./meson.sh [make-arguments]

Releases for maintainers
------------------------

(1.14)

http://www.x.org/wiki/Development/Documentation/ReleaseHOWTO/
