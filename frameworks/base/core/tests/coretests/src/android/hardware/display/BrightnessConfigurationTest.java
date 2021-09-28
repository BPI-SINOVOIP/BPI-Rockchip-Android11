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

package android.hardware.display;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Parcel;
import android.util.Pair;
import android.util.Xml;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.internal.util.FastXmlSerializer;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlSerializer;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class BrightnessConfigurationTest {
    private static final float[] LUX_LEVELS = {
        0f,
        10f,
        100f,
    };

    private static final float[] NITS_LEVELS = {
        1f,
        90f,
        100f,
    };

    @Test
    public void testSetCurveIsUnmodified() {
        BrightnessConfiguration.Builder builder = new BrightnessConfiguration.Builder(
                LUX_LEVELS, NITS_LEVELS);
        BrightnessConfiguration config = builder.build();
        Pair<float[], float[]> curve = config.getCurve();
        assertArrayEquals(LUX_LEVELS, curve.first, "lux");
        assertArrayEquals(NITS_LEVELS, curve.second, "nits");
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCurveMustHaveZeroLuxPoint() {
        float[] lux = Arrays.copyOf(LUX_LEVELS, LUX_LEVELS.length);
        lux[0] = 1f;
        new BrightnessConfiguration.Builder(lux, NITS_LEVELS);
    }

    @Test(expected = NullPointerException.class)
    public void testCurveMustNotHaveNullArrays() {
        new BrightnessConfiguration.Builder(null, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCurveMustNotHaveEmptyArrays() {
        new BrightnessConfiguration.Builder(new float[0], new float[0]);
    }

    @Test
    public void testCurveMustNotHaveArraysOfDifferentLengths() {
        assertThrows(IllegalArgumentException.class, () -> {
            float[] lux = Arrays.copyOf(LUX_LEVELS, LUX_LEVELS.length + 1);
            lux[lux.length - 1] = lux[lux.length - 2] + 1;
            new BrightnessConfiguration.Builder(lux, NITS_LEVELS);
        });

        assertThrows(IllegalArgumentException.class, () -> {
            float[] nits = Arrays.copyOf(NITS_LEVELS, NITS_LEVELS.length + 1);
            nits[nits.length - 1] = nits[nits.length - 2] + 1;
            new BrightnessConfiguration.Builder(LUX_LEVELS, nits);
        });
    }

    @Test
    public void testCurvesMustNotContainNaN() {
        assertThrows(IllegalArgumentException.class, () -> {
            float[] lux = Arrays.copyOf(LUX_LEVELS, LUX_LEVELS.length);
            lux[lux.length - 1] = Float.NaN;
            new BrightnessConfiguration.Builder(lux, NITS_LEVELS);
        });

        assertThrows(IllegalArgumentException.class, () -> {
            float[] nits = Arrays.copyOf(NITS_LEVELS, NITS_LEVELS.length);
            nits[nits.length - 1] = Float.NaN;
            new BrightnessConfiguration.Builder(LUX_LEVELS, nits);
        });
    }

    @Test
    public void testLuxMultipliersMustBePositive() {
        BrightnessConfiguration.Builder config = new BrightnessConfiguration.Builder(
                LUX_LEVELS, NITS_LEVELS);
        assertThrows(IllegalArgumentException.class, () -> {
            config.setShortTermModelUpperLuxMultiplier(-1f);
        });

        assertThrows(IllegalArgumentException.class, () -> {
            config.setShortTermModelLowerLuxMultiplier(-1f);
        });
    }

    @Test
    public void testParceledConfigIsEquivalent() {
        BrightnessConfiguration.Builder builder =
                new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShouldCollectColorSamples(true);
        builder.setShortTermModelTimeoutMillis(1234L);
        builder.setShortTermModelLowerLuxMultiplier(0.9f);
        builder.setShortTermModelUpperLuxMultiplier(0.2f);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration config = builder.build();
        Parcel p = Parcel.obtain();
        p.writeParcelable(config, 0 /*flags*/);
        p.setDataPosition(0);
        BrightnessConfiguration newConfig =
                p.readParcelable(BrightnessConfiguration.class.getClassLoader());
        assertEquals(config, newConfig);
    }

    @Test
    public void testWriteReadXml() throws IOException, XmlPullParserException {
        BrightnessConfiguration.Builder builder =
                new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShouldCollectColorSamples(true);
        builder.setShortTermModelTimeoutMillis(123L);
        builder.setShortTermModelLowerLuxMultiplier(0.4f);
        builder.setShortTermModelUpperLuxMultiplier(0.8f);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration config = builder.build();

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        XmlSerializer out = new FastXmlSerializer();
        out.setOutput(baos, StandardCharsets.UTF_8.name());
        out.startDocument(null, true);
        config.saveToXml(out);
        out.endDocument();
        baos.flush();

        ByteArrayInputStream input = new ByteArrayInputStream(baos.toByteArray());
        XmlPullParser parser = Xml.newPullParser();
        parser.setInput(input, StandardCharsets.UTF_8.name());
        BrightnessConfiguration loadedConfig = BrightnessConfiguration.loadFromXml(parser);

        assertEquals(config, loadedConfig);
    }

    @Test
    public void testEquals() {
        BrightnessConfiguration.Builder builder =
                new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShouldCollectColorSamples(true);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration baseConfig = builder.build();

        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShouldCollectColorSamples(true);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration identicalConfig = builder.build();
        assertEquals(baseConfig, identicalConfig);
        assertEquals("hashCodes must be equal for identical configs",
                baseConfig.hashCode(), identicalConfig.hashCode());

        float[] lux = Arrays.copyOf(LUX_LEVELS, LUX_LEVELS.length);
        lux[lux.length - 1] = lux[lux.length - 1] * 2;
        builder = new BrightnessConfiguration.Builder(lux, NITS_LEVELS);
        builder.setShouldCollectColorSamples(true);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration luxDifferConfig = builder.build();
        assertNotEquals(baseConfig, luxDifferConfig);

        float[] nits = Arrays.copyOf(NITS_LEVELS, NITS_LEVELS.length);
        nits[nits.length - 1] = nits[nits.length - 1] * 2;
        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, nits);
        builder.setShouldCollectColorSamples(true);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration nitsDifferConfig = builder.build();
        assertNotEquals(baseConfig, nitsDifferConfig);

        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.addCorrectionByCategory(3,
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        builder.addCorrectionByPackageName("a.package.name",
                BrightnessCorrection.createScaleAndTranslateLog(1.0f, 2.0f));
        BrightnessConfiguration correctionsDiffer = builder.build();
        assertNotEquals(baseConfig, correctionsDiffer);

        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShouldCollectColorSamples(true);
        BrightnessConfiguration colorCollectionDiffers = builder.build();
        assertNotEquals(baseConfig, colorCollectionDiffers);

        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShortTermModelTimeoutMillis(300L);
        BrightnessConfiguration timeoutDiffers = builder.build();
        assertNotEquals(baseConfig, timeoutDiffers);

        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShortTermModelLowerLuxMultiplier(0.7f);
        BrightnessConfiguration lowerLuxDiffers = builder.build();
        assertNotEquals(baseConfig, lowerLuxDiffers);

        builder = new BrightnessConfiguration.Builder(LUX_LEVELS, NITS_LEVELS);
        builder.setShortTermModelUpperLuxMultiplier(0.6f);
        BrightnessConfiguration upperLuxDiffers = builder.build();
        assertNotEquals(baseConfig, upperLuxDiffers);
    }

    private static void assertArrayEquals(float[] expected, float[] actual, String name) {
        assertEquals("Expected " + name + " arrays to be the same length!",
                expected.length, actual.length);
        for (int i = 0; i < expected.length; i++) {
            assertEquals("Expected " + name + " arrays to be equivalent when value " + i
                    + "differs", expected[i], actual[i], 0.01 /*tolerance*/);
        }
    }

    private interface ExceptionRunnable {
        void run() throws Exception;
    }

    private static void assertThrows(Class<? extends Throwable> exceptionClass,
            ExceptionRunnable r) {
        try {
            r.run();
        } catch (Throwable e) {
            assertTrue("Expected exception type " + exceptionClass.getName() + " but got "
                    + e.getClass().getName(), exceptionClass.isAssignableFrom(e.getClass()));
            return;
        }
        fail("Expected exception type " + exceptionClass.getName()
                + ", but no exception was thrown");
    }
}
