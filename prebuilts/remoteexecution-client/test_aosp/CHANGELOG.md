## Release 0.12.2 (2020-04-15)
```
Changes:
  + d52f0b7:
    Remove un-needed chdir and fix integration tests
  + 34c2872:
    Remove -debug-info-kind flag in clang-scan-deps.
  + 0e25698:
    Fix how we invoke clang++ binary to find resource directory
  + 94f70cf:
    Add an rsp package for centralizing rsp file parsing.
```

This release primarily contains bug fixes to C++ input processor and rsp file
parsing.

## Release 0.12.1 (2020-04-10)
```
Changes:
  + 09a5526:
    Also cache when we fail to find the resource-directory
```

This release fixes input processor latency for header-abi-dumper actions.

## Release 0.12.0 (2020-04-08)
```
Changes:
  + 5b29aa6:
    Fix compare mode for actions where some inputs are also outputs.
  + 9c31e0d:
    Add .toc files as additional dependencies
  + 667c5e1:
    Supporting remote execution of header ABI dump actions
  + 625fc40:
    Add more details on how to build the code and run tests, and how to contribute.
  + 99426b0:
    Add support for Bazelisk and pin to Bazel 2.2.0.
```

This release adds support for remote execution of header ABI dumper and adds a fix
for compare mode of metalava actions.

## Release 0.11.0 (2020-04-02)
```
Changes:
  + b2836e1:
    Add output_directories and multiple rsp files flags to rewrapper.
  + 4559f40:
    Log flags in rewrapper after parsing.
  + 8afef4a:
    Removing flags logging from rewrapper, making it verbosity 1
  + b7447ea:
    Use LOG_DIR variable in android integration tests.
  + 74ec9b9:
    Add support in reproxy for link actions
```

This release adds support for linking, explicit output directories, and multiple
input file lists. Also includes logging fixes.

## Release 0.10.0 (2020-04-01)
```
Changes:
  + 3be02a4:
    Fix metalava input processor.
  + 7c66991:
    Add common config file with artifact definitions for log files.
  + c2a1d10:
    Local execution to support non-uniform resource requirements. Bug: b/151818457
  + 000cd94:
    Cache metalava version to avoid running metalava multiple times.
  + 9615ed6:
    Refactor toolchain input processor and support version checks.
  + 0a5b462:
    Add a metalava flag parser.
  + 13dc031:
    Remove support for async artifact upload in LERC.
  + b5d8485:
    Failure in the input processor should fallback to local execution.
  + 751faad:
    Using SDK command proto (latest SDK commit)
  + 836d908:
    Logging improvements: log all flags from all binaries, log server address on failed dial
  + 3e20ddd:
    Add remote_disabled mode to reproxy.
```

This release mainly adds metalava support to reproxy.

## Release 0.9.5 (2020-03-16)
```
Changes:
  + 0bd4e59:
    Add scripts to do global setup of RBE variables to android developers
  + 3ff26d6:
    Add reproxy support for cache-silo key
  + 614329a:
    Dont clean command args by default
```

This release makes reproxy support cache silo key and does not clean arguments
for remote execution to prevent bootloops on output images.

## Release 0.9.4 (2020-03-05)
```
Changes:
  + 250a753:
    Fix unnecessary deps validation when deep input processor succeeds.
  + 061b94c:
    Add the gcno file to the output spec if --coverage is passed to clang
```

This release has fixes for downloading coverage file generated as part of clang
compiles and fixes LERC to NOT do un-necessary dependency validation.

## Release 0.9.3 (2020-03-03)
```
Changes:
  + 9d89a75:
    Updated the clang flag parser to handle more general clang commands.
  + a9eddb0:
    Document that --toolchain_inputs is relative to the exec root.
  + 628a60a:
    Handle -B flag and add it as a dependency.
  + bd7abce:
    Bump SDK version to include fix for batch download of blobs.
  + a792271:
    Modify create-release script to drop CHANGELOG.md to test/ folder
```

This release has fixes with respect to C++ input processor and bumps RE-SDK
version to include fix for batch blob downloads.

## Release 0.9.2 (2020-02-28)
```
Changes:
  + e140d51:
    Replace moreflag with rbeflag in the version package.
  + 290e433:
    Upgrade sdk to include batch upload size fix.
  + 415cb83:
    Enforce all returned paths from input processor are under exec root.
  + 80fb1b3:
    Remove the -fintegrated-cc1 flag when doing clang-scan-deps.
  + 6d0e3f1:
    Remove changelog from prebuilt-drop tool invocation
```

This release fixes a bug in reading RBE flags and includes input processor
refactorings and fixes for supporting the nest/chrome builds.

