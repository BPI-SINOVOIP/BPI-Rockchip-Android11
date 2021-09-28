/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.theme.app;

import static org.junit.Assert.assertTrue;

import android.app.Instrumentation;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * Test used to instrument generation of reference images.
 */
@RunWith(AndroidJUnit4.class)
public class ReferenceImagesTest {
    private Instrumentation mInstrumentation;
    private GenerateImagesActivity mActivity;

    // Overall test timeout is 30 minutes. Should only take about 5.
    private static final int TEST_RESULT_TIMEOUT = 30 * 60 * 1000;

    @Rule
    public ActivityTestRule<GenerateImagesActivity> mActivityRule =
        new ActivityTestRule<>(GenerateImagesActivity.class);

    @Before
    public void setup() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mInstrumentation.setInTouchMode(true);

        mActivity = mActivityRule.getActivity();
    }

    @Test
    public void testGenerateReferenceImages() throws Exception {
        final GenerateImagesActivity activity = mActivity;
        assertTrue("Activity failed to complete within " + TEST_RESULT_TIMEOUT + " ms",
                activity.waitForCompletion(TEST_RESULT_TIMEOUT));
        assertTrue(activity.getFinishReason(), activity.isFinishSuccess());

        final File outputZip = activity.getOutputZip();
        assertTrue("Failed to generate reference image ZIP",
                outputZip != null && outputZip.exists());
    }
}
