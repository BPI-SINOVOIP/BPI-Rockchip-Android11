/*
 * Copyright 2019 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.media.AudioSystem;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Ensures proper support of internal @TestApi functionality for AudioSystem.
 * This is a framework consistency test of important internal APIs.
 * Java applications should use the client facing AudioManager APIs for Audio management.
 */

@Presubmit
@NonMediaMainlineTest
@RunWith(AndroidJUnit4.class)
@SmallTest
@AppModeFull(reason = "Instant applications do not have permission MODIFY_AUDIO_SETTINGS")
public class AudioSystemTest {

    /**
     * Tests AudioSystem.setMasterBalance and AudioSystem.getMasterBalance
     *
     * @throws Exception
     */
    @Test
    public void testBalance() throws Exception {
        final float DELTA = 0.f; // float values must be exact

        // original balance must be valid
        final float originalBalance = AudioSystem.getMasterBalance();
        assertTrue("original balance must be within -1.f to 1.f " + originalBalance,
                originalBalance <= 1.f && originalBalance >= -1.f);

        try {
            // can we set with valid values?
            final float[] GOOD_BALANCE_VALUES = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
            for (final float b : GOOD_BALANCE_VALUES) {
                assertEquals("set must return NO_ERROR", 0, AudioSystem.setMasterBalance(b));
                assertEquals("get must return set value " + b,
                        b, AudioSystem.getMasterBalance(), DELTA);
            }

            // can we restore?
            AudioSystem.setMasterBalance(originalBalance);
            assertEquals("get must return set value",
                    originalBalance, AudioSystem.getMasterBalance(), DELTA);

            // do we reject invalid values?
            final float[] BAD_BALANCE_VALUES = {-2.f, 2.f, // out of bounds
                    Float.POSITIVE_INFINITY, Float.NaN, Float.NEGATIVE_INFINITY, // special values
            };
            for (final float b : BAD_BALANCE_VALUES) {
                assertTrue("set returns error on invalid value",
                        0 != AudioSystem.setMasterBalance(b));
                assertEquals("get reads old value on invalid set",
                        originalBalance, AudioSystem.getMasterBalance(), DELTA);
            }
            // we are at original balance here.

        } finally {
            // always attempt to restore original balance if we got it successfully.
            AudioSystem.setMasterBalance(originalBalance);
        }
    }
}
