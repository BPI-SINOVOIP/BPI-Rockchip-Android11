/*
 * Copyright (C) 2010 The Android Open Source Project
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

import com.android.tradefed.build.FileDownloadCacheFuncTest;
import com.android.tradefed.cluster.ClusterCommandLauncherFuncTest;
import com.android.tradefed.cluster.ClusterEventUploaderFuncTest;
import com.android.tradefed.command.CommandSchedulerFuncTest;
import com.android.tradefed.command.remote.RemoteManagerFuncTest;
import com.android.tradefed.device.metric.DeviceMetricDataFuncTest;
import com.android.tradefed.util.FileUtilFuncTest;
import com.android.tradefed.util.GCSFileDownloaderFuncTest;
import com.android.tradefed.util.GCSFileUploaderFuncTest;
import com.android.tradefed.util.RunUtilFuncTest;
import com.android.tradefed.util.ZipUtilFuncTest;
import com.android.tradefed.util.net.HttpHelperFuncTest;

import org.junit.runner.RunWith;
import org.junit.runners.Suite;
import org.junit.runners.Suite.SuiteClasses;

/** A test suite for all Trade Federation functional tests that do not require a device. */
@RunWith(Suite.class)
@SuiteClasses({
    // build
    FileDownloadCacheFuncTest.class,
    // cluster
    ClusterCommandLauncherFuncTest.class,
    ClusterEventUploaderFuncTest.class,
    // command
    CommandSchedulerFuncTest.class,
    // command.remote
    RemoteManagerFuncTest.class,
    // device.metric
    DeviceMetricDataFuncTest.class,
    // util
    FileUtilFuncTest.class,
    GCSFileDownloaderFuncTest.class,
    GCSFileUploaderFuncTest.class,
    HttpHelperFuncTest.class,
    RunUtilFuncTest.class,
    ZipUtilFuncTest.class,
})
public class FuncTests {}
