# Testing the CHRE Framework

[TOC]

The CHRE framework can be tested at various levels to ensure that
components/modules components are working correctly and API contracts are being
met. Below are some examples of what the team currently does to verify new
changes.

## Unit tests

Currently, unit tests exist for various core components and utilities. Since
platform-specific components likely aren’t compilable/available on a host
machine, only components that are OS independent can be tested via this path.

In order to write new tests, add a test source file under the test directory in
the appropriate subdirectory. e.g. `util/tests`. Then, add the file to the
`GOOGLETEST_SRCS` variable in the appropriate .mk file for that subdir,
`util/util.mk` for example.

Unit tests can be built and executed using `run_tests.sh`.

## PAL implementation tests

PAL implementation tests verify implementations of PAL interfaces adhere to the
requirements of that interface, and are intended primarily to support
development of PAL implementations, typically done by a chip vendor partner.
Additional setup may be required to integrate with the PAL under test and supply
necessary dependencies. The source code is in the files under `pal/tests/src`
and follows the naming scheme `*_pal_impl_test.cc`.

In order to run PAL tests, run: `run_pal_impl_tests.sh`. Note that there are
also PAL unit tests in the same directory. The unit tests are added to the
`GOOGLETEST_SRCS` target while PAL tests are added to the
`GOOGLETEST_PAL_IMPL_SRCS` target.

## FeatureWorld nanoapps

Located under the `apps/` directory, FeatureWorld nanoapps interact with the set
of CHRE APIs that they are named after, and can be useful during framework
development for manual verification of a feature area. For example, SensorWorld
attempts to samples from sensors and outputs to the log. It also offers a
break-it mode that randomly changes which sensors are being sampled in an
attempt to point out stress points in the framework or platform implementation.

These apps are usually built into the CHRE framework binary as static nanoapps
to facilitate easy development. See the Deploying Nanoapps section for more
information on static nanoapps.

## CHQTS

The Context Hub Qualification Test Suite (CHQTS) tests perform end-to-end
validation of a CHRE implementation, by using the Java APIs in Android to load
and interact with test nanoapps which then exercise the CHRE API. While this
code is nominally integrated in another test suite, the source code is available
under `java/test/chqts/` for the Java side code and `apps/test/chqts/` for the
CHQTS-only nanoapp code and `apps/test/common/` for the nanoapp code shared by
CHQTS and other test suites.
