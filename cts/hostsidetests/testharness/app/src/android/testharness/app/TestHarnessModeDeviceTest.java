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

package android.testharness.app;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import android.app.Instrumentation;
import android.app.ActivityManager;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;

/** Device-side tests for Test Harness Mode */
@RunWith(AndroidJUnit4.class)
public class TestHarnessModeDeviceTest {
    @Rule
    public ActivityTestRule<TestHarnessActivity> mActivityRule =
            new ActivityTestRule<>(android.testharness.app.TestHarnessActivity.class);

    @Test
    public void ensureActivityNotObscuredByKeyboardSetUpScreen() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        TestHarnessActivity activity = mActivityRule.getActivity();
        PollingCheck.waitFor(activity::isKeyboardVisible);

        UiDevice device = UiDevice.getInstance(instrumentation);
        Assert.assertTrue(device.hasObject(By.res("android.testharness.app", "text_view")));
    }

    @Test
    public void dirtyDevice() {
        TestHarnessActivity activity = mActivityRule.getActivity();
        PollingCheck.waitFor(activity::hasWindowFocus);

        activity.dirtyDevice();
    }

    @Test
    public void testDeviceIsClean() {
        TestHarnessActivity activity = mActivityRule.getActivity();
        PollingCheck.waitFor(activity::hasWindowFocus);

        Assert.assertTrue(activity.isDeviceClean());
    }

    @Test
    public void testDeviceInTestHarnessMode() {
        Assert.assertTrue(ActivityManager.isRunningInUserTestHarness());
    }
}
