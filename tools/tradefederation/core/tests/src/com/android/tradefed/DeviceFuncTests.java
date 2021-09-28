/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.tradefed;

import com.android.tradefed.device.TestDeviceFuncTest;
import com.android.tradefed.proto.PlatformProtosFuncTest;
import com.android.tradefed.targetprep.AppSetupFuncTest;
import com.android.tradefed.targetprep.DeviceSetupFuncTest;
import com.android.tradefed.targetprep.UserCleanerFuncTest;
import com.android.tradefed.testtype.DeviceSuite;
import com.android.tradefed.testtype.InstrumentationTestFuncTest;

import org.junit.runner.RunWith;
import org.junit.runners.Suite.SuiteClasses;

/** A test suite for all Trade Federation functional tests that requires a device. */
@RunWith(DeviceSuite.class)
@SuiteClasses({
    // device
    TestDeviceFuncTest.class,
    // proto
    PlatformProtosFuncTest.class,
    // targetprep
    AppSetupFuncTest.class,
    DeviceSetupFuncTest.class,
    UserCleanerFuncTest.class,
    // testtype
    InstrumentationTestFuncTest.class,
})
public class DeviceFuncTests {}
