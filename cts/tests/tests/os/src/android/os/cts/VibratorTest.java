/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.os.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.app.UiAutomation;
import android.media.AudioAttributes;
import android.os.cts.SimpleTestActivity;
import android.os.SystemClock;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.Vibrator.OnVibratorStateChangedListener;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class VibratorTest {
    @Rule
    public ActivityTestRule<SimpleTestActivity> mActivityRule = new ActivityTestRule<>(
            SimpleTestActivity.class);

    private static final AudioAttributes AUDIO_ATTRIBUTES =
            new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build();
    private static final long CALLBACK_TIMEOUT_MILLIS = 5000;

    private Vibrator mVibrator;
    @Mock
    private OnVibratorStateChangedListener mListener1;
    @Mock
    private OnVibratorStateChangedListener mListener2;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mVibrator = InstrumentationRegistry.getContext().getSystemService(Vibrator.class);
    }

    @Test
    public void testVibratorCancel() {
        mVibrator.vibrate(1000);
        sleep(500);
        mVibrator.cancel();
    }

    @Test
    public void testVibratePattern() {
        long[] pattern = {100, 200, 400, 800, 1600};
        mVibrator.vibrate(pattern, 3);
        try {
            mVibrator.vibrate(pattern, 10);
            fail("Should throw ArrayIndexOutOfBoundsException");
        } catch (ArrayIndexOutOfBoundsException expected) { }
        mVibrator.cancel();
    }

    @Test
    public void testVibrateMultiThread() {
        new Thread(new Runnable() {
            public void run() {
                try {
                    mVibrator.vibrate(100);
                } catch (Exception e) {
                    fail("MultiThread fail1");
                }
            }
        }).start();
        new Thread(new Runnable() {
            public void run() {
                try {
                    // This test only get two threads to run vibrator at the same time
                    // for a functional test,
                    // but it can not verify if the second thread get the precedence.
                    mVibrator.vibrate(1000);
                } catch (Exception e) {
                    fail("MultiThread fail2");
                }
            }
        }).start();
        sleep(1500);
    }

    @Test
    public void testVibrateOneShot() {
        VibrationEffect oneShot =
                VibrationEffect.createOneShot(100, VibrationEffect.DEFAULT_AMPLITUDE);
        mVibrator.vibrate(oneShot);
        sleep(100);

        oneShot = VibrationEffect.createOneShot(500, 255 /* Max amplitude */);
        mVibrator.vibrate(oneShot);
        sleep(100);
        mVibrator.cancel();

        oneShot = VibrationEffect.createOneShot(100, 1 /* Min amplitude */);
        mVibrator.vibrate(oneShot, AUDIO_ATTRIBUTES);
        sleep(100);
    }

    @Test
    public void testVibrateWaveform() {
        final long[] timings = new long[] {100, 200, 300, 400, 500};
        final int[] amplitudes = new int[] {64, 128, 255, 128, 64};
        VibrationEffect waveform = VibrationEffect.createWaveform(timings, amplitudes, -1);
        mVibrator.vibrate(waveform);
        sleep(1500);

        waveform = VibrationEffect.createWaveform(timings, amplitudes, 0);
        mVibrator.vibrate(waveform, AUDIO_ATTRIBUTES);
        sleep(2000);
        mVibrator.cancel();
    }

    @Test
    public void testVibratorHasAmplitudeControl() {
        // Just make sure it doesn't crash when this is called; we don't really have a way to test
        // if the amplitude control works or not.
        mVibrator.hasAmplitudeControl();
    }

    @Test
    public void testVibratorEffectsAreSupported() {
        // Just make sure it doesn't crash when this is called and that it returns all queries;
        // We don't really have a way to test if the device supports each effect or not.
        int[] result = mVibrator.areEffectsSupported(
                VibrationEffect.EFFECT_TICK, VibrationEffect.EFFECT_CLICK);

        assertEquals(2, result.length);
        assertEquals(0, mVibrator.areEffectsSupported().length);
    }

    @Test
    public void testVibratorAllEffectsAreSupported() {
        // Just make sure it doesn't crash when this is called;
        // We don't really have a way to test if the device supports each effect or not.
        mVibrator.areAllEffectsSupported(
                VibrationEffect.EFFECT_TICK,
                VibrationEffect.EFFECT_CLICK,
                VibrationEffect.EFFECT_DOUBLE_CLICK,
                VibrationEffect.EFFECT_HEAVY_CLICK);

        assertEquals(Vibrator.VIBRATION_EFFECT_SUPPORT_YES, mVibrator.areAllEffectsSupported());
    }

    @Test
    public void testVibratorPrimitivesAreSupported() {
        // Just make sure it doesn't crash when this is called;
        // We don't really have a way to test if the device supports each effect or not.
        boolean[] result = mVibrator.arePrimitivesSupported(
                VibrationEffect.Composition.PRIMITIVE_CLICK,
                VibrationEffect.Composition.PRIMITIVE_QUICK_RISE,
                VibrationEffect.Composition.PRIMITIVE_TICK);

        assertEquals(3, result.length);
        assertEquals(0, mVibrator.arePrimitivesSupported().length);
    }

    @Test
    public void testVibratorAllPrimitivesAreSupported() {
        // Just make sure it doesn't crash when this is called;
        // We don't really have a way to test if the device supports each effect or not.
        mVibrator.areAllPrimitivesSupported(
                VibrationEffect.Composition.PRIMITIVE_TICK);

        assertTrue(mVibrator.areAllPrimitivesSupported());
    }

    /**
     * For devices with vibrator we assert the IsVibrating state, for devices without vibrator just
     * ensure it won't crash with IsVibrating call.
     */
    private void assertIsVibrating(boolean expected) {
        final boolean isVibrating = mVibrator.isVibrating();
        if (mVibrator.hasVibrator()) {
            assertEquals(isVibrating, expected);
        }
    }

    @Test
    public void testVibratorIsVibrating() {
        final UiAutomation ui = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        ui.adoptShellPermissionIdentity("android.permission.ACCESS_VIBRATOR_STATE");
        assertIsVibrating(false);
        mVibrator.vibrate(1000);
        assertIsVibrating(true);
        mVibrator.cancel();
        assertIsVibrating(false);
    }

    @Test
    public void testVibratorVibratesNoLongerThanDuration() {
        final UiAutomation ui = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        ui.adoptShellPermissionIdentity("android.permission.ACCESS_VIBRATOR_STATE");
        assertIsVibrating(false);
        mVibrator.vibrate(100);
        SystemClock.sleep(150);
        assertIsVibrating(false);
    }

    @Test
    public void testVibratorStateCallback() throws Exception {
        final UiAutomation ui = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        ui.adoptShellPermissionIdentity("android.permission.ACCESS_VIBRATOR_STATE");
        // Add listener1
        mVibrator.addVibratorStateListener(mListener1);
        // Add listener2 on main thread.
        mVibrator.addVibratorStateListener(mListener2);
        verify(mListener1, timeout(CALLBACK_TIMEOUT_MILLIS)
                .times(1)).onVibratorStateChanged(false);
        verify(mListener2, timeout(CALLBACK_TIMEOUT_MILLIS)
                .times(1)).onVibratorStateChanged(false);

        mVibrator.vibrate(1000);
        assertIsVibrating(true);

        verify(mListener1, timeout(CALLBACK_TIMEOUT_MILLIS)
                .times(1)).onVibratorStateChanged(true);
        verify(mListener2, timeout(CALLBACK_TIMEOUT_MILLIS)
                .times(1)).onVibratorStateChanged(true);

        reset(mListener1);
        reset(mListener2);
        // Remove listener1
        mVibrator.removeVibratorStateListener(mListener1);
        mVibrator.removeVibratorStateListener(mListener2);

        mVibrator.cancel();
        assertIsVibrating(false);
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException ignored) { }
    }
}
