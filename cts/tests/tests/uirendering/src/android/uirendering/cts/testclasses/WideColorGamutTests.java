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

package android.uirendering.cts.testclasses;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BlendMode;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.Point;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapverifiers.BitmapVerifier;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.bitmapverifiers.SamplePointWideGamutVerifier;
import android.uirendering.cts.testclasses.view.BitmapView;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class WideColorGamutTests extends ActivityTestBase {
    private static final ColorSpace DISPLAY_P3 = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
    private static final ColorSpace SCRGB = ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB);

    private static final Point[] POINTS = {
            new Point(16, 16),
            new Point(48, 16),
            new Point(80, 16),
    };

    // The colors are defined as found in wide-gamut-test.png, which is Display P3
    // Since the UI toolkit renders in scRGB, we want to convert here to compare values
    // directly in the sample point verifier
    private static final Color[] COLORS = {
            Color.valueOf(0.937f, 0.000f, 0.000f, 1.0f, DISPLAY_P3).convert(SCRGB),
            Color.valueOf(1.000f, 0.000f, 0.000f, 1.0f, DISPLAY_P3).convert(SCRGB),
            Color.valueOf(0.918f, 0.200f, 0.137f, 1.0f, DISPLAY_P3).convert(SCRGB)
    };

    private Bitmap mBitmap;

    @Override
    protected boolean isWideColorGamut() {
        return true;
    }

    @Before
    public void loadBitmap() {
        try (InputStream in = getActivity().getAssets().open("wide-gamut-test.png")) {
            mBitmap = BitmapFactory.decodeStream(in);
        } catch (IOException e) {
            Assert.fail("Could not load wide-gamut-test.png");
        }
    }

    @SuppressWarnings("SameParameterValue")
    private BitmapVerifier getVerifier(Point[] points, Color[] colors, float eps) {
        if (getActivity().getWindow().isWideColorGamut()) {
            return new SamplePointWideGamutVerifier(points, colors, eps);
        }
        return new SamplePointVerifier(points,
                Arrays.stream(colors).mapToInt(Color::toArgb).toArray(),
                (int) (eps * 255.0f + 0.5f));
    }

    @Test
    public void testDraw() {
        createTest()
                .addLayout(R.layout.wide_gamut_bitmap_layout, view -> {
                    BitmapView bv = (BitmapView) view;
                    bv.setBitmap(mBitmap);
                }, true)
                .runWithVerifier(getVerifier(POINTS, COLORS, 1e-2f));
    }

    @Test
    public void testSaveLayer() {
        createTest()
                .addLayout(R.layout.wide_gamut_bitmap_layout, view -> {
                    BitmapView bv = (BitmapView) view;
                    bv.setBitmap(mBitmap);
                    bv.setSaveLayer(true);
                }, true)
                .runWithVerifier(getVerifier(POINTS, COLORS, 1e-2f));
    }

    @Test
    public void testHardwareLayer() {
        createTest()
                .addLayout(R.layout.wide_gamut_bitmap_layout, view -> {
                    BitmapView bv = (BitmapView) view;
                    bv.setBitmap(mBitmap);
                    bv.setLayerType(View.LAYER_TYPE_HARDWARE, null);
                }, true)
                .runWithVerifier(getVerifier(POINTS, COLORS, 1e-2f));
    }

    @Test
    public void testSaveLayerInHardwareLayer() {
        createTest()
                .addLayout(R.layout.wide_gamut_bitmap_layout, view -> {
                    BitmapView bv = (BitmapView) view;
                    bv.setBitmap(mBitmap);
                    bv.setSaveLayer(true);
                    bv.setLayerType(View.LAYER_TYPE_HARDWARE, null);
                }, true)
                .runWithVerifier(getVerifier(POINTS, COLORS, 1e-2f));
    }

    @Test
    public void testCanvasDrawColorLong() {
        final Color greenP3 = Color.valueOf(0, 1.0f, 0, 1.0f, DISPLAY_P3);
        createTest()
                .addCanvasClient((canvas, width, height) -> {
                    canvas.drawColor(greenP3.pack());
                }, true)
                .runWithVerifier(getVerifier(
                            new Point[] { new Point(0, 0), new Point(50, 50) },
                            new Color[] { greenP3, greenP3 },
                            .002f));
    }

    private static Color plus(Color a, Color b) {
        final ColorSpace cs = a.getColorSpace();
        Assert.assertSame(cs, b.getColorSpace());

        float[] ac = a.getComponents();
        float[] bc = b.getComponents();
        float[] result = new float[ac.length];
        for (int i = 0; i < ac.length; ++i) {
            // BlendMode.PLUS clamps to [0,1]
            result[i] = Math.max(Math.min(ac[i] + bc[i], 1.0f), 0.0f);
        }
        return Color.valueOf(result, cs);
    }

    @Test
    public void testCanvasDrawColorLongBlendMode() {
        final Color greenP3 = Color.valueOf(0, 1.0f, 0, 1.0f, DISPLAY_P3);
        final Color redP3 = Color.valueOf(1.0f, 0, 0, 1.0f, DISPLAY_P3);

        final ColorSpace displaySpace = displaySpace();
        final Color greenDisplay = greenP3.convert(displaySpace);
        final Color redDisplay = redP3.convert(displaySpace);

        final Color expected = plus(greenDisplay, redDisplay);
        createTest()
                .addCanvasClient((canvas, width, height) -> {
                    canvas.drawColor(greenP3.pack());
                    canvas.drawColor(redP3.pack(), BlendMode.PLUS);
                }, true)
                .runWithVerifier(getVerifier(
                            new Point[] { new Point(0, 0), new Point(50, 50) },
                            new Color[] { expected, expected },
                            .002f));
    }

    private ColorSpace displaySpace() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        WindowManager window = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        Display defaultDisplay = window.getDefaultDisplay();
        ColorSpace displaySpace = defaultDisplay.getPreferredWideGamutColorSpace();
        return displaySpace == null ? ColorSpace.get(ColorSpace.Named.SRGB) : displaySpace;
    }

    @Test
    public void testProPhoto() {
        Color blueProPhoto = Color.valueOf(0, 0, 1, 1,
                ColorSpace.get(ColorSpace.Named.PRO_PHOTO_RGB));
        final Color blueDisplay = blueProPhoto.convert(displaySpace());
        createTest()
                .addCanvasClient("RGBA16F_ProPhoto", (canvas, width, height) -> {
                    AssetManager assets = getActivity().getResources().getAssets();
                    try (InputStream in = assets.open("blue-16bit-prophoto.png")) {
                        Bitmap bitmap = BitmapFactory.decodeStream(in);
                        canvas.scale(
                                width / (float) bitmap.getWidth(),
                                height / (float) bitmap.getHeight());
                        canvas.drawBitmap(bitmap, 0, 0, null);
                    } catch (IOException e) {
                        throw new RuntimeException("Test failed: ", e);
                    }
                }, true)
                .runWithVerifier(getVerifier(
                        new Point[] { new Point(0, 0) },
                        new Color[] { blueDisplay },
                0.6f));
    }
}
