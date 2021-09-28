/*
 * Copyright 2020 The Android Open Source Project
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
package android.media.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.media.projection.MediaProjection;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test MediaProjection lifecycle.
 *
 * This test starts and stops a MediaProjection screen capture session using
 * MediaProjectionActivity.
 *
 * Currently we check that we are able to draw overlay windows during the session but not before
 * or after. (We request SYATEM_ALERT_WINDOW permission, but it is not granted, so by default we
 * cannot.)
 *
 * Note that there are other tests verifying that screen capturing actually works correctly in
 * CtsWindowManagerDeviceTestCases.
 */
@NonMediaMainlineTest
public class MediaProjectionTest {
    @Rule
    public ActivityTestRule<MediaProjectionActivity> mActivityRule =
            new ActivityTestRule<>(MediaProjectionActivity.class, false, false);

    private MediaProjectionActivity mActivity;
    private MediaProjection mMediaProjection;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getContext();
    }

    @Test
    public void testOverlayAllowedDuringScreenCapture() throws Exception {
        assertFalse(Settings.canDrawOverlays(mContext));

        startMediaProjection();
        assertTrue(Settings.canDrawOverlays(mContext));

        stopMediaProjection();
        assertFalse(Settings.canDrawOverlays(mContext));
    }

    private void startMediaProjection() throws Exception {
        mActivityRule.launchActivity(null);
        mActivity = mActivityRule.getActivity();
        mMediaProjection = mActivity.waitForMediaProjection();
    }

    private void stopMediaProjection() throws Exception {
        final int STOP_TIMEOUT_MS = 1000;
        CountDownLatch stoppedLatch = new CountDownLatch(1);

        mMediaProjection.registerCallback(new MediaProjection.Callback() {
            public void onStop() {
                stoppedLatch.countDown();
            }
        }, new Handler(Looper.getMainLooper()));
        mMediaProjection.stop();

        assertTrue("Could not stop the MediaProjection in " + STOP_TIMEOUT_MS + "ms",
                stoppedLatch.await(STOP_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }
}
