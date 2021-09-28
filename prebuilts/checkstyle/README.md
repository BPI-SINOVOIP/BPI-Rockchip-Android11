# Checkstyle

Checkstyle is used by developers to validate Java code style and formatting,
and can be run as part of the pre-upload hooks.

[TOC]

## Running it

It can be invoked in two ways.
1.  To check style of entire specific files:
    `checkstyle.py -f FILE [FILE ...]`
2.  To check style of the lines modified in the latest commit:
    `checkstyle.py`


## Projects used

### Checkstyle

A development tool to help programmers write Java code that adheres to a
coding standard.

*   URL: https://checkstyle.sourceforge.io/
*   Version: 7.4-SNAPSHOT
*   License: LGPL 2.1
*   License File: LICENSE
*   Source repo: https://android.googlesource.com/platform/external/checkstyle

### Git-Lint

Git-lint is a tool to run lint checks on only files changed in the latest
commit.

*   URL: https://github.com/sk-/git-lint/
*   Version: 0.0.8
*   License: Apache 2.0
*   License File: gitlint/LICENSE
*   Local Modifications:
    *   Downloaded gitlint/git.py and git/utils.py files individually.

## Pre-upload linting

To run checkstyle as part of the pre-upload hooks, add the following line to
your `PREUPLOAD.cfg`:
```
checkstyle_hook = ${REPO_ROOT}/prebuilts/checkstyle/checkstyle.py --sha ${PREUPLOAD_COMMIT}
```

Note that checkstyle does not always agree with clang-format, and so it's best
to only have one enabled for Java.

### Disabling Clang Format for Java

In `.clang-format` add the following to disable format checking and correcting
for Java:
```
---
Language: Java
DisableFormat: true
SortIncludes: false
---
```
In some versions of clang-format, `DisableFormat` doesn't stop the sorting of
includes. So to fully disable clang-format from doing anything for Java files,
both options are needed.
