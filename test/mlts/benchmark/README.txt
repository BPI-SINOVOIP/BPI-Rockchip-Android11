Copyright 2017 The Android Open Source Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
------------------------------------------------------------------

This directory contains files for the Android MLTS (Machine Learning
Test Suite). MLTS allows to evaluate NNAPI acceleration latency and accuracy
on an Android device, using few selected ML models and datesets.

Models and datasets used description and licensing can be found in
platform/test/mlts/models/README.txt file.

Usage:
* Connect a target device to your workstation, make sure it's
reachable through adb. Export target device ANDROID_SERIAL
environment variable if more than one device is connected.
* cd into android top-level source directory
> source build/envsetup.sh
> lunch aosp_arm-userdebug # Or aosp_arm64-userdebug if available.
> ./test/mlts/benchmark/build_and_run_benchmark.sh
* At the end of a benchmark run, its results will be
presented as html page, passed to xdg-open.

Changelog:
v0.1, 2018-10-15. Initial release with MobileNet(u8/f32)/TTS/ASR models.

