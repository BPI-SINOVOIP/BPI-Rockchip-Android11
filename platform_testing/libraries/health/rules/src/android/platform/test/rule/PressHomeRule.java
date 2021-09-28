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
package android.platform.test.rule;

import android.os.SystemClock;

import org.junit.runner.Description;

/**
 * This rule will navigate to home at the end of each test method.
 * TODO: Revisit this after b/132100677 is fixed.
 */
public class PressHomeRule extends TestWatcher {

    static final String GO_HOME = "press-home";

    @Override
    protected void finished(Description description) {
        // Navigate to home after the test method is executed.
        boolean goHome = Boolean.parseBoolean(getArguments().getString(GO_HOME, "true"));
        if (!goHome) {
            return;
        }

        getUiDevice().pressHome();
        // Sleep for statsd to update the metrics.
        SystemClock.sleep(3000);

    }
}
