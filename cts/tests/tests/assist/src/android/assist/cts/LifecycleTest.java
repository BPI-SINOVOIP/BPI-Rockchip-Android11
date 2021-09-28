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
import android.os.Bundle;
import android.util.Log;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

/** Test we receive proper assist data when context is disabled or enabled */
public class LifecycleTest extends AssistTestBase {
    private static final String TAG = "LifecycleTest";
    private static final String ACTION_HAS_FOCUS = Utils.LIFECYCLE_HASFOCUS;
    private static final String ACTION_LOST_FOCUS = Utils.LIFECYCLE_LOSTFOCUS;
    private static final String ACTION_ON_PAUSE = Utils.LIFECYCLE_ONPAUSE;
    private static final String ACTION_ON_STOP = Utils.LIFECYCLE_ONSTOP;
    private static final String ACTION_ON_DESTROY = Utils.LIFECYCLE_ONDESTROY;

    private static final String TEST_CASE_TYPE = Utils.LIFECYCLE;

    private AutoResetLatch mHasFocusLatch = new AutoResetLatch(1);
    private AutoResetLatch mLostFocusLatch = new AutoResetLatch(1);
    private AutoResetLatch mActivityLifecycleLatch = new AutoResetLatch(1);
    private AutoResetLatch mDestroyLatch = new AutoResetLatch(1);
    private boolean mLostFocusIsLifecycle;

    @Override
    protected void customSetup() throws Exception {
        mActionLatchReceiver = new LifecycleTestReceiver();
        mLostFocusIsLifecycle = false;
        startTestActivity(TEST_CASE_TYPE);
    }

    private void waitForHasFocus() throws Exception {
        Log.i(TAG, "waiting for window focus gain before continuing");
        if (!mHasFocusLatch.await(Utils.ACTIVITY_ONRESUME_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Activity failed to get focus in " + Utils.ACTIVITY_ONRESUME_TIMEOUT_MS + "msec");
        }
    }

    private void waitForLostFocus() throws Exception {
        Log.i(TAG, "waiting for window focus lost before continuing");
        if (!mLostFocusLatch.await(Utils.ACTIVITY_ONRESUME_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Activity failed to lose focus in " + Utils.ACTIVITY_ONRESUME_TIMEOUT_MS + "msec");
        }
    }

    private void waitAndSeeIfLifecycleMethodsAreTriggered() throws Exception {
        if (mActivityLifecycleLatch.await(Utils.TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("One or more lifecycle methods were called after triggering assist");
        }
    }

    private void waitForDestroy() throws Exception {
        Log.i(TAG, "waiting for activity destroy before continuing");
        if (!mDestroyLatch.await(Utils.ACTIVITY_ONRESUME_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Activity failed to destroy in " + Utils.ACTIVITY_ONRESUME_TIMEOUT_MS + "msec");
        }
    }

    @Test
    public void testLayerDoesNotTriggerLifecycleMethods() throws Exception {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_VOICE_RECOGNIZERS)) {
            Log.d(TAG, "Not running assist tests - voice_recognizers feature is not supported");
            return;
        }
        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();
        start3pApp(TEST_CASE_TYPE);
        waitForHasFocus();
        final AutoResetLatch latch = startSession();
        waitForContext(latch);
        // Since there is no UI, focus should not be lost.  We are counting focus lost as
        // a lifecycle event in this case.
        // Do this after waitForContext(), since we don't start looking for context until
        // calling the above (RACY!!!).
        waitForLostFocus();
        waitAndSeeIfLifecycleMethodsAreTriggered();

        Bundle bundle = new Bundle();
        bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, Utils.HIDE_LIFECYCLE_ACTIVITY);
        m3pActivityCallback.sendResult(bundle);

        waitForDestroy();
    }

    @Test
    public void testNoUiLayerDoesNotTriggerLifecycleMethods() throws Exception {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }
        mLostFocusIsLifecycle = true;
        startTest(Utils.LIFECYCLE_NOUI);
        waitForAssistantToBeReady();
        start3pApp(Utils.LIFECYCLE_NOUI);
        waitForHasFocus();
        final AutoResetLatch latch = startSession();
        waitForContext(latch);
        // Do this after waitForContext(), since we don't start looking for context until
        // calling the above (RACY!!!).
        waitAndSeeIfLifecycleMethodsAreTriggered();

        Bundle bundle = new Bundle();
        bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, Utils.HIDE_LIFECYCLE_ACTIVITY);
        m3pActivityCallback.sendResult(bundle);

        waitForDestroy();
    }

    private class LifecycleTestReceiver extends ActionLatchReceiver {

        @Override
        protected void onAction(Bundle bundle, String action) {
            if (action.equals(ACTION_HAS_FOCUS) && mHasFocusLatch != null) {
                mHasFocusLatch.countDown();
            } else if (action.equals(ACTION_LOST_FOCUS) && mLostFocusLatch != null) {
                if (mLostFocusIsLifecycle) {
                    mActivityLifecycleLatch.countDown();
                } else {
                    mLostFocusLatch.countDown();
                }
            } else if (action.equals(ACTION_ON_PAUSE) && mActivityLifecycleLatch != null) {
                mActivityLifecycleLatch.countDown();
            } else if (action.equals(ACTION_ON_STOP) && mActivityLifecycleLatch != null) {
                mActivityLifecycleLatch.countDown();
            } else if (action.equals(ACTION_ON_DESTROY) && mActivityLifecycleLatch != null) {
                mActivityLifecycleLatch.countDown();
                mDestroyLatch.countDown();
            } else {
                super.onAction(bundle, action);
            }
        }
    }
}
