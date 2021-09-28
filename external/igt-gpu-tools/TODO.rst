TODO
====

This contains a list of refactoring, cleanup and getting-started tasks around
the IGT library.

Split up igt_kms.c/igt_kms.h
----------------------------

igt_kms contains both a low-level modeset library, with thin convenience
wrappers around core kernel code and libdrm. These functions usually have a
drmtest_ prefix (but not all of them).

The other part is a higher-level library around the igt_display and related
structures. Those usually come with an igt_ prefix.

The task would be to split this up, and where necessary, fix up the prefixes to
match the level a function operates at.

Remove property enums from igt_kms
----------------------------------

These are just needless indirection for writing tests. We can keep the #defines
(since those strings are defacto uapi), but everything else is best handled by
runtime-sizing all the arrays.

Documentation
-------------

igt documentation is full of warnings and fairly incomplete. Pick a library, and
work together with its authors to fix things up.
