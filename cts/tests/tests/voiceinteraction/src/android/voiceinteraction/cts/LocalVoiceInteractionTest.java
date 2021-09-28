/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.rule.ActivityTestRule;

@RunWith(AndroidJUnit4.class)
public class LocalVoiceInteractionTest extends AbstractVoiceInteractionTestCase {

    private static final String TAG = LocalVoiceInteractionTest.class.getSimpleName();
    private static final int TIMEOUT_MS = 20 * 1000;


    private TestLocalInteractionActivity mTestActivity;
    private final CountDownLatch mLatchStart = new CountDownLatch(1);
    private final CountDownLatch mLatchStop = new CountDownLatch(1);

    @Rule
    public final ActivityTestRule<TestLocalInteractionActivity> mActivityTestRule =
            new ActivityTestRule<>(TestLocalInteractionActivity.class, false, false);

    @Before
    public void setUp() throws Exception {
        startTestActivity();
    }

    private void startTestActivity() throws Exception {
        Intent intent = new Intent()
                .setAction("android.intent.action.TEST_LOCAL_INTERACTION_ACTIVITY");
        Log.i(TAG, "startTestActivity: " + intent);
        mTestActivity = mActivityTestRule.launchActivity(intent);
    }

    @Test
    public void testLifecycle() throws Exception {
        assertWithMessage("Doesn't support LocalVoiceInteraction")
                .that(mTestActivity.isLocalVoiceInteractionSupported()).isTrue();
        mTestActivity.startLocalInteraction(mLatchStart);
        if (!mLatchStart.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Failed to start voice interaction in " + TIMEOUT_MS + "msec");
            return;
        }
        mTestActivity.stopLocalInteraction(mLatchStop);
        if (!mLatchStop.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Failed to stop voice interaction in " + TIMEOUT_MS + "msec");
            return;
        }
        mTestActivity.finish();
    }
}
