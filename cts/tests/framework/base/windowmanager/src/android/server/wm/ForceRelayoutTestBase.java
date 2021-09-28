/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static android.view.WindowInsets.Type.navigationBars;
import static android.view.WindowInsets.Type.statusBars;
import static android.view.WindowInsets.Type.systemBars;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;

import android.app.Activity;
import android.graphics.Insets;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;

import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import org.junit.Rule;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class ForceRelayoutTestBase {

    @Rule
    public ActivityTestRule<TestActivity> mDecorActivity = new ActivityTestRule<>(
            TestActivity.class);

    void testRelayoutWhenInsetsChange(boolean expectRelayoutWhenInsetsChange)
            throws Throwable {
        TestActivity activity = mDecorActivity.getActivity();
        assertNotNull("test setup failed", activity.mLastContentInsets);
        assumeFalse(Insets.NONE.equals(activity.mLastContentInsets.getInsetsIgnoringVisibility(
                statusBars() | navigationBars())));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            activity.mLayoutHappened = false;
            activity.mMeasureHappened = false;
            activity.getWindow().getInsetsController().hide(systemBars());
        });
        activity.mZeroInsets.await(180, TimeUnit.SECONDS);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            if (expectRelayoutWhenInsetsChange) {
                assertTrue(activity.mLayoutHappened);
                assertTrue(activity.mMeasureHappened);
            } else {
                assertFalse(activity.mLayoutHappened);
                assertFalse(activity.mMeasureHappened);
            }
        });
    }

    public static class TestActivity extends Activity {
        WindowInsets mLastContentInsets;
        final CountDownLatch mZeroInsets = new CountDownLatch(1);

        volatile boolean mLayoutHappened;
        volatile boolean mMeasureHappened;

        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().requestFeature(Window.FEATURE_NO_TITLE);
            getWindow().setDecorFitsSystemWindows(false);
            getWindow().getAttributes().layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            View view = new View(this) {
                @Override
                protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
                    super.onLayout(changed, left, top, right, bottom);
                    mLayoutHappened = true;
                }

                @Override
                protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                    super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                    mMeasureHappened = true;

                }
            };
            view.setOnApplyWindowInsetsListener((v, wi) -> {
                mLastContentInsets = wi;
                if (wi.getInsets(systemBars()).equals(Insets.NONE)) {

                    // Make sure full traversal happens before counting down.
                    v.post(mZeroInsets::countDown);
                }
                return WindowInsets.CONSUMED;
            });
            setContentView(view);
        }
    }
}
