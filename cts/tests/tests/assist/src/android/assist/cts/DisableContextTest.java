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

package android.assist.cts;

import android.assist.common.Utils;
import android.util.Log;

import org.junit.Test;

/** Test we receive proper assist data when context is disabled or enabled */
public class DisableContextTest extends AssistTestBase {
    static final String TAG = "DisableContextTest";

    private static final String TEST_CASE_TYPE = Utils.DISABLE_CONTEXT;

    @Override
    protected void customSetup() throws Exception {
        startTestActivity(TEST_CASE_TYPE);
    }

    @Override
    public void customTearDown() throws Exception {
        setFeaturesEnabled(StructureEnabled.TRUE, ScreenshotEnabled.TRUE);
        logContextAndScreenshotSetting();
    }

    @Test
    public void testContextAndScreenshotOff() throws Exception {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_VOICE_RECOGNIZERS)) {
            Log.d(TAG, "Not running assist tests - voice_recognizers feature is not supported");
            return;
        }
        // Both settings off
        Log.i(TAG, "DisableContext: Screenshot OFF, Context OFF");
        setFeaturesEnabled(StructureEnabled.FALSE, ScreenshotEnabled.FALSE);
        startTest(TEST_CASE_TYPE);
        waitForContext(mAssistDataReceivedLatch);

        logContextAndScreenshotSetting();
        verifyAssistDataNullness(true, true, true, true);
    }

    @Test
    public void testContextOff() throws Exception {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_VOICE_RECOGNIZERS)) {
            Log.d(TAG, "Not running assist tests - voice_recognizers feature is not supported");
            return;
        }
        // Screenshot off, context on
        Log.i(TAG, "DisableContext: Screenshot OFF, Context ON");
        setFeaturesEnabled(StructureEnabled.TRUE, ScreenshotEnabled.FALSE);
        startTest(TEST_CASE_TYPE);
        waitForContext(mAssistDataReceivedLatch);

        logContextAndScreenshotSetting();
        verifyAssistDataNullness(false, false, false, true);
    }

    @Test
    public void testScreenshotOff() throws Exception {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_VOICE_RECOGNIZERS)) {
            Log.d(TAG, "Not running assist tests - voice_recognizers feature is not supported");
            return;
        }
        // Context off, screenshot on
        Log.i(TAG, "DisableContext: Screenshot ON, Context OFF");
        setFeaturesEnabled(StructureEnabled.FALSE, ScreenshotEnabled.TRUE);
        startTest(TEST_CASE_TYPE);
        waitForContext(mAssistDataReceivedLatch);

        logContextAndScreenshotSetting();
        verifyAssistDataNullness(true, true, true, true);
    }
}
