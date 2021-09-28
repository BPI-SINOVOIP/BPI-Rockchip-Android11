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

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.fail;

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.util.Log;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

/**
 *  Test that the AssistStructure returned is properly formatted.
 */

public class AssistStructureTest extends AssistTestBase {
    private static final String TAG = "AssistStructureTest";
    private static final String TEST_CASE_TYPE = Utils.ASSIST_STRUCTURE;

    private AutoResetLatch mHasDrawedLatch;

    @Override
    protected void customSetup() throws Exception {
        mHasDrawedLatch = new AutoResetLatch(1);
        mActionLatchReceiver = new ActionLatchReceiver(Utils.APP_3P_HASDRAWED, mHasDrawedLatch);
        startTestActivity(TEST_CASE_TYPE);
    }

    private void waitForOnDraw() throws Exception {
        Log.i(TAG, "waiting for onDraw() before continuing");
        if (!mHasDrawedLatch.await(Utils.ACTIVITY_ONRESUME_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Activity failed to draw in " + Utils.ACTIVITY_ONRESUME_TIMEOUT_MS + "msec");
        }
    }

    @Test
    public void testAssistStructure() throws Throwable {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_VOICE_RECOGNIZERS)) {
            Log.d(TAG, "Not running assist tests - voice_recognizers feature is not supported");
            return;
        }
        start3pApp(TEST_CASE_TYPE);
        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();
        waitForOnDraw();
        final AutoResetLatch latch = startSession();
        waitForContext(latch);
        getInstrumentation().waitForIdleSync();
        getInstrumentation().runOnMainSync(() -> {
            verifyAssistDataNullness(false, false, false, false);
            verifyAssistStructure(Utils.getTestAppComponent(TEST_CASE_TYPE),
                    false /*FLAG_SECURE set*/);
        });
    }
}
