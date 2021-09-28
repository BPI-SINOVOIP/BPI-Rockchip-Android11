# Pass bisection

This document describes a feature for the bisection tool, which provides
pass and transformation level bisection for a bad object file.

Before reading this document, please refer to README.bisect for general usage
of the bisection tool.

The benefit of using pass level bisection is:
When building a bad object file, it can tell you which pass and transformation
in the compiler caused the error.

*Notice:* This tool will only work for LLVM/clang, since it is using options
`-opt-bisect-limit` and `print-debug-counter` that only exist in LLVM.

## Arguments

All the required arguments in object-file-level bisection tool are still
to be provided. In addition, you will need to add the following arguments:

1. `--pass_bisect`: enables pass level bisection
2. `--ir_diff`: enables output of IR differences

Please refer to `--help` or the examples below for details about how to use
them.

## HOW TO USE: ChromeOS

*TODO* - Future work: Currently this only works for Android.

## HOW TO USE: Android

1.  Prerequisites: A general setup is still needed for Android, which means that
    you need to populate good and bad set of objects with two versions of
    compilers.

    See the documentation in `README.bisect.md` for more detailed instructions.

1.  Pass/Transformation Bisection: If you do not wish to override the other
    arguments, this command should be sufficient to do pass/transformation level
    bisection:

    ```
    ./run_bisect.py android PATH_TO_ANDROID_HOME_DIR
                --pass_bisect=’android/generate_cmd.sh’
                --prune=False
                --ir_diff
                --verbose
    ```

    Where:

    ```
    --pass_bisect:
        Enables pass/transformation level bisection and with default
        script to generate the command as ‘android/generate_cmd.sh’.
    --prune:
        For now, prune must be set to False to return only the first
        bad item.
    --ir_diff:
        Optional argument to print out IR differences.
    --verbose:
        To show IR diff, verbose needs to be on.
    ```

    Other default arguments:

    ```
    --get_initial_items='android/get_initial_items.sh'
    --switch_to_good='android/switch_to_good.sh'
    --switch_to_bad='android/switch_to_bad.sh'
    --test_setup_script='android/test_setup.sh'
    --test_script='android/interactive_test.sh'
    --incremental
    --prune
    --file_args
    ```

    You can always override them if needed. See README.bisect for more
    details.

1.  Other features: Features such as resuming, number of jobs, and device id
    remain the same as before. See README.bisect for more details.
