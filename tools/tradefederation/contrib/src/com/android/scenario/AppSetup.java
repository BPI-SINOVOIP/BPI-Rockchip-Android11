/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.scenario;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.AndroidJUnitTest;

import com.google.common.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;

/** A test that runs app setup scenarios only. */
@OptionClass(alias = "app-setup")
public class AppSetup extends AndroidJUnitTest {
    @Option(
        name = "drop-cache-when-finished",
        description = "Clear the cache when setup is finished."
    )
    private boolean mDropCacheWhenFinished = false;

    @Option(
        name = "apps-to-kill-when-finished",
        description = "List of app package names to kill when setup is finished."
    )
    private List<String> mAppsToKillWhenFinished = new ArrayList<>();

    @Option(
            name = "disable",
            description = "Set it to true to disable AppSetup test."
        )
    private boolean mDisable = false;

    static final String DROP_CACHE_COMMAND = "echo 3 > /proc/sys/vm/drop_caches";
    static final String KILL_APP_COMMAND_TEMPLATE = "am force-stop %s";

    public AppSetup() {
        super();
        // Specifically target the app setup scenarios.
        setPackageName("android.platform.test.scenario");
        addIncludeAnnotation("android.platform.test.scenario.annotation.AppSetup");
    }

    /**
     * Run the test the same way the superclass does and perform the additional setup/cleanup steps.
     */
    @Override
    public void run(final ITestInvocationListener listener) throws DeviceNotAvailableException {
        if(mDisable) {
            return;
        }
        runTest(listener);

        // TODO(harrytczhang@): Switch to a solution based on test rule injection after b/123281375.
        if (mDropCacheWhenFinished) {
            getDevice().executeShellCommand(DROP_CACHE_COMMAND);
        }
        for (String packageName : mAppsToKillWhenFinished) {
            getDevice().executeShellCommand(String.format(KILL_APP_COMMAND_TEMPLATE, packageName));
        }
    }

    /**
     * Enable tests to stub out the actual run.
     *
     * @hide
     */
    @VisibleForTesting
    protected void runTest(final ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        super.run(listener);
    }
}