## Release 0.9.1 (2020-02-25)
```
Changes:
  + f4fae4d:
    Pass vargs to clang-scan-deps instead of combined string
  + 1ff5530:
    Remove the invalidation check in dependency scanner plugin
```

This release changes scan-deps interface so that it takes an unescaped
list of arguments instead of a JSON database string.

## Release 0.9.0 (2020-02-21)
```
Changes:
  + 9bdf5ca:
    Support toolchain_inputs flag for explicitly specifying toolchain
  + 4f47570:
    Handle -fsanitize-blacklist and -fprofile-sample-use similar to fprofile-use
  + b29d7a3:
    Change default execution strategy to undefined
  + 52cb208:
    Fix broken javac integ test.
  + 17cd328:
    rbeflag package allows setting flags with RBE_ prefixed env vars.
  + faf5c1e:
    Remove workaround to not delete inputs under output directories in compare mode.
  + 6abae0d:
    Add escaping for spaces on clang build command.
  + 09f5abf:
    toolchain: toolchain executable is workdir relative
  + 7179410:
    Fixing tool commands to process inputs shallow
  + bf15e5b:
    Adding ability to parse logs from multiple files, and save to separate files.
```

This release contains support for the toolchain_inputs flag and other fixes.

## Release 0.8.2 (2020-02-10)
```
Changes:
  + cc4b9cf:
    Fix log messages missing printing the error.
```

This release fixes missing error logs in removal of output directories in
compare mode.

## Release 0.8.1 (2020-02-07)
```
Changes:
  + 2ae2a7f:
    Prevent deletion of inputs under output directories in compare mode.
  + 8365bbf:
    Added strings replacer to properly encode quotation marks on created.
  + c8b2db9:
    Change updated flags instead of actual flags.
  + 453eceb:
    Added a feature to enable/disable the use of the toolchain input file.
  + f8f49b9:
    Support remote execution of javac/r8/d8.
  + ee5e60c:
    Propagate RBE_HTTP_PROXY value to reproxy, if set.
  + 7761b78:
    Add a debug helper function for dumping inputs to a tmp directory.
  + 587f2d4:
    Add working directory to the joined path of the .keep_me file.
  + 0284950:
    Add a feature to enable/disable the command argument cleaning. Default is enabled.
```

This release fixes a breakage in D8 compare builds due to having inputs under output
directories.

## Release 0.8.0 (2020-02-03)
```
Changes:
  + 7af0844
    Fix mismatch in ab/6089871 due to missing dependency on the --system dir.
  + 4cf6a50
    Update Android internal image to 2020-01-22 snapshot.
  + 6d08ef5
    Merge toolchain inputs in returned results in case of shallow fallback
  + 86630ca
    Fix segmentation fault when both toolchain and clangscandeps fail
  + 5c040c2
    Add javac LERC integration test.
  + fd852e5
    Add feature to enable in band update of action results to test
    whether it has an impact on performance.
  + 59f7155
    Fix crash in stat logging when accept-cached is false
  + f4a59aa
    Fix the paths returned by toolchain input processor
  + 0132e03
    Add -Qunused-arguments parameter to scan-deps invocation to suppress warnings
```

This release adds a feature to enable synchronous upload of cached results in LERC mode
and has a couple of bug-fixes for remote-execution flow.

## Release 0.7.2 (2020-01-23)
```
Changes:
  + edfbaae:
    Remove -verify flag before calling clang-scan-deps
  + 4930837:
    Revert "Merge "Optimize the dependency scanner plugin to reuse workers""
```

This release reverts the clang-scan-deps optimization since we discovered a bug
in clang-scan-deps caching behaviour when workers are reused.

## Release 0.7.1 (2020-01-20)
```
Changes:
  + bfee822:
    Fix occasional failure in Javac/R8/D8 compare builds
  + 6a54076:
    Remote execution integration test for re-client
  + 3cefecc:
    Optimize the dependency scanner plugin to reuse workers
  + a172d20:
    Aggregating stats per label.
  + 31fbea5:
    Use a random socket file in integration tests
  + 98f775a:
    Per proxy invocation ID.
  + fd4a213:
    Make rewrapper block until it can dial to reproxy.
  + 2b43cf9:
    Part 2 of renaming continuous_android tests to continuous_android_lerc
  + b5ced78:
    Updated scripts/install to run on mac as well as linux.
  + e15143f:
    Updated cgo directives to selectively pick certain libraries.
  + 900dbff:
    Update the dep scanning build script to run on macos as well as linux.
  + d9b6602:
    Update the cpp dependency scanner integration test to explicitly
  + f5eac3c:
    Update .gitignore file to ignore MacOS .DS_Store files.
  + 53b2fe8:
    Add virtual inputs for all -I and -isystem dir paths
```

