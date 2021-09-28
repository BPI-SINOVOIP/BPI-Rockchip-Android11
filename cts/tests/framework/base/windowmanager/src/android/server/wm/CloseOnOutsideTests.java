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

package android.server.wm;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.app.Instrumentation;
import android.util.DisplayMetrics;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.ShellUtils;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests {@link android.view.Window#setCloseOnTouchOutside(boolean)} through exposed Activity API.
 */
@RunWith(AndroidJUnit4.class)
public class CloseOnOutsideTests {

    @Rule
    public final ActivityTestRule<CloseOnOutsideTestActivity> mTestActivity =
            new ActivityTestRule<>(CloseOnOutsideTestActivity.class, true, true);

    @Test
    public void withDefaults() {
        touchAndAssert(false /* shouldBeFinishing */);
    }

    @Test
    public void finishTrue() {
        mTestActivity.getActivity().setFinishOnTouchOutside(true);
        touchAndAssert(true /* shouldBeFinishing */);
    }

    @Test
    public void finishFalse() {
        mTestActivity.getActivity().setFinishOnTouchOutside(false);
        touchAndAssert(false /* shouldBeFinishing */);
    }

    // Tap the bottom right and check the Activity is finishing
    private void touchAndAssert(boolean shouldBeFinishing) {
        DisplayMetrics displayMetrics =
                mTestActivity.getActivity().getResources().getDisplayMetrics();
        int width = (int) (displayMetrics.widthPixels * 0.875f);
        int height = (int) (displayMetrics.heightPixels * 0.875f);

        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();

        // To be safe, make sure nothing else is finishing the Activity
        instrumentation.runOnMainSync(() -> assertFalse(mTestActivity.getActivity().isFinishing()));

        ShellUtils.runShellCommand("input tap %d %d", width, height);

        instrumentation.runOnMainSync(
                () -> assertEquals(shouldBeFinishing, mTestActivity.getActivity().isFinishing()));
    }
}
