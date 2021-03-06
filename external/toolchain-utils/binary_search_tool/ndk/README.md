# NDK Bisection tool

This is an example bisection for an NDK build system. This example specifically
bisects the sample NDK Teapot app. All steps (setup and otherwise) for bisection
can be found in `DO_BISECTION.sh`. This shell script is meant to show the
process required to bisect a compiler problem in an arbitrary NDK app build
system.

There are three necessary setup steps to run this example:

1.  Install the NDK (known to work with r12b)

    1. See here for NDK: https://developer.android.com/ndk/index.html
    2. Go here for older NDK downloads: https://github.com/android-ndk/ndk/wiki

1.  Install the compiler wrapper provided with this repo. See
    compiler_wrapper.py for more details.

    1.  Essentially you must go into the NDK source (or where you build system
        stores its toolchain), rename your compilers to <compiler>.real, and
        create a symlink pointing to compiler_wrapper.py named <compiler>
        (where your compiler used to be).

    2.  If you're using the toolchains that come with the NDK they live at:
        `<ndk_path>/toolchains/<arch>/prebuilt/<host>/bin`
        example:
        `<ndk_path>/toolchains/llvm/prebuilt/linux-x86_64/bin/clang`

1.  Plug in an Arm7 compatible Android device with usb debugging enabled.

    1. This bisection example was tested with a Nexus 5X

    2. It should be relatively simple to change the example to work with other
       types of devices. Just change the scripts, and change PATCH1 to use a
       different build flavor (like x86). See below for more details.

This example contains two patches:

`PATCH1` - This is the necessary changes to the build system to make the
bisection easier. More specifically, it adds an arm7 build flavor to gradle.
By default, this project will build objects for all possible architectures and
package them into one big apk. These object files meant for another
architecture just sit there and don't actually execute. By adding a build
flavor for arm7, our compiler wrapper won't try to bisect object files meant
for another device.

`PATCH2` - This patch is what inserts the "compiler error". This is a simple
nullptr error in one of the source files, but it is meant to mimic bad code
generation. The result of the error is the app simply crashes immediately
after starting.

## Using another device architecture

If we want to bisect for an x86-64 device we first need to provide a arch
specific build flavor in our app/build.gradle file:

```
create("x86-64") {
  ndk.abiFilters.add("x86_64")
}
```

We want to add this under the same "productFlavors" section that our arm7
build flavor is in (see PATCH1). Now we should have the "installx86-64Debug"
task in our build system. We can use this to build and install an x86-64
version of our app.

Now we want to change our `test_setup.sh` script to run our new gradle task:
```
./gradlew installx86-64Debug
```

Keep in mind, these specific build flavors are not required. If your build
system makes these device specific builds difficult to implement, the
bisection tool will function perfectly fine without them. However, the
downside of not having targetting a single architecture is the bisection will
take longer (as it will need to search across more object files).

## Additional Documentation

These are internal Google documents, if you are a developer external to
Google please ask whoever gave you this sample for access or copies to the
documentation. If you cannot gain access, the various READMEs paired with the
bisector should help you.

* Ahmad's original presentation: https://goto.google.com/zxdfyi
* Bisection tool update design doc: https://goto.google.com/zcwei
* Bisection tool webpage: https://goto.google.com/ruwpyi
* Compiler wrapper webpage: https://goto.google.com/xossn