This release includes a potential fix for the flaky resource exhaustion issue
as well as an optimization for the clang-scan-deps plugin.

## Release 0.7.0 (2020-01-06)
```
Changes:
  + ea1b2a1:
    Wireup the new toolchain input processor as part of ProcessInputs fn
  + f0ae7a8:
    Script to test application default creds on Android corp buildbots
```

This release adds a feature to search for "remote_toolchain_inputs" file that
lives alongside LLVM toolchains in Android to specify the list of files that
constitute toolchain inputs.

## Release 0.6.2 (2019-12-19)
```
Changes:
  + 41c7b59:
    Update remote-apis-sdks commit to include the GRPC fix in SDK
  + bd18b14:
    Prevent failure to load clang-scan-deps from failing actions.
```

This release primarily fixes the GRPC max concurrent streams issue in the SDK
and goes back to using full input processor as default.

## Release 0.6.1 (2019-12-16)
```
Changes:
  + d3de0ae:
    Make shallow input processing the default.
  + 8ded0c8:
    Fix for flakiness in logger that potentially caused b/146229435.
  + 25dab52:
    Add clang-scan-deps to LERC.
  + 8e55b12:
    Add verification mode to runRemote.
```

## Release 0.6.0 (2019-12-03)
```
Changes:

  + 6882689:
    Compare mode for actions with output directories.
  + 08499f0:
    Add flag to enable shadow header detection.
  + 709061a:
    Switching SDK to latest commit (retries)
  + 04eb160:
    Add a tool action type to run any tool with the inputs/outputs
  + f6cc51b:
    Add reproxy version number as a cache silo to all actions.
  + 0519b5b:
    Move flags structs to a separate package: pkg/flags.
  + c16f9c8:
    Simplify the signature of ProcessInputsShallow.
  + 1165068:
    Fix flaky test due to non-deterministic order of include directories.
  + 4ff0a4b:
    Optimize shadow headers performance.
  + 04223fe:
    Refactor runLERC code to follow go readability guidelines.
  + 4087961:
    Switching to latest SDK version
  + be22f0b:
    Add documentation about the dependency scanner plugin
  + 996339d:
    Change V(2) log to warning log when RE fails and we fallback to local
  + 689c6cb:
    Update foundry-vars.sh to the correct instance name.
  + d8bcce5:
    Make rewrapper retries less aggresssive and increase max retry duration
  + 356debf:
    Fixing stats aggregation bug.
  + 222117f:
    Pass rewrapper start time to reproxy for logging and aggregation.
  + b6b1478:
    Restrict input processor parallelism to num CPU cores
  + 7b54918:
    Adding include processor timing stats.
  + fe28910:
    Adding end-to-end timing stats, minor refactoring
  + 9c1afd7:
    Adding local execution timing stats to the proxy
  + 2cac73d:
    Rename rbe_metrics file to rbe_metrics.txt
  + 82edf55:
    Adding LERC deps timing metadata
  + 50d62ad:
    Add dependency scanner plugin to the release script
```

This release adds local performance metrics and shadow header detection as an
off by default feature.

## Release 0.5.3 (2019-11-13)
```
Changes:

  + da676b7:
    Statically link libstdc++ with the Go plugin to avoid
    libstdc++ version issues on Android buildbot.
```

This release addresses libstdc++ loading issue on dependency scanner plugin.

## Release 0.5.2 (2019-11-13)
```
Changes:

  + 7a4cc47:
    Don't fail reproxy when loading of dependency scanner plugin fails.
  + efea8bf:
    Add a temporary workaround suggested in rules_go to fix issue
    with version number stamping.
```

This release makes reproxy not fail when it cannot load CPP dependency
scanner plugin.

## Release 0.5.1 (2019-11-11)
```
Changes:

  + 05875af:
    Add dependency_scanner_go_plugin.so to Kokoro regex too
```

This release makes the Kokoro workflow also upload dependency scanner
plugin.

## Release 0.5.0 (2019-11-11)
```
Changes:

  + 2904c9d:
    Implementing LERC with include directories awareness for constructing
    dependency file.
  + 2c7f757:
    Migrating to latest dependency versions and Bazel 1.1.
  + cc8cc63:
    Wire up clang-scan-deps to input processor.
  + 13374fc:
    Adding output metrics and digests to proxy log and stats.
  + a46e81f:
    Support rsp files for javac compiles.
```

This release mainly adds dependency scanner plugin to support remote execution
for C++ compile actions.

## Release 0.3.0 (2019-10-22)
```
Changes:

  + ba1466e:
    Add flag to control bootstrap timeout
  + 55d0ad6:
    Keep track of RBE tool version in Dremel
  + 3839b37:
    Renaming Dial to NewClient for clarity
```

This release mainly adds RBE tool version number to dumpstats.
