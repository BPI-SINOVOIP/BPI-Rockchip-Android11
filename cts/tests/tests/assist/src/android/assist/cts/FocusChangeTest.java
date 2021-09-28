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

import static org.junit.Assert.fail;

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.util.Log;
import android.util.Pair;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

/** Test that triggering the Assistant causes the underlying Activity to lose focus **/
public class FocusChangeTest extends AssistTestBase {
    private static final String TAG = "FocusChangeTest";
    private static final String TEST_CASE_TYPE = Utils.FOCUS_CHANGE;

    private AutoResetLatch mHasGainedFocusLatch = new AutoResetLatch(1);
    private AutoResetLatch mHasLostFocusLatch = new AutoResetLatch(1);

    @Override
    protected void customSetup() throws Exception {
        mActionLatchReceiver = new ActionLatchReceiver(
                Pair.create(Utils.GAINED_FOCUS, mHasGainedFocusLatch),
                Pair.create(Utils.LOST_FOCUS, mHasLostFocusLatch)
        );
        startTestActivity(TEST_CASE_TYPE);
    }

    private void waitToGainFocus() throws Exception {
        Log.i(TAG, "Waiting for the underlying activity to gain focus before continuing.");
        if (!mHasGainedFocusLatch.await(Utils.TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Activity failed to gain focus in " + Utils.TIMEOUT_MS + "msec.");
        }
    }

    private void waitToLoseFocus() throws Exception {
        Log.i(TAG, "Waiting for the underlying activity to lose focus.");
        if (!mHasLostFocusLatch.await(Utils.TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Activity maintained focus despite the Assistant Firing"
                    + Utils.TIMEOUT_MS + "msec.");
        }
    }

    @Test
    public void testLayerCausesUnderlyingActivityToLoseFocus() throws Exception {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }
        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();
        start3pApp(TEST_CASE_TYPE);
        waitToGainFocus();
        startSession();
        waitToLoseFocus();
    }
}
