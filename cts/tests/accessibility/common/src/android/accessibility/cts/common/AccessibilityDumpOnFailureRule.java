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
 * limitations under the License
 */

package android.accessibility.cts.common;

import androidx.test.rule.ActivityTestRule;

import org.junit.rules.ExternalResource;
import org.junit.rules.RuleChain;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

/**
 * Custom {@code TestRule} that dump accessibility related data upon test failures.
 *
 * <p>Note: when using other {@code TestRule}s, make sure to use a {@link RuleChain} to ensure it
 * is applied outside of other rules that can fail a test (otherwise this rule may not know that the
 * test failed). If using with {@link ExternalResource}-like {@code TestRule}s, {@link
 * ActivityTestRule} or {@link InstrumentedAccessibilityService}, this rule should chaining as a
 * inner rule to resources-like rules so that it will dump data before resources are cleaned up.
 *
 * <p>To capture the output of this rule, add the following to AndroidTest.xml:
 * <pre>
 *  <!-- Collect output of AccessibilityDumpOnFailureRule. -->
 *  <metrics_collector class="com.android.tradefed.device.metric.FilePullerLogCollector">
 *    <option name="directory-keys" value="/sdcard/<test.package.name>" />
 *    <option name="collect-on-run-ended-only" value="true" />
 *  </metrics_collector>
 * </pre>
 * <p>And disable external storage isolation:
 * <pre>
 *  <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
 *  <application ... android:requestLegacyExternalStorage="true" ... >
 * </pre>
 */
public class AccessibilityDumpOnFailureRule extends TestWatcher {

    public void dump(int flag) {
        AccessibilityDumper.getInstance().dump(flag);
    }

    @Override
    protected void starting(Description description) {
        AccessibilityDumper.getInstance().setName(getTestNameFrom(description));
    }

    @Override
    protected void failed(Throwable t, Description description) {
        AccessibilityDumper.getInstance().dump();
    }

    private String getTestNameFrom(Description description) {
        return description.getTestClass().getSimpleName()
                + "_" + description.getMethodName();
    }
}
