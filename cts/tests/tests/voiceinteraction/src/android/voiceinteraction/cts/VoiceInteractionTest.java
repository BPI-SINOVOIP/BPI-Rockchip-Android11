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

package android.voiceinteraction.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.voiceinteraction.common.Utils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import androidx.test.rule.ActivityTestRule;

// TODO: ideally we should split testAll() into multiple tests, and run at least one of them
// on instant
@AppModeFull(reason = "DirectActionsTest is enough")
public class VoiceInteractionTest extends AbstractVoiceInteractionTestCase {
    static final String TAG = "VoiceInteractionTest";
    private static final int TIMEOUT_MS = 20 * 1000;

    private TestStartActivity mTestActivity;
    private TestResultsReceiver mReceiver;
    private Bundle mResults;
    private final CountDownLatch mLatch = new CountDownLatch(1);

    @Rule
    public final ActivityTestRule<TestStartActivity> mActivityTestRule =
            new ActivityTestRule<>(TestStartActivity.class, false, false);

    @Before
    public void setUp() throws Exception {
        mReceiver = new TestResultsReceiver();
        mContext.registerReceiver(mReceiver, new IntentFilter(Utils.BROADCAST_INTENT));
        startTestActivity();
    }

    @After
    public void tearDown() throws Exception {
        if (mReceiver != null) {
            try {
                mContext.unregisterReceiver(mReceiver);
            } catch (IllegalArgumentException e) {
                // This exception is thrown if mReceiver in
                // the above call to unregisterReceiver is never registered.
                // If so, no harm done by ignoring this exception.
            }
            mReceiver = null;
        }
    }

    private void startTestActivity() throws Exception {
        Intent intent = new Intent();
        intent.setAction("android.intent.action.TEST_START_ACTIVITY");
        intent.setComponent(new ComponentName(mContext, TestStartActivity.class));
        Log.v(TAG, "startTestActivity:" + intent);
        mTestActivity = mActivityTestRule.launchActivity(intent);
    }

    @Test
    public void testAll() throws Exception {
        VoiceInteractionTestReceiver.waitSessionStarted(5, TimeUnit.SECONDS);

        if (!mLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Failed to receive broadcast in " + TIMEOUT_MS + "msec");
            return;
        }
        if (mResults == null) {
            fail("no results received at all!");
            return;
        }
        int numFails = 0;
        for (Utils.TestCaseType t : Utils.TestCaseType.values()) {
            String singleResult = mResults.getString(t.toString());
            if (singleResult == null) {
                numFails++;
                Log.i(TAG, "No testresults received for " + t);
            } else {
                verifySingleTestcaseResult(t, singleResult);
            }
        }
        assertThat(numFails).isEqualTo(0);
        mTestActivity.finish();
    }

    private void verifySingleTestcaseResult(Utils.TestCaseType testCaseType, String result) {
        Log.i(TAG, "Received testresult: " + result + " for " + testCaseType);
        switch (testCaseType) {
          case ABORT_REQUEST_CANCEL_TEST:
              assertThat(result.equals(Utils.ABORT_REQUEST_CANCEL_SUCCESS)).isTrue();
              break;
          case ABORT_REQUEST_TEST:
              assertThat(result.equals(Utils.ABORT_REQUEST_SUCCESS)).isTrue();
              break;
          case COMMANDREQUEST_TEST:
              assertThat(result.equals(Utils.COMMANDREQUEST_SUCCESS)).isTrue();
              break;
          case COMMANDREQUEST_CANCEL_TEST:
              assertThat(result.equals(Utils.COMMANDREQUEST_CANCEL_SUCCESS)).isTrue();
              break;
          case COMPLETION_REQUEST_CANCEL_TEST:
              assertThat(result.equals(Utils.COMPLETION_REQUEST_CANCEL_SUCCESS)).isTrue();
              break;
          case COMPLETION_REQUEST_TEST:
              assertThat(result.equals(Utils.COMPLETION_REQUEST_SUCCESS)).isTrue();
              break;
          case CONFIRMATION_REQUEST_CANCEL_TEST:
              assertThat(result.equals(Utils.CONFIRMATION_REQUEST_CANCEL_SUCCESS)).isTrue();
              break;
          case CONFIRMATION_REQUEST_TEST:
              assertThat(result.equals(Utils.CONFIRMATION_REQUEST_SUCCESS)).isTrue();
              break;
          case PICKOPTION_REQUEST_CANCEL_TEST:
              assertThat(result.equals(Utils.PICKOPTION_REQUEST_CANCEL_SUCCESS)).isTrue();
              break;
          case PICKOPTION_REQUEST_TEST:
              assertThat(result.equals(Utils.PICKOPTION_REQUEST_SUCCESS)).isTrue();
              break;
          case SUPPORTS_COMMANDS_TEST:
              assertThat(result.equals(Utils.SUPPORTS_COMMANDS_SUCCESS)).isTrue();
              break;
          default:
              Log.wtf(TAG, "not expected");
              break;
        }
        Log.i(TAG, testCaseType + " passed");
    }


    class TestResultsReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equalsIgnoreCase(Utils.BROADCAST_INTENT)) {
                Log.i(TAG, "received broadcast with results ");
                VoiceInteractionTest.this.mResults = intent.getExtras();
                mLatch.countDown();
            }
        }
    }
}
