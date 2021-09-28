# API Lint

Metalava can optionally run warn about various API problems based on the Android
API Council's guidelines, described in go/android-api-guidelines.

These rules are not always exact. For example, you should avoid using acronyms
in names; e.g. a method should be named handleUri, not handleURI. But what about
getZOrder?  This is probably better than getZorder, but metalava can't tell
without consulting a dictionary.

Therefore, there are cases where you want to say "ok, that's good advice in
general, but wrong here". In order to avoid having this warningshow up again
and again, there are two ways to mark an issue such that it is no longer
flagged.

(Note that metalava will not report issues on classes, methods and fields that
are deprecated since these are presumably already known to be bad and are already
discouraged.)

## Suppressing with @Suppress

Next to an error message, metalava will include the issue id. For example,
here's a sample error message:

    src/android/pkg/MyStringImpl.java:3: error: Don't expose your implementation details: MyStringImpl ends with Impl [EndsWithImpl]

Here the id is "EndsWithImpl". You can suppress this with the @SuppressLint
annotation:

    ...
    import android.annotation.SuppressLint;
    ...

    @SuppressLint("EndsWithImpl")
    public class MyStringImpl {
        ...

## Suppressing with baselines

Metalava also has support for "baselines", which are files which record all the
current warnings and errors in the codebase. When metalava runs, it looks up the
baseline to see if a given issue is already listed in the baseline, and if so,
it is silently ignored.

You can pass a flag to metalava ("--update-baseline") to tell it to update the
baseline files with any new errors it comes across instead of reporting
them. With soong, if you specify the baseline explicitly, like this:

    --- a/Android.bp
    +++ b/Android.bp
    @@ -1678,6 +1678,7 @@ droidstubs {
         },
         api_lint: {
             enabled: true,
        ==>  baseline_filename: "api/baseline.txt", <==
         }
         jdiff_enabled: true,
     }

then the build system will automatically supply `--update-baseline` on your
behalf to a generated file in the out/ folder, and if there's a failure metalava
will tell you where to find the updated baseline which you can then copy into
place:

    ...
    93 new API lint issues were found. See tools/metalava/API-LINT.md for how to handle these.
    ************************************************************
    Your API changes are triggering API Lint warnings or errors.
    To make these errors go away, you have two choices:

    1. You can suppress the errors with @SuppressLint("<id>")
    2. You can update the baseline by executing the following
       command:
           cp \
           out/soong/.intermediates/frameworks/base/system-api-stubs-docs/android_common/api/system-baseline.txt \
           frameworks/base/api/system-baseline.txt
       To submit the revised baseline.txt to the main Android
       repository, you will need approval.
    ************************************************************



Then re-run the build and you should now see diffs to the baseline file; git
diff to make sure you're really only marking the issues you intended to include.

    $ git status
    ...
    Untracked files:
      (use "git add <file>..." to include in what will be committed)

          baseline.txt

## Copying File Manually

In the near future the build system will not allow source files to be modified
by the build. At that point you'll need to manually copy in the file instead.
During the build, before failing, metalava will emit a message like this:

    ...
    metalava wrote updated baseline to out/something/baseline.txt
    ...

At that point you can copy the file to where you need:

```sh
$ cp out/something/baseline.txt frameworks/base/api/baseline.txt
```

## Existing Issues

You can view the exact set of existing issues (current APIs that get flagged by
API lint) by inspecting the baseline files in the source tree, but to get a
sense of the types of issues that are more likely to have a false positive,
here's the existing distribution as of early 2019:

    Count Issue Id                       Rule Severity
    --------------------------------------------------

      932 ProtectedMember                M7   error
      321 OnNameExpected                      warning
      288 ArrayReturn                         warning
      269 ActionValue                    C4   error
      266 NoByteOrShort                  FW12 warning
      221 ExecutorRegistration           L1   warning
      211 AllUpper                       C2   error
      206 GetterSetterNames              M6   error
      185 BannedThrow                         error
      172 SamShouldBeLast                     warning
      159 NoClone                             error
      159 ParcelNotFinal                 FW8  error
      119 NotCloseable                        warning
      105 ListenerLast                   M3   warning
       81 ConcreteCollection             CL2  error
       78 StaticUtils                         error
       76 IntentName                     C3   error
       74 VisiblySynchronized            M5   error
       72 GenericException               S1   error
       65 KotlinOperator                      warning
       57 AcronymName                    S1   warning
       55 ParcelCreator                  FW3  error
       54 ParcelConstructor              FW3  error
       53 UseIcu                              warning
       48 Enum                           F5   error
       41 RethrowRemoteException         FW9  error
       37 AutoBoxing                     M11  error
       35 StreamFiles                    M10  warning
       28 IntentBuilderName              FW1  warning
       27 ServiceName                    C4   error
       26 ListenerInterface              L1   error
       25 ContextFirst                   M3   error
       25 InterfaceConstant              C4   error
       24 CallbackInterface              CL3  error
       24 RegistrationName               L3   error
       23 IllegalStateException          S1   warning
       22 EqualsAndHashCode              M8   error
       22 PackageLayering                FW6  warning
       18 MinMaxConstant                 C8   warning
       18 SingletonConstructor                error
       17 MethodNameUnits                     error
       15 MissingBuildMethod                  warning
       15 UserHandleName                      warning
       14 UserHandle                          warning
       13 ResourceFieldName                   error
       12 ManagerLookup                       error
       11 ManagerConstructor                  error
        9 CallbackMethodName             L1   error
        9 ParcelableList                      warning
        8 CallbackName                   L1   warning
        7 HeavyBitSet                         error
        7 ResourceValueFieldName         C7   error
        6 CompileTimeConstant                 error
        6 SetterReturnsThis              M4   warning
        4 EndsWithImpl                        error
        4 TopLevelBuilder                     warning
        4 UseParcelFileDescriptor        FW11 error
        3 MentionsGoogle                      error
        3 StartWithLower                 S1   error
        2 AbstractInner                       warning
        2 BuilderSetStyle                     warning
        2 OverlappingConstants           C1   warning
        2 PairedRegistration             L2   error
        2 SingularCallback               L1   error
        2 StartWithUpper                 S1   error
        1 ContextNameSuffix              C4   error
        1 KotlinKeyword                       error
    --------------------------------------------------
     4902

(This is generated when metalava is invoked with both --verbose and --update-baseline.)
