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

package com.android.compatibility;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.testtype.InstrumentationTest;

/** Uses AppCompatibilityRunner to check if the app starts correctly. */
@OptionClass(alias = "app-compatibility")
public final class AppLaunchCompatibilityTest extends AppCompatibilityTest {
    private static final String APP_LAUNCH_TIMEOUT_LABEL = "app_launch_timeout_ms";
    private static final String WORKSPACE_LAUNCH_TIMEOUT_LABEL = "workspace_launch_timeout_ms";

    @Option(
            name = "app-launch-timeout-ms",
            description = "Time to wait for app to launch in msecs.")
    private int mAppLaunchTimeoutMs = 15000;

    @Option(
            name = "workspace-launch-timeout-ms",
            description = "Time to wait when launched back into the workspace in msecs.")
    private int mWorkspaceLaunchTimeoutMs = 2000;

    public AppLaunchCompatibilityTest() {
        super(
                "com.android.compatibilitytest",
                "com.android.compatibilitytest.AppCompatibilityRunner",
                "package_to_launch");
    }

    @Override
    public InstrumentationTest createInstrumentationTest(String packageBeingTested) {
        InstrumentationTest instrTest = createDefaultInstrumentationTest(packageBeingTested);
        instrTest.addInstrumentationArg(
                APP_LAUNCH_TIMEOUT_LABEL, Integer.toString(mAppLaunchTimeoutMs));
        instrTest.addInstrumentationArg(
                WORKSPACE_LAUNCH_TIMEOUT_LABEL, Integer.toString(mWorkspaceLaunchTimeoutMs));
        return instrTest;
    }
}
