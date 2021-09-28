/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.fail;

import android.os.Parcel;
import android.os.VibrationEffect;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class VibrationEffectTest {
    private static final long TEST_TIMING = 100;
    private static final int TEST_AMPLITUDE = 100;

    private static final long[] TEST_TIMINGS = new long[] { 100, 100, 200 };
    private static final int[] TEST_AMPLITUDES =
            new int[] { 255, 0, VibrationEffect.DEFAULT_AMPLITUDE };

    private static final VibrationEffect TEST_ONE_SHOT =
            VibrationEffect.createOneShot(TEST_TIMING, TEST_AMPLITUDE);
    private static final VibrationEffect TEST_WAVEFORM =
            VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, -1);
    private static final VibrationEffect TEST_WAVEFORM_NO_AMPLITUDES =
            VibrationEffect.createWaveform(TEST_TIMINGS, -1);
    private static final VibrationEffect TEST_PREBAKED =
            VibrationEffect.get(VibrationEffect.EFFECT_CLICK, true);
    private static final VibrationEffect TEST_COMPOSED =
            VibrationEffect.startComposition()
                    .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                    .compose();


    @Test
    public void testCreateOneShot() {
        VibrationEffect e = VibrationEffect.createOneShot(100, VibrationEffect.DEFAULT_AMPLITUDE);
        assertEquals(100, e.getDuration());
        assertEquals(VibrationEffect.DEFAULT_AMPLITUDE,
                ((VibrationEffect.OneShot)e).getAmplitude());
        e = VibrationEffect.createOneShot(1, 1);
        assertEquals(1, e.getDuration());
        assertEquals(1, ((VibrationEffect.OneShot)e).getAmplitude());
        e = VibrationEffect.createOneShot(1000, 255);
        assertEquals(1000, e.getDuration());
        assertEquals(255, ((VibrationEffect.OneShot)e).getAmplitude());
    }

    @Test
    public void testCreateOneShotFailsBadTiming() {
        try {
            VibrationEffect.createOneShot(0, TEST_AMPLITUDE);
            fail("Invalid timing, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testCreateOneShotFailsBadAmplitude() {
        try {
            VibrationEffect.createOneShot(TEST_TIMING, -2);
            fail("Invalid amplitude, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            VibrationEffect.createOneShot(TEST_TIMING, 0);
            fail("Invalid amplitude, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            VibrationEffect.createOneShot(TEST_TIMING, 256);
            fail("Invalid amplitude, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testOneShotEquals() {
        VibrationEffect otherEffect = VibrationEffect.createOneShot(TEST_TIMING, TEST_AMPLITUDE);
        assertEquals(TEST_ONE_SHOT, otherEffect);
        assertEquals(TEST_ONE_SHOT.hashCode(), otherEffect.hashCode());
    }

    @Test
    public void testOneShotNotEqualsAmplitude() {
        VibrationEffect otherEffect =
                VibrationEffect.createOneShot(TEST_TIMING, TEST_AMPLITUDE - 1);
        assertNotEquals(TEST_ONE_SHOT, otherEffect);
    }

    @Test
    public void testOneShotNotEqualsTiming() {
        VibrationEffect otherEffect =
                VibrationEffect.createOneShot(TEST_TIMING - 1, TEST_AMPLITUDE);
        assertNotEquals(TEST_ONE_SHOT, otherEffect);
    }

    @Test
    public void testOneShotEqualsWithDefaultAmplitude() {
        VibrationEffect effect =
                VibrationEffect.createOneShot(TEST_TIMING, VibrationEffect.DEFAULT_AMPLITUDE);
        VibrationEffect otherEffect =
                VibrationEffect.createOneShot(TEST_TIMING, VibrationEffect.DEFAULT_AMPLITUDE);
        assertEquals(effect, otherEffect);
        assertEquals(effect.hashCode(), otherEffect.hashCode());
    }

    @Test
    public void testCreatePrebaked() {
        int[] ids = { VibrationEffect.EFFECT_CLICK, VibrationEffect.EFFECT_DOUBLE_CLICK,
                VibrationEffect.EFFECT_TICK, VibrationEffect.EFFECT_THUD,
                VibrationEffect.EFFECT_POP, VibrationEffect.EFFECT_HEAVY_CLICK,
                VibrationEffect.EFFECT_TEXTURE_TICK };
        boolean[] fallbacks = { false, true };
        for (int id : ids) {
            for (boolean fallback : fallbacks) {
                VibrationEffect.Prebaked effect = (VibrationEffect.Prebaked)
                        VibrationEffect.get(id, fallback);
                assertEquals(id, effect.getId());
                assertEquals(fallback, effect.shouldFallback());
                assertEquals(-1, effect.getDuration());
            }
        }
    }

    @Test
    public void testPrebakedEquals() {
        VibrationEffect otherEffect = VibrationEffect.get(VibrationEffect.EFFECT_CLICK, true);
        assertEquals(TEST_PREBAKED, otherEffect);
        assertEquals(TEST_PREBAKED.hashCode(), otherEffect.hashCode());
    }

    @Test
    public void testCreatePredefined() {
        VibrationEffect expectedEffect = VibrationEffect.get(
                VibrationEffect.EFFECT_DOUBLE_CLICK, true);
        VibrationEffect predefinedEffect = VibrationEffect.createPredefined(
                VibrationEffect.EFFECT_DOUBLE_CLICK);
        assertEquals(expectedEffect, predefinedEffect);
        assertEquals(expectedEffect.hashCode(), predefinedEffect.hashCode());
    }

    @Test
    public void testCreateWaveform() {
        VibrationEffect.Waveform effect = (VibrationEffect.Waveform)
                VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, -1);
        assertArrayEquals(TEST_TIMINGS, effect.getTimings());
        assertArrayEquals(TEST_AMPLITUDES, effect.getAmplitudes());
        assertEquals(-1, effect.getRepeatIndex());
        assertEquals(400, effect.getDuration());
        effect = (VibrationEffect.Waveform)
            VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, 0);
        assertArrayEquals(TEST_TIMINGS, effect.getTimings());
        assertArrayEquals(TEST_AMPLITUDES, effect.getAmplitudes());
        assertEquals(0, effect.getRepeatIndex());
        assertEquals(Long.MAX_VALUE, effect.getDuration());
        effect = (VibrationEffect.Waveform)VibrationEffect.createWaveform(TEST_TIMINGS,
                TEST_AMPLITUDES, TEST_AMPLITUDES.length - 1);
        assertArrayEquals(TEST_TIMINGS, effect.getTimings());
        assertArrayEquals(TEST_AMPLITUDES, effect.getAmplitudes());
        assertEquals(TEST_AMPLITUDES.length - 1, effect.getRepeatIndex());
        assertEquals(Long.MAX_VALUE, effect.getDuration());
    }

    @Test
    public void testCreateWaveformFailsDifferentArraySize() {
        try {
            VibrationEffect.createWaveform(
                    Arrays.copyOfRange(TEST_TIMINGS, 0, TEST_TIMINGS.length - 1),
                    TEST_AMPLITUDES, -1);
            fail("Timing and amplitudes arrays are different sizes, " +
                    "should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            VibrationEffect.createWaveform(
                    TEST_TIMINGS,
                    Arrays.copyOfRange(TEST_AMPLITUDES, 0, TEST_AMPLITUDES.length - 1), -1);
            fail("Timing and amplitudes arrays are different sizes, " +
                    "should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testCreateWaveformFailsRepeatIndexOutOfBounds() {
        try {
            VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, -2);
            fail("Repeat index is < -1, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, TEST_AMPLITUDES.length);
            fail("Repeat index is >= array length, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testCreateWaveformFailsBadTimingValues() {
        try {
            final long[] badTimings = Arrays.copyOf(TEST_TIMINGS, TEST_TIMINGS.length);
            badTimings[1] = -1;
            VibrationEffect.createWaveform(badTimings,TEST_AMPLITUDES, -1);
            fail("Has a timing < 0, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            final long[] badTimings = new long[TEST_TIMINGS.length];
            VibrationEffect.createWaveform(badTimings, TEST_AMPLITUDES, -1);
            fail("Has no non-zero timings, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testCreateWaveformFailsBadAmplitudeValues() {
        try {
            final int[] badAmplitudes = new int[TEST_TIMINGS.length];
            badAmplitudes[1] = -2;
            VibrationEffect.createWaveform(TEST_TIMINGS, badAmplitudes, -1);
            fail("Has an amplitude < VibrationEffect.DEFAULT_AMPLITUDE, " +
                    "should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            final int[] badAmplitudes = new int[TEST_TIMINGS.length];
            badAmplitudes[1] = 256;
            VibrationEffect.createWaveform(TEST_TIMINGS, badAmplitudes, -1);
            fail("Has an amplitude > 255, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testCreateWaveformWithNoAmplitudes() {
        VibrationEffect.createWaveform(TEST_TIMINGS, -1);
        VibrationEffect.createWaveform(TEST_TIMINGS, 0);
        VibrationEffect.createWaveform(TEST_TIMINGS, TEST_TIMINGS.length - 1);
    }

    @Test
    public void testCreateWaveformWithNoAmplitudesFailsRepeatIndexOutOfBounds() {
        try {
            VibrationEffect.createWaveform(TEST_TIMINGS, -2);
            fail("Repeat index is < -1, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }

        try {
            VibrationEffect.createWaveform(TEST_TIMINGS, TEST_TIMINGS.length);
            fail("Repeat index is >= timings array length, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) { }
    }

    @Test
    public void testWaveformEquals() {
        VibrationEffect effect = VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, -1);
        VibrationEffect otherEffect =
                VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, -1);
        assertEquals(effect, otherEffect);
        assertEquals(effect.hashCode(), otherEffect.hashCode());
    }

    @Test
    public void testWaveformNotEqualsDifferentRepeatIndex() {
        VibrationEffect otherEffect =
                VibrationEffect.createWaveform(TEST_TIMINGS, TEST_AMPLITUDES, 0);
        assertNotEquals(TEST_WAVEFORM, otherEffect);
    }

    @Test
    public void testWaveformNotEqualsDifferentTimingArrayValue() {
        long[] newTimings = Arrays.copyOf(TEST_TIMINGS, TEST_TIMINGS.length);
        newTimings[0] = 200;
        VibrationEffect otherEffect =
                VibrationEffect.createWaveform(newTimings, TEST_AMPLITUDES, -1);
        assertNotEquals(TEST_WAVEFORM, otherEffect);
    }

    @Test
    public void testWaveformNotEqualsDifferentAmplitudeArrayValue() {
        int[] newAmplitudes = Arrays.copyOf(TEST_AMPLITUDES, TEST_AMPLITUDES.length);
        newAmplitudes[0] = 1;
        VibrationEffect otherEffect =
                VibrationEffect.createWaveform(TEST_TIMINGS, newAmplitudes, -1);
        assertNotEquals(TEST_WAVEFORM, otherEffect);
    }

    @Test
    public void testWaveformNotEqualsDifferentArrayLength() {
        long[] newTimings = Arrays.copyOfRange(TEST_TIMINGS, 0, TEST_TIMINGS.length - 1);
        int[] newAmplitudes = Arrays.copyOfRange(TEST_AMPLITUDES, 0, TEST_AMPLITUDES.length -1);
        VibrationEffect otherEffect =
                VibrationEffect.createWaveform(newTimings, newAmplitudes, -1);
        assertNotEquals(TEST_WAVEFORM, otherEffect);
        assertNotEquals(otherEffect, TEST_WAVEFORM);
    }

    @Test
    public void testWaveformWithNoAmplitudesEquals() {
        VibrationEffect otherEffect = VibrationEffect.createWaveform(TEST_TIMINGS, -1);
        assertEquals(TEST_WAVEFORM_NO_AMPLITUDES, otherEffect);
        assertEquals(TEST_WAVEFORM_NO_AMPLITUDES.hashCode(), otherEffect.hashCode());
    }

    @Test
    public void testWaveformWithNoAmplitudesNotEqualsDifferentRepeatIndex() {
        VibrationEffect otherEffect = VibrationEffect.createWaveform(TEST_TIMINGS, 0);
        assertNotEquals(TEST_WAVEFORM_NO_AMPLITUDES, otherEffect);
    }

    @Test
    public void testWaveformWithNoAmplitudesNotEqualsDifferentArrayLength() {
        long[] newTimings = Arrays.copyOfRange(TEST_TIMINGS, 0, TEST_TIMINGS.length - 1);
        VibrationEffect otherEffect = VibrationEffect.createWaveform(newTimings, -1);
        assertNotEquals(TEST_WAVEFORM_NO_AMPLITUDES, otherEffect);
    }

    @Test
    public void testWaveformWithNoAmplitudesNotEqualsDifferentTimingValue() {
        long[] newTimings = Arrays.copyOf(TEST_TIMINGS, TEST_TIMINGS.length);
        newTimings[0] = 1;
        VibrationEffect otherEffect = VibrationEffect.createWaveform(newTimings, -1);
        assertNotEquals(TEST_WAVEFORM_NO_AMPLITUDES, otherEffect);
    }

    @Test
    public void testParcelingOneShot() {
        Parcel p = Parcel.obtain();
        TEST_ONE_SHOT.writeToParcel(p, 0);
        p.setDataPosition(0);
        VibrationEffect parceledEffect = VibrationEffect.CREATOR.createFromParcel(p);
        assertEquals(TEST_ONE_SHOT, parceledEffect);
    }

    @Test
    public void testParcelingWaveForm() {
        Parcel p = Parcel.obtain();
        TEST_WAVEFORM.writeToParcel(p, 0);
        p.setDataPosition(0);
        VibrationEffect parceledEffect = VibrationEffect.CREATOR.createFromParcel(p);
        assertEquals(TEST_WAVEFORM, parceledEffect);
    }

    @Test
    public void testParcelingPrebaked() {
        Parcel p = Parcel.obtain();
        TEST_PREBAKED.writeToParcel(p, 0);
        p.setDataPosition(0);
        VibrationEffect parceledEffect = VibrationEffect.CREATOR.createFromParcel(p);
        assertEquals(TEST_PREBAKED, parceledEffect);
    }

    @Test
    public void testDescribeContents() {
        TEST_ONE_SHOT.describeContents();
        TEST_WAVEFORM.describeContents();
        TEST_WAVEFORM_NO_AMPLITUDES.describeContents();
        TEST_PREBAKED.describeContents();
        TEST_COMPOSED.describeContents();
    }

    @Test
    public void testSetStrength() {
        VibrationEffect.Prebaked effect = (VibrationEffect.Prebaked)VibrationEffect.get(
                VibrationEffect.EFFECT_CLICK, true);
        int[] strengths = {
                VibrationEffect.EFFECT_STRENGTH_LIGHT,
                VibrationEffect.EFFECT_STRENGTH_MEDIUM,
                VibrationEffect.EFFECT_STRENGTH_STRONG
        };
        for (int strength : strengths) {
            effect.setEffectStrength(strength);
            assertEquals(strength, effect.getEffectStrength());
        }
    }

    @Test
    public void testSetStrengthInvalid() {
        VibrationEffect.Prebaked effect = (VibrationEffect.Prebaked)VibrationEffect.get(
                VibrationEffect.EFFECT_CLICK, true);
        try {
            effect.setEffectStrength(239017);
            fail("Illegal strength, should throw IllegalArgumentException");
        } catch (IllegalArgumentException expected) {}
    }

    @Test
    public void testStartComposition() {
        VibrationEffect.Composition first = VibrationEffect.startComposition();
        VibrationEffect.Composition other = VibrationEffect.startComposition();
        assertNotEquals(first, other);
    }

    @Test
    public void testComposedEquals() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_QUICK_FALL)
                .compose();
        VibrationEffect otherEffect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK, 1f)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_QUICK_FALL, 1f, 0)
                .compose();
        assertEquals(effect, otherEffect);
        assertEquals(effect.hashCode(), otherEffect.hashCode());
    }

    @Test
    public void testComposedDifferentPrimitivesNotEquals() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK)
                .compose();
        VibrationEffect otherEffect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .compose();
        assertNotEquals(effect, otherEffect);
    }

    @Test
    public void testComposedDifferentScaleNotEquals() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK, 0.4f)
                .compose();
        VibrationEffect otherEffect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK, 0.5f)
                .compose();
        assertNotEquals(effect, otherEffect);
    }

    @Test
    public void testComposedDifferentDelayNotEquals() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK, 0.8f, 10)
                .compose();
        VibrationEffect otherEffect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK, 0.8f, 100)
                .compose();
        assertNotEquals(effect, otherEffect);
    }

    @Test
    public void testComposedDifferentOrderNotEquals() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK)
                .compose();
        VibrationEffect otherEffect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .compose();
        assertNotEquals(effect, otherEffect);
    }

    @Test
    public void testComposedDifferentNumberOfPrimitivesNotEquals() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .compose();
        VibrationEffect otherEffect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK)
                .compose();
        assertNotEquals(effect, otherEffect);
    }

    @Test
    public void testComposedDuration() {
        VibrationEffect effect = VibrationEffect.startComposition()
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_CLICK, 0.5f, 1000)
                .addPrimitive(VibrationEffect.Composition.PRIMITIVE_TICK)
                .compose();
        assertEquals(-1, effect.getDuration());
    }

    @Test
    public void testComposeEmptyCompositionIsInvalid() {
        try {
            VibrationEffect.startComposition().compose();
            fail("Illegal composition, should throw IllegalStateException");
        } catch (IllegalStateException expected) {}
    }

    @Test
    public void testToString() {
        TEST_ONE_SHOT.toString();
        TEST_WAVEFORM.toString();
        TEST_PREBAKED.toString();
        TEST_COMPOSED.toString();
    }
}
