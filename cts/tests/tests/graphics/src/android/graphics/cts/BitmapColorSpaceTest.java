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

package android.graphics.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.ImageDecoder;
import android.graphics.Matrix;
import android.os.Parcel;
import android.util.Log;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.RequiresDevice;
import androidx.test.filters.SmallTest;

import com.android.compatibility.common.util.ColorUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Arrays;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

@SmallTest
@RunWith(JUnitParamsRunner.class)
public class BitmapColorSpaceTest {
    private static final String LOG_TAG = "BitmapColorSpaceTest";

    private Resources mResources;

    @Before
    public void setup() {
        mResources = InstrumentationRegistry.getTargetContext().getResources();
    }

    @SuppressWarnings("deprecation")
    @Test
    public void createWithColorSpace() {
        // We don't test HARDWARE configs because they are not compatible with mutable bitmaps

        Bitmap.Config[] configs = new Bitmap.Config[] {
                Bitmap.Config.ARGB_8888,
                Bitmap.Config.RGB_565,
                Bitmap.Config.ARGB_4444,
                Bitmap.Config.RGBA_F16,
        };
        // in most cases, createBitmap respects the ColorSpace
        for (Bitmap.Config config : configs) {
            for (ColorSpace.Named e : new ColorSpace.Named[] {
                    ColorSpace.Named.PRO_PHOTO_RGB,
                    ColorSpace.Named.ADOBE_RGB,
                    ColorSpace.Named.DISPLAY_P3,
                    ColorSpace.Named.DCI_P3,
                    ColorSpace.Named.BT709,
                    ColorSpace.Named.BT2020,
            }) {
                ColorSpace requested = ColorSpace.get(e);
                Bitmap b = Bitmap.createBitmap(32, 32, config, false, requested);
                ColorSpace cs = b.getColorSpace();
                assertNotNull(cs);
                assertSame(requested, cs);
            }

            // SRGB and LINEAR_SRGB are special.
            ColorSpace sRGB = ColorSpace.get(ColorSpace.Named.SRGB);
            ColorSpace extendedSrgb = ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB);
            for (ColorSpace requested : new ColorSpace[] {
                    sRGB,
                    extendedSrgb,
            }) {
                Bitmap b = Bitmap.createBitmap(32, 32, config, false, requested);
                ColorSpace cs = b.getColorSpace();
                assertNotNull(cs);
                if (config == Bitmap.Config.RGBA_F16) {
                    assertSame(extendedSrgb, cs);
                } else {
                    assertSame(sRGB, cs);
                }
            }

            ColorSpace linearRgb = ColorSpace.get(ColorSpace.Named.LINEAR_SRGB);
            ColorSpace linearExtendedSrgb = ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB);
            for (ColorSpace requested : new ColorSpace[] {
                    linearRgb,
                    linearExtendedSrgb,
            }) {
                Bitmap b = Bitmap.createBitmap(32, 32, config, false, requested);
                ColorSpace cs = b.getColorSpace();
                assertNotNull(cs);
                if (config == Bitmap.Config.RGBA_F16) {
                    assertSame(linearExtendedSrgb, cs);
                } else {
                    assertSame(linearRgb, cs);
                }
            }
        }
    }

    @Test
    public void createAlpha8ColorSpace() {
        Bitmap bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ALPHA_8);
        assertNull(bitmap.getColorSpace());

        for (ColorSpace cs : BitmapTest.getRgbColorSpaces()) {
            bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ALPHA_8, true, cs);
            assertNull(bitmap.getColorSpace());
        }
    }

    @Test
    public void createDefaultColorSpace() {
        ColorSpace sRGB = ColorSpace.get(ColorSpace.Named.SRGB);
        Bitmap.Config[] configs = new Bitmap.Config[] {
                Bitmap.Config.RGB_565, Bitmap.Config.ARGB_8888
        };
        for (Bitmap.Config config : configs) {
            Bitmap bitmap = Bitmap.createBitmap(32, 32, config, true);
            assertSame(sRGB, bitmap.getColorSpace());
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void createWithoutColorSpace() {
        Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888, true, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void createWithNonRgbColorSpace() {
        Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888, true,
                ColorSpace.get(ColorSpace.Named.CIE_LAB));
    }

    @Test(expected = IllegalArgumentException.class)
    public void createWithNoTransferParameters() {
        Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888, true,
                new ColorSpace.Rgb("NoTransferParams",
                    new float[]{ 0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f },
                    ColorSpace.ILLUMINANT_D50,
                    x -> Math.pow(x, 1.0f / 2.2f), x -> Math.pow(x, 2.2f),
                    0, 1));
    }

    @Test
    public void createFromSourceWithColorSpace() {
        for (ColorSpace rgb : BitmapTest.getRgbColorSpaces()) {
            Bitmap.Config[] configs = new Bitmap.Config[] {
                    Bitmap.Config.ARGB_8888,
                    Bitmap.Config.RGB_565,
                    Bitmap.Config.ALPHA_8,
                    Bitmap.Config.ARGB_4444,
                    Bitmap.Config.RGBA_F16,
            };
            for (Bitmap.Config config : configs) {
                Bitmap orig = Bitmap.createBitmap(32, 32, config, false, rgb);
                Bitmap cropped = Bitmap.createBitmap(orig, 0, 0, orig.getWidth() / 2,
                        orig.getHeight() / 2, null, false);
                assertSame(orig.getColorSpace(), cropped.getColorSpace());
                if (config == Bitmap.Config.ALPHA_8) {
                    assertNull(cropped.getColorSpace());
                }

                Matrix m = new Matrix();
                m.setRotate(45, orig.getWidth() / 2, orig.getHeight() / 2);
                Bitmap rotated = Bitmap.createBitmap(orig, 0, 0, orig.getWidth(),
                        orig.getHeight(), m, false);
                switch (config) {
                    case ALPHA_8:
                        assertSame(Bitmap.Config.ARGB_8888, rotated.getConfig());
                        assertSame(ColorSpace.get(ColorSpace.Named.SRGB), rotated.getColorSpace());
                        break;
                    case RGB_565:
                        assertSame(Bitmap.Config.ARGB_8888, rotated.getConfig());
                        // Fallthrough.
                    default:
                        assertSame("Mismatch with Config " + config,
                                orig.getColorSpace(), rotated.getColorSpace());
                        break;
                }
            }
        }
    }

    @Test
    public void sRGB() {
        Bitmap b = BitmapFactory.decodeResource(mResources, R.drawable.robot);
        ColorSpace cs = b.getColorSpace();
        assertNotNull(cs);
        assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);

        b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
        cs = b.getColorSpace();
        assertNotNull(cs);
        assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);

        b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
        cs = b.getColorSpace();
        assertNotNull(cs);
        assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);
    }

    @Test
    public void p3() {
        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            Bitmap b = BitmapFactory.decodeStream(in);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);

            b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
            cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);

            b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
            cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void extendedSRGB() {
        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);

            b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
            cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);

            b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
            cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void linearSRGB() {
        String assetInLinearSRGB = "grayscale-linearSrgb.png";
        try (InputStream in = mResources.getAssets().open(assetInLinearSRGB)) {
            Bitmap b = BitmapFactory.decodeStream(in);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_SRGB), cs);
        } catch (IOException e) {
            fail();
        }

        try (InputStream in = mResources.getAssets().open(assetInLinearSRGB)) {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.RGBA_F16;
            Bitmap b = BitmapFactory.decodeStream(in, null, options);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    private static class Asset {
        public final String name;
        public final ColorSpace colorSpace;
        Asset(String name, ColorSpace.Named e) {
            this.name = name;
            this.colorSpace = ColorSpace.get(e);
        }
    };

    @Test
    public void reconfigure() {
        Asset[] assets = new Asset[] {
                new Asset("green-p3.png", ColorSpace.Named.DISPLAY_P3),
                new Asset("red-adobergb.png", ColorSpace.Named.ADOBE_RGB),
        };
        for (Asset asset : assets) {
            for (Bitmap.Config config : new Bitmap.Config[] {
                    Bitmap.Config.ARGB_8888,
                    Bitmap.Config.RGB_565,
            }) {
                try (InputStream in = mResources.getAssets().open(asset.name)) {
                    BitmapFactory.Options opts = new BitmapFactory.Options();
                    opts.inMutable = true;
                    opts.inPreferredConfig = config;

                    Bitmap b = BitmapFactory.decodeStream(in, null, opts);
                    ColorSpace cs = b.getColorSpace();
                    assertNotNull(cs);
                    assertSame(asset.colorSpace, cs);

                    b.reconfigure(b.getWidth() / 4, b.getHeight() / 4, Bitmap.Config.RGBA_F16);
                    cs = b.getColorSpace();
                    assertNotNull(cs);
                    assertSame(asset.colorSpace, cs);

                    b.reconfigure(b.getWidth(), b.getHeight(), config);
                    cs = b.getColorSpace();
                    assertNotNull(cs);
                    assertSame(asset.colorSpace, cs);
                } catch (IOException e) {
                    fail();
                }
            }
        }
    }

    @Test
    public void reuse() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inMutable = true;

        Bitmap bitmap1 = null;
        try (InputStream in = mResources.getAssets().open("green-srgb.png")) {
            bitmap1 = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = bitmap1.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);
        } catch (IOException e) {
            fail();
        }

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            opts.inBitmap = bitmap1;

            Bitmap bitmap2 = BitmapFactory.decodeStream(in, null, opts);
            assertSame(bitmap1, bitmap2);
            ColorSpace cs = bitmap2.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void getPixel() {
        verifyGetPixel("green-p3.png", 0x75fb4cff);
        verifyGetPixel("translucent-green-p3.png", 0x3a7d267f); // 50% translucent
    }

    private void verifyGetPixel(@NonNull String fileName, @ColorInt int rawColor) {
        try (InputStream in = mResources.getAssets().open(fileName)) {
            Bitmap b = BitmapFactory.decodeStream(in);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);

            verifyGetPixel(b, rawColor);

            b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
            verifyGetPixel(b, rawColor);

            b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
            verifyGetPixel(b, rawColor);
        } catch (IOException e) {
            fail();
        }
    }

    private static void verifyGetPixel(@NonNull Bitmap b, @ColorInt int rawColor) {
        ByteBuffer dst = ByteBuffer.allocate(b.getByteCount());
        b.copyPixelsToBuffer(dst);
        dst.rewind();

        // Stored as RGBA
        assertEquals(rawColor, dst.asIntBuffer().get());

        int srgbColor = convertPremulColorToColorInt(rawColor, b.getColorSpace());
        int srgb = b.getPixel(15, 15);
        almostEqual(srgbColor, srgb, 3, 15 * b.getWidth() + 15);
    }

    private static int convertPremulColorToColorInt(int premulColor, ColorSpace premulCS) {
        float alpha = (premulColor & 0xff) / 255.0f;
        return Color.toArgb(Color.convert((premulColor >>> 24) / 255.0f / alpha,
                ((premulColor >> 16) & 0xff) / 255.0f / alpha,
                ((premulColor >> 8) & 0xff) / 255.0f / alpha,
                alpha, premulCS, ColorSpace.get(ColorSpace.Named.SRGB)));
    }

    @Test
    public void getPixels() {
        verifyGetPixels("green-p3.png");
        verifyGetPixels("translucent-green-p3.png"); // 50% translucent
    }

    private void verifyGetPixels(@NonNull String fileName) {
        try (InputStream in = mResources.getAssets().open(fileName)) {
            Bitmap b = BitmapFactory.decodeStream(in);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);

            ByteBuffer dst = ByteBuffer.allocate(b.getByteCount());
            b.copyPixelsToBuffer(dst);
            dst.rewind();

            // Stored as RGBA
            int expected = convertPremulColorToColorInt(dst.asIntBuffer().get(), b.getColorSpace());

            verifyGetPixels(b, expected);

            b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
            verifyGetPixels(b, expected);

            b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
            verifyGetPixels(b, expected);
        } catch (IOException e) {
            fail();
        }
    }

    private static void verifyGetPixels(@NonNull Bitmap b, @ColorInt int expected) {
        int[] pixels = new int[b.getWidth() * b.getHeight()];
        b.getPixels(pixels, 0, b.getWidth(), 0, 0, b.getWidth(), b.getHeight());

        for (int i = 0; i < pixels.length; i++) {
            int pixel = pixels[i];
            almostEqual(expected, pixel, 3, i);
        }
    }

    @Test
    public void setPixel() {
        verifySetPixel("green-p3.png", 0xffff0000, 0xea3323ff);
        verifySetPixel("translucent-green-p3.png", 0x7fff0000, 0x7519127f);
    }

    private void verifySetPixel(@NonNull String fileName,
            @ColorInt int newColor, @ColorInt int expectedColor) {
        try (InputStream in = mResources.getAssets().open(fileName)) {
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inMutable = true;

            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());
            assertTrue(b.isMutable());
            verifySetPixel(b, newColor, expectedColor);

            b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());
            assertTrue(b.isMutable());
            verifySetPixel(b, newColor, expectedColor);

            b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());
            assertTrue(b.isMutable());
            verifySetPixel(b, newColor, expectedColor);
        } catch (IOException e) {
            fail();
        }
    }

    private static void verifySetPixel(@NonNull Bitmap b,
            @ColorInt int newColor, @ColorInt int expectedColor) {
        assertTrue(b.isMutable());
        b.setPixel(0, 0, newColor);

        ByteBuffer dst = ByteBuffer.allocate(b.getByteCount());
        b.copyPixelsToBuffer(dst);
        dst.rewind();
        // Stored as RGBA
        ColorUtils.verifyColor(expectedColor, dst.asIntBuffer().get(), 1);
    }

    @Test
    public void setPixels() {
        verifySetPixels("green-p3.png", 0xffff0000, 0xea3323ff);
        verifySetPixels("translucent-green-p3.png", 0x7fff0000, 0x7519127f);
    }

    private void verifySetPixels(@NonNull String fileName,
            @ColorInt int newColor, @ColorInt int expectedColor) {
        try (InputStream in = mResources.getAssets().open(fileName)) {
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inMutable = true;

            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            assertNotNull(b.getColorSpace());
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());

            verifySetPixels(b, newColor, expectedColor);

            b = Bitmap.createBitmap(b, 0, 0, b.getWidth() / 2, b.getHeight() / 2);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());
            assertTrue(b.isMutable());
            verifySetPixels(b, newColor, expectedColor);

            b = Bitmap.createScaledBitmap(b, b.getWidth() / 2, b.getHeight() / 2, true);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());
            assertTrue(b.isMutable());
            verifySetPixels(b, newColor, expectedColor);
        } catch (IOException e) {
            fail();
        }
    }

    private static void verifySetPixels(@NonNull Bitmap b,
            @ColorInt int newColor, @ColorInt int expectedColor) {
        assertTrue(b.isMutable());
        int[] pixels = new int[b.getWidth() * b.getHeight()];
        Arrays.fill(pixels, newColor);
        b.setPixels(pixels, 0, b.getWidth(), 0, 0, b.getWidth(), b.getHeight());

        ByteBuffer dst = ByteBuffer.allocate(b.getByteCount());
        b.copyPixelsToBuffer(dst);
        dst.rewind();

        IntBuffer buffer = dst.asIntBuffer();
        //noinspection ForLoopReplaceableByForEach
        for (int i = 0; i < pixels.length; i++) {
            // Stored as RGBA
            ColorUtils.verifyColor(expectedColor, buffer.get(), 1);
        }
    }

    @Test
    public void writeColorSpace() {
        verifyColorSpaceMarshalling("green-srgb.png", ColorSpace.get(ColorSpace.Named.SRGB));
        verifyColorSpaceMarshalling("green-p3.png", ColorSpace.get(ColorSpace.Named.DISPLAY_P3));
        verifyColorSpaceMarshalling("blue-16bit-srgb.png",
                ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB));

        Bitmap bitmapIn = BitmapFactory.decodeResource(mResources, R.drawable.robot);
        verifyParcelUnparcel(bitmapIn, ColorSpace.get(ColorSpace.Named.SRGB));
    }

    private void verifyColorSpaceMarshalling(
            @NonNull String fileName, @NonNull ColorSpace colorSpace) {
        try (InputStream in = mResources.getAssets().open(fileName)) {
            Bitmap bitmapIn = BitmapFactory.decodeStream(in);
            verifyParcelUnparcel(bitmapIn, colorSpace);
        } catch (IOException e) {
            fail();
        }
    }

    private void verifyParcelUnparcel(Bitmap bitmapIn, ColorSpace expected) {
        ColorSpace cs = bitmapIn.getColorSpace();
        assertNotNull(cs);
        assertSame(expected, cs);

        Parcel p = Parcel.obtain();
        bitmapIn.writeToParcel(p, 0);
        p.setDataPosition(0);

        Bitmap bitmapOut = Bitmap.CREATOR.createFromParcel(p);
        cs = bitmapOut.getColorSpace();
        assertNotNull(cs);
        assertSame(expected, cs);

        p.recycle();
    }

    @Test
    public void p3rgb565() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredConfig = Bitmap.Config.RGB_565;

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void p3hardware() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredConfig = Bitmap.Config.HARDWARE;

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guessSRGB() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("green-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guess16bitUntagged() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guessProPhotoRGB() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("blue-16bit-prophoto.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.PRO_PHOTO_RGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guessP3() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guessAdobeRGB() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("red-adobergb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.ADOBE_RGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guessUnknown() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("purple-displayprofile.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertEquals("Unknown", cs.getName());
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void guessCMYK() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;

        try (InputStream in = mResources.getAssets().open("purple-cmyk.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = opts.outColorSpace;
            assertNull(b);
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpaceP3ToSRGB() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.SRGB);

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);
            assertEquals(opts.inPreferredColorSpace, opts.outColorSpace);

            verifyGetPixel(b, 0x2ff00ff);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpaceSRGBToP3() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);

        try (InputStream in = mResources.getAssets().open("green-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
            assertEquals(opts.inPreferredColorSpace, opts.outColorSpace);

            verifyGetPixel(b, 0x75fb4cff);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpaceWith16BitSrc() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.ADOBE_RGB);

        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.ADOBE_RGB), cs);
            assertSame(opts.inPreferredColorSpace, opts.outColorSpace);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpaceWith16BitDst() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredConfig = Bitmap.Config.RGBA_F16;

        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpaceWith16BitSrcAndDst() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.ADOBE_RGB);
        opts.inPreferredConfig = Bitmap.Config.RGBA_F16;

        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(opts.inPreferredColorSpace, cs);
            assertSame(opts.inPreferredColorSpace, opts.outColorSpace);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpaceWith16BitWithDecreasedGamut() {
        final String asset = "blue-16bit-prophoto.png";
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;
        try (InputStream in = mResources.getAssets().open(asset)) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            assertNull(b);
            assertEquals(ColorSpace.get(ColorSpace.Named.PRO_PHOTO_RGB), opts.outColorSpace);
            assertEquals(Bitmap.Config.RGBA_F16, opts.outConfig);
        } catch (IOException e) {
            fail();
        }

        opts.inJustDecodeBounds = false;
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);

        try (InputStream in = mResources.getAssets().open(asset)) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void inColorSpace565() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.ADOBE_RGB);
        opts.inPreferredConfig = Bitmap.Config.RGB_565;

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            Bitmap b = BitmapFactory.decodeStream(in, null, opts);
            ColorSpace cs = b.getColorSpace();
            assertNotNull(cs);
            assertSame(opts.inPreferredColorSpace, cs);
            assertSame(opts.inPreferredColorSpace, opts.outColorSpace);
        } catch (IOException e) {
            fail();
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void inColorSpaceNotRGB() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = ColorSpace.get(ColorSpace.Named.CIE_LAB);

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            BitmapFactory.decodeStream(in, null, opts);
        } catch (IOException e) {
            fail();
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void inColorSpaceNoTransferParameters() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredColorSpace = new ColorSpace.Rgb("NoTransferParams",
                new float[]{ 0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f },
                ColorSpace.ILLUMINANT_D50,
                x -> Math.pow(x, 1.0f / 2.2f), x -> Math.pow(x, 2.2f),
                0, 1);

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            BitmapFactory.decodeStream(in, null, opts);
        } catch (IOException e) {
            fail();
        }
    }

    @Test
    public void copyF16() {
        // Copying from (LINEAR_)SRGB to RGBA_F16 results in (LINEAR_)EXTENDED_SRGB.
        ColorSpace[] srcCS = new ColorSpace[] { ColorSpace.get(ColorSpace.Named.SRGB),
            ColorSpace.get(ColorSpace.Named.LINEAR_SRGB) };
        ColorSpace[] dstCS = new ColorSpace[] { ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB),
            ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB) };

        for (int i = 0; i < srcCS.length; ++i) {
            for (Bitmap.Config config : new Bitmap.Config[] { Bitmap.Config.ARGB_8888,
                    Bitmap.Config.RGB_565 }) {
                Bitmap b = Bitmap.createBitmap(10, 10, config, false, srcCS[i]);
                assertSame(srcCS[i], b.getColorSpace());

                for (boolean mutable : new boolean[] { true, false }) {
                    Bitmap copy = b.copy(Bitmap.Config.RGBA_F16, mutable);
                    assertSame(dstCS[i], copy.getColorSpace());
                }
            }
        }

        // The same is true for the reverse
        for (int i = 0; i < srcCS.length; ++i) {
            Bitmap b = Bitmap.createBitmap(10, 10, Bitmap.Config.RGBA_F16, false, dstCS[i]);
            assertSame(dstCS[i], b.getColorSpace());
            for (Bitmap.Config config : new Bitmap.Config[] { Bitmap.Config.ARGB_8888,
                    Bitmap.Config.RGB_565 }) {
                for (boolean mutable : new boolean[] { true, false }) {
                    Bitmap copy = b.copy(config, mutable);
                    assertSame(srcCS[i], copy.getColorSpace());
                }
            }
        }
    }

    @Test
    public void copyAlpha8() {
        for (Bitmap.Config srcConfig : new Bitmap.Config[] {
                Bitmap.Config.ALPHA_8,
                Bitmap.Config.RGB_565,
                Bitmap.Config.ARGB_8888,
                Bitmap.Config.RGBA_F16,
        }) {
            Bitmap b = Bitmap.createBitmap(1, 1, srcConfig);
            assertNotNull(b);
            if (srcConfig == Bitmap.Config.ALPHA_8) {
                assertNull(b.getColorSpace());
            } else {
                assertNotNull(b.getColorSpace());
            }

            Bitmap copy = b.copy(Bitmap.Config.ALPHA_8, false);
            assertNotNull(copy);
            assertNull(copy.getColorSpace());

            Bitmap copy2 = copy.copy(srcConfig, false);
            switch (srcConfig) {
                case RGBA_F16:
                    assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB),
                            copy2.getColorSpace());
                    break;
                case ALPHA_8:
                    assertNull(b.getColorSpace());
                    break;
                default:
                    assertSame("Copied from ALPHA_8 to " + srcConfig,
                            ColorSpace.get(ColorSpace.Named.SRGB), copy2.getColorSpace());
            }
        }
    }

    @Test
    public void copyHardwareToAlpha8() {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.HARDWARE;
        Bitmap b = BitmapFactory.decodeResource(mResources, R.drawable.robot, options);
        assertSame(Bitmap.Config.HARDWARE, b.getConfig());
        assertNotNull(b.getColorSpace());

        Bitmap copy = b.copy(Bitmap.Config.ALPHA_8, false);
        assertNull(copy.getColorSpace());
    }

    @Test
    public void copy() {
        Bitmap b = BitmapFactory.decodeResource(mResources, R.drawable.robot);
        Bitmap c;
        ColorSpace cs;
        boolean[] trueFalse = new boolean[] { true, false };

        for (boolean mutable : trueFalse) {
            c = b.copy(Bitmap.Config.ARGB_8888, mutable);
            cs = c.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.SRGB), cs);
        }

        try (InputStream in = mResources.getAssets().open("green-p3.png")) {
            b = BitmapFactory.decodeStream(in);
            c = b.copy(Bitmap.Config.ARGB_8888, false);
            cs = c.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);

            c = b.copy(Bitmap.Config.ARGB_8888, true);
            cs = c.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), cs);
        } catch (IOException e) {
            fail();
        }

        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            b = BitmapFactory.decodeStream(in);
            c = b.copy(Bitmap.Config.RGBA_F16, false);
            cs = c.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);

            c = b.copy(Bitmap.Config.RGBA_F16, true);
            cs = c.getColorSpace();
            assertNotNull(cs);
            assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), cs);
        } catch (IOException e) {
            fail();
        }
    }

    @SuppressWarnings("SameParameterValue")
    private static void almostEqual(@ColorInt int expected,
            @ColorInt int pixel, int threshold, int index) {
        int diffA = Math.abs((expected >>> 24) - (pixel >>> 24));
        int diffR = Math.abs(((expected >> 16) & 0xff) - ((pixel >> 16) & 0xff));
        int diffG = Math.abs(((expected >>  8) & 0xff) - ((pixel >>  8) & 0xff));
        int diffB = Math.abs((expected & 0xff) - (pixel & 0xff));

        boolean pass = diffA + diffR + diffG + diffB <= threshold;
        if (!pass) {
            Log.d(LOG_TAG, "Expected 0x" + Integer.toHexString(expected) +
                    " but was 0x" + Integer.toHexString(pixel) + " with index " + index);
        }

        assertTrue(pass);
    }

    private Object[] compressFormatsAndColorSpaces() {
        return Utils.crossProduct(Bitmap.CompressFormat.values(),
                BitmapTest.getRgbColorSpaces().toArray());
    }

    @Test
    @Parameters(method = "compressFormatsAndColorSpaces")
    public void testEncodeColorSpace(Bitmap.CompressFormat format, ColorSpace colorSpace) {
        Bitmap b = null;
        ColorSpace decodedColorSpace = null;
        ImageDecoder.Source src = ImageDecoder.createSource(mResources.getAssets(),
                "blue-16bit-srgb.png");
        try {
            b = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                decoder.setTargetColorSpace(colorSpace);
            });
            assertNotNull(b);
            assertEquals(Bitmap.Config.RGBA_F16, b.getConfig());
            decodedColorSpace = b.getColorSpace();

            // Requesting a ColorSpace with an EXTENDED variant will use the EXTENDED one because
            // the image is 16-bit.
            if (colorSpace == ColorSpace.get(ColorSpace.Named.SRGB)) {
                assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), decodedColorSpace);
            } else if (colorSpace == ColorSpace.get(ColorSpace.Named.LINEAR_SRGB)) {
                assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB),
                          decodedColorSpace);
            } else {
                assertSame(colorSpace, decodedColorSpace);
            }
        } catch (IOException e) {
            fail("Failed with " + e);
        }

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        assertTrue("Failed to encode F16 to " + format, b.compress(format, 100, out));

        byte[] array = out.toByteArray();
        src = ImageDecoder.createSource(ByteBuffer.wrap(array));

        try {
            Bitmap b2 = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
            });
            ColorSpace encodedColorSpace = b2.getColorSpace();
            if (format == Bitmap.CompressFormat.PNG) {
                assertEquals(Bitmap.Config.RGBA_F16, b2.getConfig());
                assertSame(decodedColorSpace, encodedColorSpace);
            } else {
                // Compressing to the other formats does not support creating a compressed version
                // that we will decode to F16.
                assertEquals(Bitmap.Config.ARGB_8888, b2.getConfig());

                // Decoding an EXTENDED variant to 8888 results in the non-extended variant.
                if (decodedColorSpace == ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB)) {
                    assertSame(ColorSpace.get(ColorSpace.Named.SRGB), encodedColorSpace);
                } else if (decodedColorSpace
                        == ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB)) {
                    assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_SRGB), encodedColorSpace);
                } else {
                    assertSame(decodedColorSpace, encodedColorSpace);
                }
            }
        } catch (IOException e) {
            fail("Failed with " + e);
        }
    }

    @Test
    public void testEncodeP3hardware() {
        Bitmap b = null;
        ImageDecoder.Source src = ImageDecoder.createSource(mResources.getAssets(),
                "green-p3.png");
        try {
            b = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                decoder.setAllocator(ImageDecoder.ALLOCATOR_HARDWARE);
            });
            assertNotNull(b);
            assertEquals(Bitmap.Config.HARDWARE, b.getConfig());
            assertEquals(ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b.getColorSpace());
        } catch (IOException e) {
            fail("Failed with " + e);
        }

        for (Bitmap.CompressFormat format : Bitmap.CompressFormat.values()) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            assertTrue("Failed to encode 8888 to " + format, b.compress(format, 100, out));

            byte[] array = out.toByteArray();
            src = ImageDecoder.createSource(ByteBuffer.wrap(array));

            try {
                Bitmap b2 = ImageDecoder.decodeBitmap(src);
                assertEquals("Wrong color space for " + format,
                        ColorSpace.get(ColorSpace.Named.DISPLAY_P3), b2.getColorSpace());
            } catch (IOException e) {
                fail("Failed with " + e);
            }
        }
    }

    @Test
    @RequiresDevice // SwiftShader does not yet have support for F16 in HARDWARE b/75778024
    public void test16bitHardware() {
        // Decoding to HARDWARE may use EXTENDED_SRGB or SRGB, depending
        // on whether F16 is supported in HARDWARE.
        try (InputStream in = mResources.getAssets().open("blue-16bit-srgb.png")) {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.HARDWARE;
            Bitmap b = BitmapFactory.decodeStream(in, null, options);
            assertEquals(Bitmap.Config.HARDWARE, b.getConfig());

            final ColorSpace cs = b.getColorSpace();
            if (cs != ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB)
                    && cs != ColorSpace.get(ColorSpace.Named.SRGB)) {
                fail("Unexpected color space " + cs);
            }
        } catch (Exception e) {
            fail("Failed with " + e);
        }
    }

    @Test
    public void testProPhoto() throws IOException {
        ColorSpace extendedSrgb = ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB);
        Color blue = Color.valueOf(0, 0, 1, 1, ColorSpace.get(ColorSpace.Named.PRO_PHOTO_RGB));
        Color expected = blue.convert(extendedSrgb);
        try (InputStream in = mResources.getAssets().open("blue-16bit-prophoto.png")) {
            Bitmap src = BitmapFactory.decodeStream(in, null, null);

            Bitmap dst = Bitmap.createBitmap(src.getWidth(), src.getHeight(),
                    Bitmap.Config.RGBA_F16, true, extendedSrgb);
            Canvas c = new Canvas(dst);
            c.drawBitmap(src, 0, 0, null);
            ColorUtils.verifyColor("PRO_PHOTO image did not convert properly", expected,
                    dst.getColor(0, 0), .001f);
        }
    }

    @Test
    public void testGrayscaleProfile() throws IOException {
        ImageDecoder.Source source = ImageDecoder.createSource(mResources.getAssets(),
                "gimp-d65-grayscale.jpg");
        Bitmap bm = ImageDecoder.decodeBitmap(source, (decoder, info, s) -> {
            decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
        });
        ColorSpace cs = bm.getColorSpace();
        assertNotNull(cs);
        assertTrue(cs instanceof ColorSpace.Rgb);
        ColorSpace.Rgb rgbCs = (ColorSpace.Rgb) cs;

        // A gray color space uses a special primaries array of all 1s.
        float[] primaries = rgbCs.getPrimaries();
        assertNotNull(primaries);
        assertEquals(6, primaries.length);
        for (float primary : primaries) {
            assertEquals(0, Float.compare(primary, 1.0f));
        }

        // A gray color space will have all zeroes in the transform
        // and inverse transform, except for the diagonal.
        for (float[] transform : new float[][]{rgbCs.getTransform(), rgbCs.getInverseTransform()}) {
            assertNotNull(transform);
            assertEquals(9, transform.length);
            for (int index : new int[] { 1, 2, 3, 5, 6, 7 }) {
                assertEquals(0, Float.compare(0.0f, transform[index]));
            }
        }

        // When creating another Bitmap with the same ColorSpace, the two
        // ColorSpaces should be equal.
        Bitmap otherBm = Bitmap.createBitmap(null, 100, 100, Bitmap.Config.ARGB_8888, true, cs);
        assertEquals(cs, otherBm.getColorSpace());

        // Same for a scaled bitmap.
        Bitmap scaledBm = Bitmap.createScaledBitmap(bm, bm.getWidth() / 4, bm.getHeight() / 4,
                true);
        assertEquals(cs, scaledBm.getColorSpace());

        // A previous ColorSpace bug resulted in a Bitmap created like scaledBm
        // having all black pixels. Verify that the Bitmap contains colors other
        // than black and white.
        boolean foundOtherColor = false;
        final int width = scaledBm.getWidth();
        final int height = scaledBm.getHeight();
        int[] pixels = new int[width * height];
        scaledBm.getPixels(pixels, 0, width, 0, 0, width, height);
        for (int pixel : pixels) {
            if (pixel != Color.BLACK && pixel != Color.WHITE) {
                foundOtherColor = true;
                break;
            }
        }
        assertTrue(foundOtherColor);
    }
}
