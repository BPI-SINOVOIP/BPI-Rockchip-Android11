# Car test apps

This repository is only for car test applications. They can be unbundled from Android devices. 

## Prerequisites

* You need to build or have a [car-ui-lib](https://cs.android.com/android/platform/superproject/+/android-10.0.0_r30:packages/apps/Car/libs/car-ui-lib/)
aar ready first if to build test apps on Android Studio.
* android-10.0.0_r30 is a release tag placeholder in this doc, you should replace the one you need.

## Building

1. There are 3 ways to get the source. Pick one works better for you.
  * A: Download [tgz](https://android.googlesource.com/platform/packages/apps/Car/tests/+archive/refs/tags/android-10.0.0_r30.tar.gz)
from the source if no plan to contribue.
  * B: Repo workflow, see [workstation setup](#workstation-setup).
  * C: Git workflow, e.g.
```
git clone -b $BRANCH https://android.googlesource.com/platform/packages/apps/Car/tests
cd tests
f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f
```

* To learn more, checkout [Basic Gerrit Walkthrough](https://gerrit-review.googlesource.com/Documentation/intro-gerrit-walkthrough-github.html).
* See tools/git_clone_projects.sh as an example to get both Car/libs and tests projects. 

2. Install [Android Studio](https://developer.android.com/studio), open the `tests`
project by Android Studio and do your magic.
  * You will need to build car-ui-lib by Android Studio first.

### TestMediaApp

TestMediaApp should be one of the run configurations. The green Run button should build and install
the app on your phone.

To see TestMediaApp in Android Auto Projected:

1. Open Android Auto on phone
2. Click hamburger icon at top left -> Settings
3. Scroll to Version at bottom and tap ~10 times to unlock Developer Mode
4. Click kebab icon at top right -> Developer settings
5. Scroll to bottom and enable "Unknown sources"
6. Exit and re-open Android Auto
7. TestMediaApp should now be visible (click headphones icon in phone app to see app picker)

### RotaryPlayground

RotaryPlayground is a test and reference application for the AAOS Rotary framework to use with an
external rotary input device.

Beside building in Android Studio, you can also build and install RotaryPlayground into an AAOS
device:

```
$ make RotaryPlayground
$ adb install -r -g out/target/[path]/system/app/RotaryPlayground/RotaryPlayground.apk
```

* See tools/go_rotary.sh for an example build, install and run the test app in an Android tree. 


### RotaryIME

RotaryIME is a sample input method for rotary controllers.

To build and install RotaryIME onto an AAOS device:
```
$ make RotaryIME
$ adb install -r -g out/target/[path]/system/app/RotaryIME/RotaryIME.apk
```

## Contributing

### Workstation setup

Install [repo](https://source.android.com/setup/build/downloading#installing-repo) command line
tool. Then run:

```
sudo apt-get install gitk
sudo apt-get install git-gui
mkdir WORKING_DIRECTORY_FOR_GIT_REPO
cd WORKING_DIRECTORY_FOR_GIT_REPO
repo init -u https://android.googlesource.com/platform/manifest -b $BRANCH -g name:platform/tools/repohooks,name:platform/packages/apps/Car/tests --depth=1
repo sync
```

### Making a change
#### Repo workflow

```
repo start BRANCH_NAME .
# Make some changes
git gui &
# Use GUI to create a CL. Check amend box to update a work-in-progress CL
repo upload .
```

#### Git workflow
```
# Make some changes
git add .
git commit
git push origin HEAD:refs/for/$BRANCH
```
