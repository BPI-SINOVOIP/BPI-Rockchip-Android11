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
package android.graphics.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.ColorSpace.Named;
import android.graphics.ImageDecoder;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.Shader;
import android.hardware.HardwareBuffer;
import android.os.Debug;
import android.os.Parcel;
import android.os.StrictMode;
import android.util.DisplayMetrics;
import android.view.Surface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import com.android.compatibility.common.util.BitmapUtils;
import com.android.compatibility.common.util.ColorUtils;
import com.android.compatibility.common.util.WidgetTestUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

@SmallTest
@RunWith(JUnitParamsRunner.class)
public class BitmapTest {
    // small alpha values cause color values to be pre-multiplied down, losing accuracy
    private static final int PREMUL_COLOR = Color.argb(2, 255, 254, 253);
    private static final int PREMUL_ROUNDED_COLOR = Color.argb(2, 255, 255, 255);
    private static final int PREMUL_STORED_COLOR = Color.argb(2, 2, 2, 2);

    private static final BitmapFactory.Options HARDWARE_OPTIONS = createHardwareBitmapOptions();

    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    private Resources mRes;
    private Bitmap mBitmap;
    private BitmapFactory.Options mOptions;

    public static List<ColorSpace> getRgbColorSpaces() {
        List<ColorSpace> rgbColorSpaces;
        rgbColorSpaces = new ArrayList<ColorSpace>();
        for (ColorSpace.Named e : ColorSpace.Named.values()) {
            ColorSpace cs = ColorSpace.get(e);
            if (cs.getModel() != ColorSpace.Model.RGB) {
                continue;
            }
            if (((ColorSpace.Rgb) cs).getTransferParameters() == null) {
                continue;
            }
            rgbColorSpaces.add(cs);
        }
        return rgbColorSpaces;
    }

    @Before
    public void setup() {
        mRes = InstrumentationRegistry.getTargetContext().getResources();
        mOptions = new BitmapFactory.Options();
        mOptions.inScaled = false;
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
    }

    @Test(expected=IllegalStateException.class)
    public void testCompressRecycled() {
        mBitmap.recycle();
        mBitmap.compress(CompressFormat.JPEG, 0, null);
    }

    @Test(expected=NullPointerException.class)
    public void testCompressNullStream() {
        mBitmap.compress(CompressFormat.JPEG, 0, null);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCompressQualityTooLow() {
        mBitmap.compress(CompressFormat.JPEG, -1, new ByteArrayOutputStream());
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCompressQualityTooHigh() {
        mBitmap.compress(CompressFormat.JPEG, 101, new ByteArrayOutputStream());
    }

    private static Object[] compressFormats() {
        return CompressFormat.values();
    }

    @Test
    @Parameters(method = "compressFormats")
    public void testCompress(CompressFormat format) {
        assertTrue(mBitmap.compress(format, 50, new ByteArrayOutputStream()));
    }

    private Bitmap decodeBytes(byte[] bytes) {
        ByteBuffer buffer = ByteBuffer.wrap(bytes);
        ImageDecoder.Source src = ImageDecoder.createSource(buffer);
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
            });
        } catch (IOException e) {
            fail("Failed to decode with " + e);
            return null;
        }
    }

    // There are three color components and
    // each should be within a square difference of 15 * 15.
    private static final int MSE_MARGIN = 3 * (15 * 15);

    @Test
    public void testCompressWebpLossy() {
        // For qualities < 100, WEBP performs a lossy decode.
        byte[] last = null;
        Bitmap lastBitmap = null;
        for (int quality : new int[] { 25, 50, 80, 99 }) {
            ByteArrayOutputStream webp = new ByteArrayOutputStream();
            assertTrue(mBitmap.compress(CompressFormat.WEBP, quality, webp));
            byte[] webpCompressed = webp.toByteArray();


            ByteArrayOutputStream webpLossy = new ByteArrayOutputStream();
            assertTrue(mBitmap.compress(CompressFormat.WEBP_LOSSY, quality, webpLossy));
            byte[] webpLossyCompressed = webpLossy.toByteArray();

            assertTrue("Compression did not match at quality " + quality,
                    Arrays.equals(webpCompressed, webpLossyCompressed));

            Bitmap result = decodeBytes(webpCompressed);
            if (last != null) {
                // Higher quality will generally result in a larger file.
                assertTrue(webpCompressed.length > last.length);
                if (!BitmapUtils.compareBitmapsMse(lastBitmap, result, MSE_MARGIN, true, false)) {
                    fail("Bad comparison for quality " + quality);
                }
            }
            last = webpCompressed;
            lastBitmap = result;
        }
    }

    @Test
    @Parameters({ "0", "50", "80", "99", "100" })
    public void testCompressWebpLossless(int quality) {
        ByteArrayOutputStream webp = new ByteArrayOutputStream();
        assertTrue(mBitmap.compress(CompressFormat.WEBP_LOSSLESS, quality, webp));
        byte[] webpCompressed = webp.toByteArray();
        Bitmap result = decodeBytes(webpCompressed);

        assertTrue("WEBP_LOSSLESS did not losslessly compress at quality " + quality,
                BitmapUtils.compareBitmaps(mBitmap, result));
    }

    @Test
    public void testCompressWebp100MeansLossless() {
        ByteArrayOutputStream webp = new ByteArrayOutputStream();
        assertTrue(mBitmap.compress(CompressFormat.WEBP, 100, webp));
        byte[] webpCompressed = webp.toByteArray();
        Bitmap result = decodeBytes(webpCompressed);
        assertTrue("WEBP_LOSSLESS did not losslessly compress at quality 100",
                BitmapUtils.compareBitmaps(mBitmap, result));
    }

    @Test
    @Parameters(method = "compressFormats")
    public void testCompressAlpha8Fails(CompressFormat format) {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);
        assertFalse("Incorrectly compressed ALPHA_8 to " + format,
                bitmap.compress(format, 50, new ByteArrayOutputStream()));

        if (format == CompressFormat.WEBP) {
            // Skip the native test, since the NDK just has equivalents for
            // WEBP_LOSSY and WEBP_LOSSLESS.
            return;
        }

        byte[] storage = new byte[16 * 1024];
        OutputStream stream = new ByteArrayOutputStream();
        assertFalse("Incorrectly compressed ALPHA_8 with the NDK to " + format,
                nCompress(bitmap, nativeCompressFormat(format), 50, stream, storage));
    }

    @Test(expected=IllegalStateException.class)
    public void testCopyRecycled() {
        mBitmap.recycle();
        mBitmap.copy(Config.RGB_565, false);
    }

    @Test
    public void testCopy() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap = mBitmap.copy(Config.ARGB_8888, false);
        WidgetTestUtils.assertEquals(mBitmap, bitmap);
    }

    @Test
    public void testCopyConfigs() {
        Config[] supportedConfigs = new Config[] {
                Config.ALPHA_8, Config.RGB_565, Config.ARGB_8888, Config.RGBA_F16,
        };
        for (Config src : supportedConfigs) {
            for (Config dst : supportedConfigs) {
                Bitmap srcBitmap = Bitmap.createBitmap(1, 1, src);
                srcBitmap.eraseColor(Color.WHITE);
                Bitmap dstBitmap = srcBitmap.copy(dst, false);
                assertNotNull("Should support copying from " + src + " to " + dst,
                        dstBitmap);
                if (Config.ALPHA_8 == dst || Config.ALPHA_8 == src) {
                    // Color will be opaque but color information will be lost.
                    assertEquals("Color should be black when copying from " + src + " to "
                            + dst, Color.BLACK, dstBitmap.getPixel(0, 0));
                } else {
                    assertEquals("Color should be preserved when copying from " + src + " to "
                            + dst, Color.WHITE, dstBitmap.getPixel(0, 0));
                }
            }
        }
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCopyMutableHwBitmap() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.copy(Config.HARDWARE, true);
    }

    @Test(expected=RuntimeException.class)
    public void testCopyPixelsToBufferUnsupportedBufferClass() {
        final int pixSize = mBitmap.getRowBytes() * mBitmap.getHeight();

        mBitmap.copyPixelsToBuffer(CharBuffer.allocate(pixSize));
    }

    @Test(expected=RuntimeException.class)
    public void testCopyPixelsToBufferBufferTooSmall() {
        final int pixSize = mBitmap.getRowBytes() * mBitmap.getHeight();
        final int tooSmall = pixSize / 2;

        mBitmap.copyPixelsToBuffer(ByteBuffer.allocate(tooSmall));
    }

    @Test
    public void testCopyPixelsToBuffer() {
        final int pixSize = mBitmap.getRowBytes() * mBitmap.getHeight();

        ByteBuffer byteBuf = ByteBuffer.allocate(pixSize);
        assertEquals(0, byteBuf.position());
        mBitmap.copyPixelsToBuffer(byteBuf);
        assertEquals(pixSize, byteBuf.position());

        ShortBuffer shortBuf = ShortBuffer.allocate(pixSize);
        assertEquals(0, shortBuf.position());
        mBitmap.copyPixelsToBuffer(shortBuf);
        assertEquals(pixSize >> 1, shortBuf.position());

        IntBuffer intBuf1 = IntBuffer.allocate(pixSize);
        assertEquals(0, intBuf1.position());
        mBitmap.copyPixelsToBuffer(intBuf1);
        assertEquals(pixSize >> 2, intBuf1.position());

        Bitmap bitmap = Bitmap.createBitmap(mBitmap.getWidth(), mBitmap.getHeight(),
                mBitmap.getConfig());
        intBuf1.position(0); // copyPixelsToBuffer adjusted the position, so rewind to start
        bitmap.copyPixelsFromBuffer(intBuf1);
        IntBuffer intBuf2 = IntBuffer.allocate(pixSize);
        bitmap.copyPixelsToBuffer(intBuf2);

        assertEquals(pixSize >> 2, intBuf2.position());
        assertEquals(intBuf1.position(), intBuf2.position());
        int size = intBuf1.position();
        intBuf1.position(0);
        intBuf2.position(0);
        for (int i = 0; i < size; i++) {
            assertEquals("mismatching pixels at position " + i, intBuf1.get(), intBuf2.get());
        }
    }

    @Test
    public void testCreateBitmap1() {
        int[] colors = createColors(100);
        Bitmap bitmap = Bitmap.createBitmap(colors, 10, 10, Config.RGB_565);
        assertFalse(bitmap.isMutable());
        Bitmap ret = Bitmap.createBitmap(bitmap);
        assertNotNull(ret);
        assertFalse(ret.isMutable());
        assertEquals(10, ret.getWidth());
        assertEquals(10, ret.getHeight());
        assertEquals(Config.RGB_565, ret.getConfig());
        assertEquals(ANDROID_BITMAP_FORMAT_RGB_565, nGetFormat(ret));
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapNegativeX() {
        Bitmap.createBitmap(mBitmap, -100, 50, 50, 200);
    }

    @Test
    public void testCreateBitmap2() {
        // special case: output bitmap is equal to the input bitmap
        mBitmap = Bitmap.createBitmap(new int[100 * 100], 100, 100, Config.ARGB_8888);
        assertFalse(mBitmap.isMutable()); // createBitmap w/ colors should be immutable
        Bitmap ret = Bitmap.createBitmap(mBitmap, 0, 0, 100, 100);
        assertNotNull(ret);
        assertFalse(ret.isMutable()); // createBitmap from subset should be immutable
        assertTrue(mBitmap.equals(ret));

        //normal case
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        ret = Bitmap.createBitmap(mBitmap, 10, 10, 50, 50);
        assertNotNull(ret);
        assertFalse(mBitmap.equals(ret));
        assertEquals(ANDROID_BITMAP_FORMAT_RGBA_8888, nGetFormat(mBitmap));
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapNegativeXY() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);

        // abnormal case: x and/or y less than 0
        Bitmap.createBitmap(mBitmap, -1, -1, 10, 10, null, false);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapNegativeWidthHeight() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);

        // abnormal case: width and/or height less than 0
        Bitmap.createBitmap(mBitmap, 1, 1, -10, -10, null, false);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapXRegionTooWide() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);

        // abnormal case: (x + width) bigger than source bitmap's width
        Bitmap.createBitmap(mBitmap, 10, 10, 95, 50, null, false);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapYRegionTooTall() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);

        // abnormal case: (y + height) bigger than source bitmap's height
        Bitmap.createBitmap(mBitmap, 10, 10, 50, 95, null, false);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateMutableBitmapWithHardwareConfig() {
        Bitmap.createBitmap(100, 100, Config.HARDWARE);
    }

    @Test
    public void testCreateBitmap3() {
        // special case: output bitmap is equal to the input bitmap
        mBitmap = Bitmap.createBitmap(new int[100 * 100], 100, 100, Config.ARGB_8888);
        Bitmap ret = Bitmap.createBitmap(mBitmap, 0, 0, 100, 100, null, false);
        assertNotNull(ret);
        assertFalse(ret.isMutable()); // subset should be immutable
        assertTrue(mBitmap.equals(ret));

        // normal case
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        ret = Bitmap.createBitmap(mBitmap, 10, 10, 50, 50, new Matrix(), true);
        assertTrue(ret.isMutable());
        assertNotNull(ret);
        assertFalse(mBitmap.equals(ret));
    }

    @Test
    public void testCreateBitmapFromHardwareBitmap() {
        Bitmap hardwareBitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot,
                HARDWARE_OPTIONS);
        assertEquals(Config.HARDWARE, hardwareBitmap.getConfig());

        Bitmap ret = Bitmap.createBitmap(hardwareBitmap, 0, 0, 100, 100, null, false);
        assertEquals(Config.HARDWARE, ret.getConfig());
        assertFalse(ret.isMutable());
    }

    @Test
    public void testCreateBitmap4() {
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        assertNotNull(ret);
        assertTrue(ret.isMutable());
        assertEquals(100, ret.getWidth());
        assertEquals(200, ret.getHeight());
        assertEquals(Config.RGB_565, ret.getConfig());
    }

    private static void verify2x2BitmapContents(int[] expected, Bitmap observed) {
        ColorUtils.verifyColor(expected[0], observed.getPixel(0, 0));
        ColorUtils.verifyColor(expected[1], observed.getPixel(1, 0));
        ColorUtils.verifyColor(expected[2], observed.getPixel(0, 1));
        ColorUtils.verifyColor(expected[3], observed.getPixel(1, 1));
    }

    @Test
    public void testCreateBitmap_matrix() {
        int[] colorArray = new int[] { Color.RED, Color.GREEN, Color.BLUE, Color.BLACK };
        Bitmap src = Bitmap.createBitmap(2, 2, Config.ARGB_8888);
        assertTrue(src.isMutable());
        src.setPixels(colorArray,0, 2, 0, 0, 2, 2);

        // baseline
        verify2x2BitmapContents(colorArray, src);

        // null
        Bitmap dst = Bitmap.createBitmap(src, 0, 0, 2, 2, null, false);
        assertTrue(dst.isMutable());
        verify2x2BitmapContents(colorArray, dst);

        // identity matrix
        Matrix matrix = new Matrix();
        dst = Bitmap.createBitmap(src, 0, 0, 2, 2, matrix, false);
        assertTrue(dst.isMutable());
        verify2x2BitmapContents(colorArray, dst);

        // big scale - only red visible
        matrix.setScale(10, 10);
        dst = Bitmap.createBitmap(src, 0, 0, 2, 2, matrix, false);
        assertTrue(dst.isMutable());
        verify2x2BitmapContents(new int[] { Color.RED, Color.RED, Color.RED, Color.RED }, dst);

        // rotation
        matrix.setRotate(90);
        dst = Bitmap.createBitmap(src, 0, 0, 2, 2, matrix, false);
        assertTrue(dst.isMutable());
        verify2x2BitmapContents(
                new int[] { Color.BLUE, Color.RED, Color.BLACK, Color.GREEN }, dst);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapFromColorsNegativeWidthHeight() {
        int[] colors = createColors(100);

        // abnormal case: width and/or height less than 0
        Bitmap.createBitmap(colors, 0, 100, -1, 100, Config.RGB_565);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testCreateBitmapFromColorsIllegalStride() {
        int[] colors = createColors(100);

        // abnormal case: stride less than width and bigger than -width
        Bitmap.createBitmap(colors, 10, 10, 100, 100, Config.RGB_565);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testCreateBitmapFromColorsNegativeOffset() {
        int[] colors = createColors(100);

        // abnormal case: offset less than 0
        Bitmap.createBitmap(colors, -10, 100, 100, 100, Config.RGB_565);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testCreateBitmapFromColorsOffsetTooLarge() {
        int[] colors = createColors(100);

        // abnormal case: (offset + width) bigger than colors' length
        Bitmap.createBitmap(colors, 10, 100, 100, 100, Config.RGB_565);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testCreateBitmapFromColorsScalnlineTooLarge() {
        int[] colors = createColors(100);

        // abnormal case: (lastScanline + width) bigger than colors' length
        Bitmap.createBitmap(colors, 10, 100, 50, 100, Config.RGB_565);
    }

    @Test
    public void testCreateBitmap6() {
        int[] colors = createColors(100);

        // normal case
        Bitmap ret = Bitmap.createBitmap(colors, 5, 10, 10, 5, Config.RGB_565);
        assertNotNull(ret);
        assertFalse(ret.isMutable());
        assertEquals(10, ret.getWidth());
        assertEquals(5, ret.getHeight());
        assertEquals(Config.RGB_565, ret.getConfig());
    }

    @Test
    public void testCreateBitmap_displayMetrics_mutable() {
        DisplayMetrics metrics =
                InstrumentationRegistry.getTargetContext().getResources().getDisplayMetrics();

        Bitmap bitmap;
        bitmap = Bitmap.createBitmap(metrics, 10, 10, Config.ARGB_8888);
        assertTrue(bitmap.isMutable());
        assertEquals(metrics.densityDpi, bitmap.getDensity());

        bitmap = Bitmap.createBitmap(metrics, 10, 10, Config.ARGB_8888);
        assertTrue(bitmap.isMutable());
        assertEquals(metrics.densityDpi, bitmap.getDensity());

        bitmap = Bitmap.createBitmap(metrics, 10, 10, Config.ARGB_8888, true);
        assertTrue(bitmap.isMutable());
        assertEquals(metrics.densityDpi, bitmap.getDensity());

        bitmap = Bitmap.createBitmap(metrics, 10, 10, Config.ARGB_8888, true, ColorSpace.get(
                ColorSpace.Named.SRGB));

        assertTrue(bitmap.isMutable());
        assertEquals(metrics.densityDpi, bitmap.getDensity());

        int[] colors = createColors(100);
        bitmap = Bitmap.createBitmap(metrics, colors, 0, 10, 10, 10, Config.ARGB_8888);
        assertNotNull(bitmap);
        assertFalse(bitmap.isMutable());

        bitmap = Bitmap.createBitmap(metrics, colors, 10, 10, Config.ARGB_8888);
        assertNotNull(bitmap);
        assertFalse(bitmap.isMutable());
    }

    @Test
    public void testCreateBitmap_noDisplayMetrics_mutable() {
        Bitmap bitmap;
        bitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        assertTrue(bitmap.isMutable());

        bitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888, true);
        assertTrue(bitmap.isMutable());

        bitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888, true, ColorSpace.get(Named.SRGB));
        assertTrue(bitmap.isMutable());
    }

    @Test
    public void testCreateBitmap_displayMetrics_immutable() {
        DisplayMetrics metrics =
                InstrumentationRegistry.getTargetContext().getResources().getDisplayMetrics();
        int[] colors = createColors(100);

        Bitmap bitmap;
        bitmap = Bitmap.createBitmap(metrics, colors, 0, 10, 10, 10, Config.ARGB_8888);
        assertFalse(bitmap.isMutable());
        assertEquals(metrics.densityDpi, bitmap.getDensity());

        bitmap = Bitmap.createBitmap(metrics, colors, 10, 10, Config.ARGB_8888);
        assertFalse(bitmap.isMutable());
        assertEquals(metrics.densityDpi, bitmap.getDensity());
    }

    @Test
    public void testCreateBitmap_noDisplayMetrics_immutable() {
        int[] colors = createColors(100);
        Bitmap bitmap;
        bitmap = Bitmap.createBitmap(colors, 0, 10, 10, 10, Config.ARGB_8888);
        assertFalse(bitmap.isMutable());

        bitmap = Bitmap.createBitmap(colors, 10, 10, Config.ARGB_8888);
        assertFalse(bitmap.isMutable());
    }

    @Test
    public void testCreateBitmap_Picture_immutable() {
        Picture picture = new Picture();
        Canvas canvas = picture.beginRecording(200, 100);

        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);

        p.setColor(0x88FF0000);
        canvas.drawCircle(50, 50, 40, p);

        p.setColor(Color.GREEN);
        p.setTextSize(30);
        canvas.drawText("Pictures", 60, 60, p);
        picture.endRecording();

        Bitmap bitmap;
        bitmap = Bitmap.createBitmap(picture);
        assertFalse(bitmap.isMutable());

        bitmap = Bitmap.createBitmap(picture, 100, 100, Config.HARDWARE);
        assertFalse(bitmap.isMutable());
        assertNotNull(bitmap.getColorSpace());

        bitmap = Bitmap.createBitmap(picture, 100, 100, Config.ARGB_8888);
        assertFalse(bitmap.isMutable());
    }

    @Test
    public void testCreateScaledBitmap() {
        mBitmap = Bitmap.createBitmap(100, 200, Config.RGB_565);
        assertTrue(mBitmap.isMutable());
        Bitmap ret = Bitmap.createScaledBitmap(mBitmap, 50, 100, false);
        assertNotNull(ret);
        assertEquals(50, ret.getWidth());
        assertEquals(100, ret.getHeight());
        assertTrue(ret.isMutable());
    }

    @Test
    public void testWrapHardwareBufferSucceeds() {
        try (HardwareBuffer hwBuffer = createTestBuffer(128, 128, false)) {
            Bitmap bitmap = Bitmap.wrapHardwareBuffer(hwBuffer, ColorSpace.get(Named.SRGB));
            assertNotNull(bitmap);
            bitmap.recycle();
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testWrapHardwareBufferWithInvalidUsageFails() {
        try (HardwareBuffer hwBuffer = HardwareBuffer.create(512, 512, HardwareBuffer.RGBA_8888, 1,
            HardwareBuffer.USAGE_CPU_WRITE_RARELY)) {
            Bitmap bitmap = Bitmap.wrapHardwareBuffer(hwBuffer, ColorSpace.get(Named.SRGB));
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testWrapHardwareBufferWithRgbBufferButNonRgbColorSpaceFails() {
        try (HardwareBuffer hwBuffer = HardwareBuffer.create(512, 512, HardwareBuffer.RGBA_8888, 1,
            HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE)) {
            Bitmap bitmap = Bitmap.wrapHardwareBuffer(hwBuffer, ColorSpace.get(Named.CIE_LAB));
        }
    }

    @Test
    public void testGenerationId() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        int genId = bitmap.getGenerationId();
        assertEquals("not expected to change", genId, bitmap.getGenerationId());
        bitmap.setDensity(bitmap.getDensity() + 4);
        assertEquals("not expected to change", genId, bitmap.getGenerationId());
        bitmap.getPixel(0, 0);
        assertEquals("not expected to change", genId, bitmap.getGenerationId());

        int beforeGenId = bitmap.getGenerationId();
        bitmap.eraseColor(Color.WHITE);
        int afterGenId = bitmap.getGenerationId();
        assertTrue("expected to increase", afterGenId > beforeGenId);

        beforeGenId = bitmap.getGenerationId();
        bitmap.setPixel(4, 4, Color.BLUE);
        afterGenId = bitmap.getGenerationId();
        assertTrue("expected to increase again", afterGenId > beforeGenId);
    }

    @Test
    public void testDescribeContents() {
        assertEquals(0, mBitmap.describeContents());
    }

    @Test(expected=IllegalStateException.class)
    public void testEraseColorOnRecycled() {
        mBitmap.recycle();

        mBitmap.eraseColor(0);
    }

    @Test(expected = IllegalStateException.class)
    public void testEraseColorLongOnRecycled() {
        mBitmap.recycle();

        mBitmap.eraseColor(Color.pack(0));
    }

    @Test(expected=IllegalStateException.class)
    public void testEraseColorOnImmutable() {
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);

        //abnormal case: bitmap is immutable
        mBitmap.eraseColor(0);
    }

    @Test(expected = IllegalStateException.class)
    public void testEraseColorLongOnImmutable() {
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);

        //abnormal case: bitmap is immutable
        mBitmap.eraseColor(Color.pack(0));
    }

    @Test
    public void testEraseColor() {
        // normal case
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.eraseColor(0xffff0000);
        assertEquals(0xffff0000, mBitmap.getPixel(10, 10));
        assertEquals(0xffff0000, mBitmap.getPixel(50, 50));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetColorOOB() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.getColor(-1, 0);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetColorOOB2() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.getColor(5, -10);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetColorOOB3() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.getColor(100, 10);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetColorOOB4() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.getColor(99, 1000);
    }

    @Test(expected = IllegalStateException.class)
    public void testGetColorRecycled() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.recycle();
        mBitmap.getColor(0, 0);
    }

    @Test(expected = IllegalStateException.class)
    public void testGetColorHardware() {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.HARDWARE;
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, options);
        mBitmap.getColor(50, 50);

    }

    private static float clamp(float f) {
        return clamp(f, 0.0f, 1.0f);
    }

    private static float clamp(float f, float min, float max) {
        return Math.min(Math.max(f, min), max);
    }

    @Test
    public void testGetColor() {
        final ColorSpace sRGB = ColorSpace.get(ColorSpace.Named.SRGB);
        List<ColorSpace> rgbColorSpaces = getRgbColorSpaces();
        for (Config config : new Config[] { Config.ARGB_8888, Config.RGBA_F16, Config.RGB_565 }) {
            for (ColorSpace bitmapColorSpace : rgbColorSpaces) {
                mBitmap = Bitmap.createBitmap(1, 1, config, /*hasAlpha*/ false,
                        bitmapColorSpace);
                bitmapColorSpace = mBitmap.getColorSpace();
                for (ColorSpace eraseColorSpace : rgbColorSpaces) {
                    for (long wideGamutLong : new long[] {
                            Color.pack(1.0f, 0.0f, 0.0f, 1.0f, eraseColorSpace),
                            Color.pack(0.0f, 1.0f, 0.0f, 1.0f, eraseColorSpace),
                            Color.pack(0.0f, 0.0f, 1.0f, 1.0f, eraseColorSpace)}) {
                        mBitmap.eraseColor(wideGamutLong);

                        Color result = mBitmap.getColor(0, 0);
                        if (mBitmap.getColorSpace().equals(sRGB)) {
                            assertEquals(mBitmap.getPixel(0, 0), result.toArgb());
                        }
                        if (eraseColorSpace.equals(bitmapColorSpace)) {
                            final Color wideGamutColor = Color.valueOf(wideGamutLong);
                            ColorUtils.verifyColor("Erasing to Bitmap's ColorSpace "
                                    + bitmapColorSpace, wideGamutColor, result, .001f);

                        } else {
                            Color convertedColor = Color.valueOf(
                                    Color.convert(wideGamutLong, bitmapColorSpace));
                            if (mBitmap.getConfig() != Config.RGBA_F16) {
                                // It's possible that we have to clip to fit into the Config.
                                convertedColor = Color.valueOf(
                                        clamp(convertedColor.red()),
                                        clamp(convertedColor.green()),
                                        clamp(convertedColor.blue()),
                                        convertedColor.alpha(),
                                        bitmapColorSpace);
                            }
                            ColorUtils.verifyColor("Bitmap(Config: " + mBitmap.getConfig()
                                    + ", ColorSpace: " + bitmapColorSpace
                                    + ") erasing to " + Color.valueOf(wideGamutLong),
                                    convertedColor, result, .03f);
                        }
                    }
                }
            }
        }
    }

    private static class ARGB {
        public float alpha;
        public float red;
        public float green;
        public float blue;
        ARGB(float alpha, float red, float green, float blue) {
            this.alpha = alpha;
            this.red = red;
            this.green = green;
            this.blue = blue;
        }
    };

    @Test
    public void testEraseColorLong() {
        List<ColorSpace> rgbColorSpaces = getRgbColorSpaces();
        for (Config config : new Config[]{Config.ARGB_8888, Config.RGB_565, Config.RGBA_F16}) {
            mBitmap = Bitmap.createBitmap(100, 100, config);
            // pack SRGB colors into ColorLongs.
            for (int color : new int[]{ Color.RED, Color.BLUE, Color.GREEN, Color.BLACK,
                    Color.WHITE, Color.TRANSPARENT }) {
                if (config.equals(Config.RGB_565) && Float.compare(Color.alpha(color), 1.0f) != 0) {
                    // 565 doesn't support alpha.
                    continue;
                }
                mBitmap.eraseColor(Color.pack(color));
                // The Bitmap is either SRGB or SRGBLinear (F16). getPixel(), which retrieves the
                // color in SRGB, should match exactly.
                ColorUtils.verifyColor("Config " + config + " mismatch at 10, 10 ",
                        color, mBitmap.getPixel(10, 10), 0);
                ColorUtils.verifyColor("Config " + config + " mismatch at 50, 50 ",
                        color, mBitmap.getPixel(50, 50), 0);
            }

            // Use arbitrary colors in various ColorSpaces. getPixel() should approximately match
            // the SRGB version of the color.
            for (ARGB color : new ARGB[]{ new ARGB(1.0f, .5f, .5f, .5f),
                                          new ARGB(1.0f, .3f, .6f, .9f),
                                          new ARGB(0.5f, .2f, .8f, .7f) }) {
                if (config.equals(Config.RGB_565) && Float.compare(color.alpha, 1.0f) != 0) {
                    continue;
                }
                int srgbColor = Color.argb(color.alpha, color.red, color.green, color.blue);
                for (ColorSpace cs : rgbColorSpaces) {
                    long longColor = Color.convert(srgbColor, cs);
                    mBitmap.eraseColor(longColor);
                    // These tolerances were chosen by trial and error. It is expected that
                    // some conversions do not round-trip perfectly.
                    int tolerance = 1;
                    if (config.equals(Config.RGB_565)) {
                        tolerance = 4;
                    } else if (cs.equals(ColorSpace.get(ColorSpace.Named.SMPTE_C))) {
                        tolerance = 3;
                    }

                    ColorUtils.verifyColor("Config " + config + ", ColorSpace " + cs
                            + ", mismatch at 10, 10 ", srgbColor, mBitmap.getPixel(10, 10),
                            tolerance);
                    ColorUtils.verifyColor("Config " + config + ", ColorSpace " + cs
                            + ", mismatch at 50, 50 ", srgbColor, mBitmap.getPixel(50, 50),
                            tolerance);
                }
            }
        }
    }

    @Test
    public void testEraseColorOnP3() {
        // Use a ColorLong with a different ColorSpace than the Bitmap. getPixel() should
        // approximately match the SRGB version of the color.
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888, true,
                ColorSpace.get(ColorSpace.Named.DISPLAY_P3));
        int srgbColor = Color.argb(.5f, .3f, .6f, .7f);
        long acesColor = Color.convert(srgbColor, ColorSpace.get(ColorSpace.Named.ACES));
        mBitmap.eraseColor(acesColor);
        ColorUtils.verifyColor("Mismatch at 15, 15", srgbColor, mBitmap.getPixel(15, 15), 1);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testEraseColorXYZ() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.eraseColor(Color.convert(Color.BLUE, ColorSpace.get(ColorSpace.Named.CIE_XYZ)));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testEraseColorLAB() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.eraseColor(Color.convert(Color.BLUE, ColorSpace.get(ColorSpace.Named.CIE_LAB)));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testEraseColorUnknown() {
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.eraseColor(-1L);
    }

    @Test(expected=IllegalStateException.class)
    public void testExtractAlphaFromRecycled() {
        mBitmap.recycle();

        mBitmap.extractAlpha();
    }

    @Test
    public void testExtractAlpha() {
        // normal case
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
        Bitmap ret = mBitmap.extractAlpha();
        assertNotNull(ret);
        int source = mBitmap.getPixel(10, 20);
        int result = ret.getPixel(10, 20);
        assertEquals(Color.alpha(source), Color.alpha(result));
        assertEquals(0xFF, Color.alpha(result));
    }

    @Test(expected=IllegalStateException.class)
    public void testExtractAlphaWithPaintAndOffsetFromRecycled() {
        mBitmap.recycle();

        mBitmap.extractAlpha(new Paint(), new int[]{0, 1});
    }

    @Test
    public void testExtractAlphaWithPaintAndOffset() {
        // normal case
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
        Bitmap ret = mBitmap.extractAlpha(new Paint(), new int[]{0, 1});
        assertNotNull(ret);
        int source = mBitmap.getPixel(10, 20);
        int result = ret.getPixel(10, 20);
        assertEquals(Color.alpha(source), Color.alpha(result));
        assertEquals(0xFF, Color.alpha(result));
    }

    @Test
    public void testGetAllocationByteCount() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        int alloc = mBitmap.getAllocationByteCount();
        assertEquals(mBitmap.getByteCount(), alloc);

        // reconfigure same size
        mBitmap.reconfigure(50, 100, Bitmap.Config.ARGB_8888);
        assertEquals(mBitmap.getByteCount(), alloc);
        assertEquals(mBitmap.getAllocationByteCount(), alloc);

        // reconfigure different size
        mBitmap.reconfigure(10, 10, Bitmap.Config.ALPHA_8);
        assertEquals(mBitmap.getByteCount(), 100);
        assertEquals(mBitmap.getAllocationByteCount(), alloc);
    }

    @Test
    public void testGetConfig() {
        Bitmap bm0 = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        Bitmap bm1 = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        Bitmap bm2 = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);
        Bitmap bm3 = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_4444);

        assertEquals(Bitmap.Config.ALPHA_8, bm0.getConfig());
        assertEquals(Bitmap.Config.ARGB_8888, bm1.getConfig());
        assertEquals(Bitmap.Config.RGB_565, bm2.getConfig());
        // Attempting to create a 4444 bitmap actually creates an 8888 bitmap.
        assertEquals(Bitmap.Config.ARGB_8888, bm3.getConfig());

        // Can't call Bitmap.createBitmap with Bitmap.Config.HARDWARE,
        // because createBitmap creates mutable bitmap and hardware bitmaps are always immutable,
        // so such call will throw an exception.
        Bitmap hardwareBitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot,
                HARDWARE_OPTIONS);
        assertEquals(Bitmap.Config.HARDWARE, hardwareBitmap.getConfig());
    }

    @Test
    public void testGetHeight() {
        assertEquals(31, mBitmap.getHeight());
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        assertEquals(200, mBitmap.getHeight());
    }

    @Test
    public void testGetNinePatchChunk() {
        assertNull(mBitmap.getNinePatchChunk());
    }

    @Test(expected=IllegalStateException.class)
    public void testGetPixelFromRecycled() {
        mBitmap.recycle();

        mBitmap.getPixel(10, 16);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testGetPixelXTooLarge() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        // abnormal case: x bigger than the source bitmap's width
        mBitmap.getPixel(200, 16);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testGetPixelYTooLarge() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        // abnormal case: y bigger than the source bitmap's height
        mBitmap.getPixel(10, 300);
    }

    @Test
    public void testGetPixel() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        // normal case 565
        mBitmap.setPixel(10, 16, 0xFF << 24);
        assertEquals(0xFF << 24, mBitmap.getPixel(10, 16));

        // normal case A_8
        mBitmap = Bitmap.createBitmap(10, 10, Config.ALPHA_8);
        mBitmap.setPixel(5, 5, 0xFFFFFFFF);
        assertEquals(0xFF000000, mBitmap.getPixel(5, 5));
        mBitmap.setPixel(5, 5, 0xA8A8A8A8);
        assertEquals(0xA8000000, mBitmap.getPixel(5, 5));
        mBitmap.setPixel(5, 5, 0x00000000);
        assertEquals(0x00000000, mBitmap.getPixel(5, 5));
        mBitmap.setPixel(5, 5, 0x1F000000);
        assertEquals(0x1F000000, mBitmap.getPixel(5, 5));
    }

    @Test
    public void testGetRowBytes() {
        Bitmap bm0 = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        Bitmap bm1 = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        Bitmap bm2 = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);
        Bitmap bm3 = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_4444);

        assertEquals(100, bm0.getRowBytes());
        assertEquals(400, bm1.getRowBytes());
        assertEquals(200, bm2.getRowBytes());
        // Attempting to create a 4444 bitmap actually creates an 8888 bitmap.
        assertEquals(400, bm3.getRowBytes());
    }

    @Test
    public void testGetWidth() {
        assertEquals(31, mBitmap.getWidth());
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        assertEquals(100, mBitmap.getWidth());
    }

    @Test
    public void testHasAlpha() {
        assertFalse(mBitmap.hasAlpha());
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        assertTrue(mBitmap.hasAlpha());
    }

    @Test
    public void testIsMutable() {
        assertFalse(mBitmap.isMutable());
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        assertTrue(mBitmap.isMutable());
    }

    @Test
    public void testIsRecycled() {
        assertFalse(mBitmap.isRecycled());
        mBitmap.recycle();
        assertTrue(mBitmap.isRecycled());
    }

    @Test
    public void testReconfigure() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);
        int alloc = mBitmap.getAllocationByteCount();

        // test shrinking
        mBitmap.reconfigure(50, 100, Bitmap.Config.ALPHA_8);
        assertEquals(mBitmap.getAllocationByteCount(), alloc);
        assertEquals(mBitmap.getByteCount() * 8, alloc);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testReconfigureExpanding() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);
        mBitmap.reconfigure(101, 201, Bitmap.Config.ARGB_8888);
    }

    @Test(expected=IllegalStateException.class)
    public void testReconfigureMutable() {
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
        mBitmap.reconfigure(1, 1, Bitmap.Config.ALPHA_8);
    }

    // Used by testAlphaAndPremul.
    private static Config[] CONFIGS = new Config[] { Config.ALPHA_8, Config.ARGB_4444,
            Config.ARGB_8888, Config.RGB_565 };

    // test that reconfigure, setHasAlpha, and setPremultiplied behave as expected with
    // respect to alpha and premultiplied.
    @Test
    public void testAlphaAndPremul() {
        boolean falseTrue[] = new boolean[] { false, true };
        for (Config fromConfig : CONFIGS) {
            for (Config toConfig : CONFIGS) {
                for (boolean hasAlpha : falseTrue) {
                    for (boolean isPremul : falseTrue) {
                        Bitmap bitmap = Bitmap.createBitmap(10, 10, fromConfig);

                        // 4444 is deprecated, and will convert to 8888. No need to
                        // attempt a reconfigure, which will be tested when fromConfig
                        // is 8888.
                        if (fromConfig == Config.ARGB_4444) {
                            assertEquals(bitmap.getConfig(), Config.ARGB_8888);
                            break;
                        }

                        bitmap.setHasAlpha(hasAlpha);
                        bitmap.setPremultiplied(isPremul);

                        verifyAlphaAndPremul(bitmap, hasAlpha, isPremul, false);

                        // reconfigure to a smaller size so the function will still succeed when
                        // going to a Config that requires more bits.
                        bitmap.reconfigure(1, 1, toConfig);
                        if (toConfig == Config.ARGB_4444) {
                            assertEquals(bitmap.getConfig(), Config.ARGB_8888);
                        } else {
                            assertEquals(bitmap.getConfig(), toConfig);
                        }

                        // Check that the alpha and premultiplied state has not changed (unless
                        // we expected it to).
                        verifyAlphaAndPremul(bitmap, hasAlpha, isPremul, fromConfig == Config.RGB_565);
                    }
                }
            }
        }
    }

    /**
     *  Assert that bitmap returns the appropriate values for hasAlpha() and isPremultiplied().
     *  @param bitmap Bitmap to check.
     *  @param expectedAlpha Expected return value from bitmap.hasAlpha(). Note that this is based
     *          on what was set, but may be different from the actual return value depending on the
     *          Config and convertedFrom565.
     *  @param expectedPremul Expected return value from bitmap.isPremultiplied(). Similar to
     *          expectedAlpha, this is based on what was set, but may be different from the actual
     *          return value depending on the Config.
     *  @param convertedFrom565 Whether bitmap was converted to its current Config by being
     *          reconfigured from RGB_565. If true, and bitmap is now a Config that supports alpha,
     *          hasAlpha() is expected to be true even if expectedAlpha is false.
     */
    private void verifyAlphaAndPremul(Bitmap bitmap, boolean expectedAlpha, boolean expectedPremul,
            boolean convertedFrom565) {
        switch (bitmap.getConfig()) {
            case ARGB_4444:
                // This shouldn't happen, since we don't allow creating or converting
                // to 4444.
                assertFalse(true);
                break;
            case RGB_565:
                assertFalse(bitmap.hasAlpha());
                assertFalse(bitmap.isPremultiplied());
                break;
            case ALPHA_8:
                // ALPHA_8 behaves mostly the same as 8888, except for premultiplied. Fall through.
            case ARGB_8888:
                // Since 565 is necessarily opaque, we revert to hasAlpha when switching to a type
                // that can have alpha.
                if (convertedFrom565) {
                    assertTrue(bitmap.hasAlpha());
                } else {
                    assertEquals(bitmap.hasAlpha(), expectedAlpha);
                }

                if (bitmap.hasAlpha()) {
                    // ALPHA_8's premultiplied status is undefined.
                    if (bitmap.getConfig() != Config.ALPHA_8) {
                        assertEquals(bitmap.isPremultiplied(), expectedPremul);
                    }
                } else {
                    // Opaque bitmap is never considered premultiplied.
                    assertFalse(bitmap.isPremultiplied());
                }
                break;
        }
    }

    @Test
    public void testSetColorSpace() {
        // Use arbitrary colors and assign to various ColorSpaces.
        for (ARGB color : new ARGB[]{ new ARGB(1.0f, .5f, .5f, .5f),
                new ARGB(1.0f, .3f, .6f, .9f),
                new ARGB(0.5f, .2f, .8f, .7f) }) {

            int srgbColor = Color.argb(color.alpha, color.red, color.green, color.blue);
            for (ColorSpace cs : getRgbColorSpaces()) {
                for (Config config : new Config[] {
                        // F16 is tested elsewhere, since it defaults to EXTENDED_SRGB, and
                        // many of these calls to setColorSpace would reduce the range, resulting
                        // in an Exception.
                        Config.ARGB_8888,
                        Config.RGB_565,
                }) {
                    mBitmap = Bitmap.createBitmap(10, 10, config);
                    mBitmap.eraseColor(srgbColor);
                    mBitmap.setColorSpace(cs);
                    ColorSpace actual = mBitmap.getColorSpace();
                    if (cs == ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB)) {
                        assertSame(ColorSpace.get(ColorSpace.Named.SRGB), actual);
                    } else if (cs == ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB)) {
                        assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_SRGB), actual);
                    } else {
                        assertSame(cs, actual);
                    }

                    // This tolerance was chosen by trial and error. It is expected that
                    // some conversions do not round-trip perfectly.
                    int tolerance = 2;
                    Color c = Color.valueOf(color.red, color.green, color.blue, color.alpha, cs);
                    ColorUtils.verifyColor("Mismatch after setting the colorSpace to "
                            + cs.getName(), c.convert(mBitmap.getColorSpace()),
                            mBitmap.getColor(5, 5), tolerance);
                }
            }
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testSetColorSpaceRecycled() {
        mBitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        mBitmap.recycle();
        mBitmap.setColorSpace(ColorSpace.get(Named.DISPLAY_P3));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testSetColorSpaceNull() {
        mBitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        mBitmap.setColorSpace(null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testSetColorSpaceXYZ() {
        mBitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        mBitmap.setColorSpace(ColorSpace.get(Named.CIE_XYZ));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testSetColorSpaceNoTransferParameters() {
        mBitmap = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        ColorSpace cs = new ColorSpace.Rgb("NoTransferParams",
                new float[]{ 0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f },
                ColorSpace.ILLUMINANT_D50,
                x -> Math.pow(x, 1.0f / 2.2f), x -> Math.pow(x, 2.2f),
                0, 1);
        mBitmap.setColorSpace(cs);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testSetColorSpaceAlpha8() {
        mBitmap = Bitmap.createBitmap(10, 10, Config.ALPHA_8);
        assertNull(mBitmap.getColorSpace());
        mBitmap.setColorSpace(ColorSpace.get(ColorSpace.Named.SRGB));
    }

    @Test
    public void testSetColorSpaceReducedRange() {
        ColorSpace aces = ColorSpace.get(Named.ACES);
        mBitmap = Bitmap.createBitmap(10, 10, Config.RGBA_F16, true, aces);
        try {
            mBitmap.setColorSpace(ColorSpace.get(Named.SRGB));
            fail("Expected IllegalArgumentException!");
        } catch (IllegalArgumentException e) {
            assertSame(aces, mBitmap.getColorSpace());
        }
    }

    @Test
    public void testSetColorSpaceNotReducedRange() {
        ColorSpace extended = ColorSpace.get(Named.EXTENDED_SRGB);
        mBitmap = Bitmap.createBitmap(10, 10, Config.RGBA_F16, true,
                extended);
        mBitmap.setColorSpace(ColorSpace.get(Named.SRGB));
        assertSame(mBitmap.getColorSpace(), extended);
    }

    @Test
    public void testSetColorSpaceNotReducedRangeLinear() {
        ColorSpace linearExtended = ColorSpace.get(Named.LINEAR_EXTENDED_SRGB);
        mBitmap = Bitmap.createBitmap(10, 10, Config.RGBA_F16, true,
                linearExtended);
        mBitmap.setColorSpace(ColorSpace.get(Named.LINEAR_SRGB));
        assertSame(mBitmap.getColorSpace(), linearExtended);
    }

    @Test
    public void testSetColorSpaceIncreasedRange() {
        mBitmap = Bitmap.createBitmap(10, 10, Config.RGBA_F16, true,
                ColorSpace.get(Named.DISPLAY_P3));
        ColorSpace linearExtended = ColorSpace.get(Named.LINEAR_EXTENDED_SRGB);
        mBitmap.setColorSpace(linearExtended);
        assertSame(mBitmap.getColorSpace(), linearExtended);
    }

    @Test
    public void testSetConfig() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);
        int alloc = mBitmap.getAllocationByteCount();

        // test shrinking
        mBitmap.setConfig(Bitmap.Config.ALPHA_8);
        assertEquals(mBitmap.getAllocationByteCount(), alloc);
        assertEquals(mBitmap.getByteCount() * 2, alloc);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetConfigExpanding() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);
        // test expanding
        mBitmap.setConfig(Bitmap.Config.ARGB_8888);
    }

    @Test(expected=IllegalStateException.class)
    public void testSetConfigMutable() {
        // test mutable
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
        mBitmap.setConfig(Bitmap.Config.ALPHA_8);
    }

    @Test
    public void testSetHeight() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        int alloc = mBitmap.getAllocationByteCount();

        // test shrinking
        mBitmap.setHeight(100);
        assertEquals(mBitmap.getAllocationByteCount(), alloc);
        assertEquals(mBitmap.getByteCount() * 2, alloc);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetHeightExpanding() {
        // test expanding
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        mBitmap.setHeight(201);
    }

    @Test(expected=IllegalStateException.class)
    public void testSetHeightMutable() {
        // test mutable
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
        mBitmap.setHeight(1);
    }

    @Test(expected=IllegalStateException.class)
    public void testSetPixelOnRecycled() {
        int color = 0xff << 24;

        mBitmap.recycle();
        mBitmap.setPixel(10, 16, color);
    }

    @Test(expected=IllegalStateException.class)
    public void testSetPixelOnImmutable() {
        int color = 0xff << 24;
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);

        mBitmap.setPixel(10, 16, color);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelXIsTooLarge() {
        int color = 0xff << 24;
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        // abnormal case: x bigger than the source bitmap's width
        mBitmap.setPixel(200, 16, color);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelYIsTooLarge() {
        int color = 0xff << 24;
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        // abnormal case: y bigger than the source bitmap's height
        mBitmap.setPixel(10, 300, color);
    }

    @Test
    public void testSetPixel() {
        int color = 0xff << 24;
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        // normal case
        mBitmap.setPixel(10, 16, color);
        assertEquals(color, mBitmap.getPixel(10, 16));
    }

    @Test(expected=IllegalStateException.class)
    public void testSetPixelsOnRecycled() {
        int[] colors = createColors(100);

        mBitmap.recycle();
        mBitmap.setPixels(colors, 0, 0, 0, 0, 0, 0);
    }

    @Test(expected=IllegalStateException.class)
    public void testSetPixelsOnImmutable() {
        int[] colors = createColors(100);
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);

        mBitmap.setPixels(colors, 0, 0, 0, 0, 0, 0);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelsXYNegative() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: x and/or y less than 0
        mBitmap.setPixels(colors, 0, 0, -1, -1, 200, 16);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelsWidthHeightNegative() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: width and/or height less than 0
        mBitmap.setPixels(colors, 0, 0, 0, 0, -1, -1);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelsXTooHigh() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: (x + width) bigger than the source bitmap's width
        mBitmap.setPixels(colors, 0, 0, 10, 10, 95, 50);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelsYTooHigh() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: (y + height) bigger than the source bitmap's height
        mBitmap.setPixels(colors, 0, 0, 10, 10, 50, 95);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetPixelsStrideIllegal() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: stride less than width and bigger than -width
        mBitmap.setPixels(colors, 0, 10, 10, 10, 50, 50);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testSetPixelsOffsetNegative() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: offset less than 0
        mBitmap.setPixels(colors, -1, 50, 10, 10, 50, 50);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testSetPixelsOffsetTooBig() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: (offset + width) bigger than the length of colors
        mBitmap.setPixels(colors, 60, 50, 10, 10, 50, 50);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testSetPixelsLastScanlineNegative() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: lastScanline less than 0
        mBitmap.setPixels(colors, 10, -50, 10, 10, 50, 50);
    }

    @Test(expected=ArrayIndexOutOfBoundsException.class)
    public void testSetPixelsLastScanlineTooBig() {
        int[] colors = createColors(100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // abnormal case: (lastScanline + width) bigger than the length of colors
        mBitmap.setPixels(colors, 10, 50, 10, 10, 50, 50);
    }

    @Test
    public void testSetPixels() {
        int[] colors = createColors(100 * 100);
        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmap.setPixels(colors, 0, 100, 0, 0, 100, 100);
        int[] ret = new int[100 * 100];
        mBitmap.getPixels(ret, 0, 100, 0, 0, 100, 100);

        for(int i = 0; i < 10000; i++){
            assertEquals(ret[i], colors[i]);
        }
    }

    private void verifyPremultipliedBitmapConfig(Config config, boolean expectedPremul) {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, config);
        bitmap.setPremultiplied(true);
        bitmap.setPixel(0, 0, Color.TRANSPARENT);
        assertTrue(bitmap.isPremultiplied() == expectedPremul);

        bitmap.setHasAlpha(false);
        assertFalse(bitmap.isPremultiplied());
    }

    @Test
    public void testSetPremultipliedSimple() {
        verifyPremultipliedBitmapConfig(Bitmap.Config.ALPHA_8, true);
        verifyPremultipliedBitmapConfig(Bitmap.Config.RGB_565, false);
        verifyPremultipliedBitmapConfig(Bitmap.Config.ARGB_4444, true);
        verifyPremultipliedBitmapConfig(Bitmap.Config.ARGB_8888, true);
    }

    @Test
    public void testSetPremultipliedData() {
        // with premul, will store 2,2,2,2, so it doesn't get value correct
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.setPixel(0, 0, PREMUL_COLOR);
        assertEquals(bitmap.getPixel(0, 0), PREMUL_ROUNDED_COLOR);

        // read premultiplied value directly
        bitmap.setPremultiplied(false);
        assertEquals(bitmap.getPixel(0, 0), PREMUL_STORED_COLOR);

        // value can now be stored/read correctly
        bitmap.setPixel(0, 0, PREMUL_COLOR);
        assertEquals(bitmap.getPixel(0, 0), PREMUL_COLOR);

        // verify with array methods
        int testArray[] = new int[] { PREMUL_COLOR };
        bitmap.setPixels(testArray, 0, 1, 0, 0, 1, 1);
        bitmap.getPixels(testArray, 0, 1, 0, 0, 1, 1);
        assertEquals(bitmap.getPixel(0, 0), PREMUL_COLOR);
    }

    @Test
    public void testPremultipliedCanvas() {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.setHasAlpha(true);
        bitmap.setPremultiplied(false);
        assertFalse(bitmap.isPremultiplied());

        Canvas c = new Canvas();
        try {
            c.drawBitmap(bitmap, 0, 0, null);
            fail("canvas should fail with exception");
        } catch (RuntimeException e) {
        }
    }

    private int getBitmapRawInt(Bitmap bitmap) {
        IntBuffer buffer = IntBuffer.allocate(1);
        bitmap.copyPixelsToBuffer(buffer);
        return buffer.get(0);
    }

    private void bitmapStoreRawInt(Bitmap bitmap, int value) {
        IntBuffer buffer = IntBuffer.allocate(1);
        buffer.put(0, value);
        bitmap.copyPixelsFromBuffer(buffer);
    }

    @Test
    public void testSetPremultipliedToBuffer() {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.setPixel(0, 0, PREMUL_COLOR);
        int storedPremul = getBitmapRawInt(bitmap);

        bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.setPremultiplied(false);
        bitmap.setPixel(0, 0, PREMUL_STORED_COLOR);

        assertEquals(getBitmapRawInt(bitmap), storedPremul);
    }

    @Test
    public void testSetPremultipliedFromBuffer() {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.setPremultiplied(false);
        bitmap.setPixel(0, 0, PREMUL_COLOR);
        int rawTestColor = getBitmapRawInt(bitmap);

        bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.setPremultiplied(false);
        bitmapStoreRawInt(bitmap, rawTestColor);
        assertEquals(bitmap.getPixel(0, 0), PREMUL_COLOR);
    }

    @Test
    public void testSetWidth() {
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
        int alloc = mBitmap.getAllocationByteCount();

        // test shrinking
        mBitmap.setWidth(50);
        assertEquals(mBitmap.getAllocationByteCount(), alloc);
        assertEquals(mBitmap.getByteCount() * 2, alloc);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testSetWidthExpanding() {
        // test expanding
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

        mBitmap.setWidth(101);
    }

    @Test(expected=IllegalStateException.class)
    public void testSetWidthMutable() {
        // test mutable
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);

        mBitmap.setWidth(1);
    }

    @Test(expected=IllegalStateException.class)
    public void testWriteToParcelRecycled() {
        mBitmap.recycle();

        mBitmap.writeToParcel(null, 0);
    }

    @Test
    public void testWriteToParcel() {
        // abnormal case: failed to unparcel Bitmap
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, mOptions);
        Parcel p = Parcel.obtain();
        mBitmap.writeToParcel(p, 0);

        try {
            Bitmap.CREATOR.createFromParcel(p);
            fail("shouldn't come to here");
        } catch(RuntimeException e){
        }

        p.recycle();
        // normal case
        p = Parcel.obtain();
        mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        mBitmap.writeToParcel(p, 0);
        p.setDataPosition(0);
        assertTrue(mBitmap.sameAs(Bitmap.CREATOR.createFromParcel(p)));

        p.recycle();
    }

    /**
     * Although not specified as required behavior, it's something that some apps appear
     * to rely upon when sending bitmaps between themselves
     */
    @Test
    public void testWriteToParcelPreserveMutability() {
        Bitmap source = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        assertTrue(source.isMutable());
        Parcel p = Parcel.obtain();
        source.writeToParcel(p, 0);
        p.setDataPosition(0);
        Bitmap result = Bitmap.CREATOR.createFromParcel(p);
        p.recycle();
        assertTrue(result.isMutable());
    }

    @Test
    public void testWriteToParcelPreserveImmutability() {
        // Kinda silly way to create an immutable bitmap but it works
        Bitmap source = Bitmap.createBitmap(100, 100, Config.ARGB_8888)
                .copy(Config.ARGB_8888, false);
        assertFalse(source.isMutable());
        Parcel p = Parcel.obtain();
        source.writeToParcel(p, 0);
        p.setDataPosition(0);
        Bitmap result = Bitmap.CREATOR.createFromParcel(p);
        p.recycle();
        assertFalse(result.isMutable());
    }

    @Test
    public void testWriteHwBitmapToParcel() {
        mBitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        Parcel p = Parcel.obtain();
        mBitmap.writeToParcel(p, 0);
        p.setDataPosition(0);
        Bitmap expectedBitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot);
        assertTrue(expectedBitmap.sameAs(Bitmap.CREATOR.createFromParcel(p)));

        p.recycle();
    }

    @Test
    public void testParcelF16ColorSpace() {
        for (ColorSpace.Named e : new ColorSpace.Named[] {
                ColorSpace.Named.EXTENDED_SRGB,
                ColorSpace.Named.LINEAR_EXTENDED_SRGB,
                ColorSpace.Named.PRO_PHOTO_RGB,
                ColorSpace.Named.DISPLAY_P3
        }) {
            final ColorSpace cs = ColorSpace.get(e);
            Bitmap b = Bitmap.createBitmap(10, 10, Config.RGBA_F16, true, cs);
            assertSame(cs, b.getColorSpace());

            Parcel p = Parcel.obtain();
            b.writeToParcel(p, 0);
            p.setDataPosition(0);
            Bitmap unparceled = Bitmap.CREATOR.createFromParcel(p);
            assertSame(cs, unparceled.getColorSpace());
        }
    }

    @Test
    public void testGetScaledHeight1() {
        int dummyDensity = 5;
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        int scaledHeight = scaleFromDensity(ret.getHeight(), ret.getDensity(), dummyDensity);
        assertNotNull(ret);
        assertEquals(scaledHeight, ret.getScaledHeight(dummyDensity));
    }

    @Test
    public void testGetScaledHeight2() {
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        DisplayMetrics metrics =
                InstrumentationRegistry.getTargetContext().getResources().getDisplayMetrics();
        int scaledHeight = scaleFromDensity(ret.getHeight(), ret.getDensity(), metrics.densityDpi);
        assertEquals(scaledHeight, ret.getScaledHeight(metrics));
    }

    @Test
    public void testGetScaledHeight3() {
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        Bitmap mMutableBitmap = Bitmap.createBitmap(100, 200, Config.ARGB_8888);
        Canvas mCanvas = new Canvas(mMutableBitmap);
        // set Density
        mCanvas.setDensity(DisplayMetrics.DENSITY_HIGH);
        int scaledHeight = scaleFromDensity(
                ret.getHeight(), ret.getDensity(), mCanvas.getDensity());
        assertEquals(scaledHeight, ret.getScaledHeight(mCanvas));
    }

    @Test
    public void testGetScaledWidth1() {
        int dummyDensity = 5;
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        int scaledWidth = scaleFromDensity(ret.getWidth(), ret.getDensity(), dummyDensity);
        assertNotNull(ret);
        assertEquals(scaledWidth, ret.getScaledWidth(dummyDensity));
    }

    @Test
    public void testGetScaledWidth2() {
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        DisplayMetrics metrics =
                InstrumentationRegistry.getTargetContext().getResources().getDisplayMetrics();
        int scaledWidth = scaleFromDensity(ret.getWidth(), ret.getDensity(), metrics.densityDpi);
        assertEquals(scaledWidth, ret.getScaledWidth(metrics));
    }

    @Test
    public void testGetScaledWidth3() {
        Bitmap ret = Bitmap.createBitmap(100, 200, Config.RGB_565);
        Bitmap mMutableBitmap = Bitmap.createBitmap(100, 200, Config.ARGB_8888);
        Canvas mCanvas = new Canvas(mMutableBitmap);
        // set Density
        mCanvas.setDensity(DisplayMetrics.DENSITY_HIGH);
        int scaledWidth = scaleFromDensity(ret.getWidth(), ret.getDensity(),  mCanvas.getDensity());
        assertEquals(scaledWidth, ret.getScaledWidth(mCanvas));
    }

    @Test
    public void testSameAs_simpleSuccess() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);
        assertTrue(bitmap1.sameAs(bitmap2));
        assertTrue(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_simpleFail() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);
        bitmap2.setPixel(20, 10, Color.WHITE);
        assertFalse(bitmap1.sameAs(bitmap2));
        assertFalse(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_reconfigure() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(150, 150, Config.ARGB_8888);
        bitmap2.reconfigure(100, 100, Config.ARGB_8888); // now same size, so should be same
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);
        assertTrue(bitmap1.sameAs(bitmap2));
        assertTrue(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_config() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 200, Config.RGB_565);
        Bitmap bitmap2 = Bitmap.createBitmap(100, 200, Config.ARGB_8888);

        // both bitmaps can represent black perfectly
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);

        // but not same due to config
        assertFalse(bitmap1.sameAs(bitmap2));
        assertFalse(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_width() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(101, 100, Config.ARGB_8888);
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);
        assertFalse(bitmap1.sameAs(bitmap2));
        assertFalse(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_height() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(102, 100, Config.ARGB_8888);
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);
        assertFalse(bitmap1.sameAs(bitmap2));
        assertFalse(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_opaque() {
        Bitmap bitmap1 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        bitmap1.eraseColor(Color.BLACK);
        bitmap2.eraseColor(Color.BLACK);
        bitmap1.setHasAlpha(true);
        bitmap2.setHasAlpha(false);
        assertFalse(bitmap1.sameAs(bitmap2));
        assertFalse(bitmap2.sameAs(bitmap1));
    }

    @Test
    public void testSameAs_hardware() {
        Bitmap bitmap1 = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        Bitmap bitmap2 = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        Bitmap bitmap3 = BitmapFactory.decodeResource(mRes, R.drawable.robot);
        Bitmap bitmap4 = BitmapFactory.decodeResource(mRes, R.drawable.start, HARDWARE_OPTIONS);
        assertTrue(bitmap1.sameAs(bitmap2));
        assertTrue(bitmap2.sameAs(bitmap1));
        assertFalse(bitmap1.sameAs(bitmap3));
        assertFalse(bitmap1.sameAs(bitmap4));
    }

    @Test
    public void testSameAs_wrappedHardwareBuffer() {
        try (HardwareBuffer hwBufferA = createTestBuffer(512, 512, true);
             HardwareBuffer hwBufferB = createTestBuffer(512, 512, true);
             HardwareBuffer hwBufferC = createTestBuffer(512, 512, true);) {
            // Fill buffer C with generated data
            nFillRgbaHwBuffer(hwBufferC);

            // Create the test bitmaps
            Bitmap bitmap1 = Bitmap.wrapHardwareBuffer(hwBufferA, ColorSpace.get(Named.SRGB));
            Bitmap bitmap2 = Bitmap.wrapHardwareBuffer(hwBufferA, ColorSpace.get(Named.SRGB));
            Bitmap bitmap3 = BitmapFactory.decodeResource(mRes, R.drawable.robot);
            Bitmap bitmap4 = Bitmap.wrapHardwareBuffer(hwBufferB, ColorSpace.get(Named.SRGB));
            Bitmap bitmap5 = Bitmap.wrapHardwareBuffer(hwBufferC, ColorSpace.get(Named.SRGB));

            // Run the compare-a-thon
            assertTrue(bitmap1.sameAs(bitmap2));  // SAME UNDERLYING BUFFER
            assertTrue(bitmap2.sameAs(bitmap1));  // SAME UNDERLYING BUFFER
            assertFalse(bitmap1.sameAs(bitmap3)); // HW vs. NON-HW
            assertTrue(bitmap1.sameAs(bitmap4));  // DIFFERENT BUFFERS, SAME CONTENT
            assertFalse(bitmap1.sameAs(bitmap5)); // DIFFERENT BUFFERS, DIFFERENT CONTENT
        }
    }

    @Test(expected=IllegalStateException.class)
    public void testHardwareGetPixel() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.getPixel(0, 0);
    }

    @Test(expected=IllegalStateException.class)
    public void testHardwareGetPixels() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.getPixels(new int[5], 0, 5, 0, 0, 5, 1);
    }

    @Test
    public void testGetConfigOnRecycled() {
        Bitmap bitmap1 = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap1.recycle();
        assertEquals(Config.HARDWARE, bitmap1.getConfig());
        Bitmap bitmap2 = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
        bitmap2.recycle();
        assertEquals(Config.ARGB_8888, bitmap2.getConfig());
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareSetWidth() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.setWidth(30);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareSetHeight() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.setHeight(30);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareSetConfig() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.setConfig(Config.ARGB_8888);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareReconfigure() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.reconfigure(30, 30, Config.ARGB_8888);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareSetPixels() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.setPixels(new int[10], 0, 1, 0, 0, 1, 1);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareSetPixel() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.setPixel(1, 1, 0);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareEraseColor() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.eraseColor(0);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareEraseColorLong() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, HARDWARE_OPTIONS);
        bitmap.eraseColor(Color.pack(0));
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareCopyPixelsToBuffer() {
        Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, HARDWARE_OPTIONS);
        ByteBuffer byteBuf = ByteBuffer.allocate(bitmap.getRowBytes() * bitmap.getHeight());
        bitmap.copyPixelsToBuffer(byteBuf);
    }

    @Test(expected = IllegalStateException.class)
    public void testHardwareCopyPixelsFromBuffer() {
        IntBuffer intBuf1 = IntBuffer.allocate(mBitmap.getRowBytes() * mBitmap.getHeight());
        assertEquals(0, intBuf1.position());
        mBitmap.copyPixelsToBuffer(intBuf1);
        Bitmap hwBitmap = BitmapFactory.decodeResource(mRes, R.drawable.start, HARDWARE_OPTIONS);
        hwBitmap.copyPixelsFromBuffer(intBuf1);
    }

    @Test
    public void testUseMetadataAfterRecycle() {
        Bitmap bitmap = Bitmap.createBitmap(10, 20, Config.RGB_565);
        bitmap.recycle();
        assertEquals(10, bitmap.getWidth());
        assertEquals(20, bitmap.getHeight());
        assertEquals(Config.RGB_565, bitmap.getConfig());
    }

    @Test
    public void testCopyHWBitmapInStrictMode() {
        strictModeTest(()->{
            Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            Bitmap hwBitmap = bitmap.copy(Config.HARDWARE, false);
            hwBitmap.copy(Config.ARGB_8888, false);
        });
    }

    @Test
    public void testCreateScaledFromHWInStrictMode() {
        strictModeTest(()->{
            Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            Bitmap hwBitmap = bitmap.copy(Config.HARDWARE, false);
            Bitmap.createScaledBitmap(hwBitmap, 200, 200, false);
        });
    }

    @Test
    public void testExtractAlphaFromHWInStrictMode() {
        strictModeTest(()->{
            Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            Bitmap hwBitmap = bitmap.copy(Config.HARDWARE, false);
            hwBitmap.extractAlpha();
        });
    }

    @Test
    public void testCompressInStrictMode() {
        strictModeTest(()->{
            Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            bitmap.compress(CompressFormat.JPEG, 90, new ByteArrayOutputStream());
        });
    }

    @Test
    public void testParcelHWInStrictMode() {
        strictModeTest(()->{
            mBitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            Bitmap hwBitmap = mBitmap.copy(Config.HARDWARE, false);
            hwBitmap.writeToParcel(Parcel.obtain(), 0);
        });
    }

    @Test
    public void testSameAsFirstHWInStrictMode() {
        strictModeTest(()->{
            Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            Bitmap hwBitmap = bitmap.copy(Config.HARDWARE, false);
            hwBitmap.sameAs(bitmap);
        });
    }

    @Test
    public void testSameAsSecondHWInStrictMode() {
        strictModeTest(()->{
            Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.ARGB_8888);
            Bitmap hwBitmap = bitmap.copy(Config.HARDWARE, false);
            bitmap.sameAs(hwBitmap);
        });
    }

    @Test
    public void testNdkAccessAfterRecycle() {
        Bitmap bitmap = Bitmap.createBitmap(10, 20, Config.RGB_565);
        Bitmap hardware = bitmap.copy(Config.HARDWARE, false);
        nValidateBitmapInfo(bitmap, 10, 20, true);
        nValidateBitmapInfo(hardware, 10, 20, true);

        bitmap.recycle();
        hardware.recycle();

        nValidateBitmapInfo(bitmap, 10, 20, true);
        nValidateBitmapInfo(hardware, 10, 20, true);
        nValidateNdkAccessFails(bitmap);
    }

    @Test
    public void bitmapIsMutable() {
        Bitmap b = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        assertTrue("CreateBitmap w/ params should be mutable", b.isMutable());
        assertTrue("CreateBitmap from bitmap should be mutable",
                Bitmap.createBitmap(b).isMutable());
    }

    private static void runGcAndFinalizersSync() {
        Runtime.getRuntime().gc();
        Runtime.getRuntime().runFinalization();

        final CountDownLatch fence = new CountDownLatch(1);
        new Object() {
            @Override
            protected void finalize() throws Throwable {
                try {
                    fence.countDown();
                } finally {
                    super.finalize();
                }
            }
        };
        try {
            do {
                Runtime.getRuntime().gc();
                Runtime.getRuntime().runFinalization();
            } while (!fence.await(100, TimeUnit.MILLISECONDS));
        } catch (InterruptedException ex) {
            throw new RuntimeException(ex);
        }
    }

    private static File sProcSelfFd = new File("/proc/self/fd");
    private static int getFdCount() {
        return sProcSelfFd.listFiles().length;
    }

    private static void assertNotLeaking(int iteration,
            Debug.MemoryInfo start, Debug.MemoryInfo end) {
        Debug.getMemoryInfo(end);
        assertNotEquals(0, start.getTotalPss());
        assertNotEquals(0, end.getTotalPss());
        if (end.getTotalPss() - start.getTotalPss() > 2000 /* kB */) {
            runGcAndFinalizersSync();
            Debug.getMemoryInfo(end);
            if (end.getTotalPss() - start.getTotalPss() > 4000 /* kB */) {
                // Guarded by if so we don't continually generate garbage for the
                // assertion string.
                assertEquals("Memory leaked, iteration=" + iteration,
                        start.getTotalPss(), end.getTotalPss(),
                        4000 /* kb */);
            }
        }
    }

    private static void runNotLeakingTest(Runnable test) {
        Debug.MemoryInfo meminfoStart = new Debug.MemoryInfo();
        Debug.MemoryInfo meminfoEnd = new Debug.MemoryInfo();
        int fdCount = -1;
        // Do a warmup to reach steady-state memory usage
        for (int i = 0; i < 50; i++) {
            test.run();
        }
        runGcAndFinalizersSync();
        Debug.getMemoryInfo(meminfoStart);
        fdCount = getFdCount();
        // Now run the test
        for (int i = 0; i < 2000; i++) {
            if (i % 100 == 5) {
                assertNotLeaking(i, meminfoStart, meminfoEnd);
                final int curFdCount = getFdCount();
                if (curFdCount - fdCount > 10) {
                    fail(String.format("FDs leaked. Expected=%d, current=%d, iteration=%d",
                            fdCount, curFdCount, i));
                }
            }
            test.run();
        }
        assertNotLeaking(2000, meminfoStart, meminfoEnd);
        final int curFdCount = getFdCount();
        if (curFdCount - fdCount > 10) {
            fail(String.format("FDs leaked. Expected=%d, current=%d", fdCount, curFdCount));
        }
    }

    @Test
    @LargeTest
    public void testHardwareBitmapNotLeaking() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredConfig = Config.HARDWARE;
        opts.inScaled = false;

        runNotLeakingTest(() -> {
            Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, opts);
            assertNotNull(bitmap);
            // Make sure nothing messed with the bitmap
            assertEquals(128, bitmap.getWidth());
            assertEquals(128, bitmap.getHeight());
            assertEquals(Config.HARDWARE, bitmap.getConfig());
            bitmap.recycle();
        });
    }

    @Test
    @LargeTest
    public void testWrappedHardwareBufferBitmapNotLeaking() {
        final ColorSpace colorSpace = ColorSpace.get(Named.SRGB);
        try (HardwareBuffer hwBuffer = createTestBuffer(1024, 512, false)) {
            runNotLeakingTest(() -> {
                Bitmap bitmap = Bitmap.wrapHardwareBuffer(hwBuffer, colorSpace);
                assertNotNull(bitmap);
                // Make sure nothing messed with the bitmap
                assertEquals(1024, bitmap.getWidth());
                assertEquals(512, bitmap.getHeight());
                assertEquals(Config.HARDWARE, bitmap.getConfig());
                bitmap.recycle();
            });
        }
    }

    @Test
    @LargeTest
    public void testDrawingHardwareBitmapNotLeaking() {
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inPreferredConfig = Config.HARDWARE;
        opts.inScaled = false;
        RenderTarget renderTarget = RenderTarget.create();
        renderTarget.setDefaultSize(128, 128);
        final Surface surface = renderTarget.getSurface();

        runNotLeakingTest(() -> {
            Bitmap bitmap = BitmapFactory.decodeResource(mRes, R.drawable.robot, opts);
            assertNotNull(bitmap);
            // Make sure nothing messed with the bitmap
            assertEquals(128, bitmap.getWidth());
            assertEquals(128, bitmap.getHeight());
            assertEquals(Config.HARDWARE, bitmap.getConfig());
            Canvas canvas = surface.lockHardwareCanvas();
            canvas.drawBitmap(bitmap, 0, 0, null);
            surface.unlockCanvasAndPost(canvas);
            bitmap.recycle();
        });
        renderTarget.destroy();
    }

    @Test
    public void testWrapHardwareBufferHoldsReference() {
        Bitmap bitmap;
        // Create hardware-buffer and wrap it in a Bitmap
        try (HardwareBuffer hwBuffer = createTestBuffer(128, 128, true)) {
            // Fill buffer with colors (x, y, 42, 255)
            nFillRgbaHwBuffer(hwBuffer);
            bitmap = Bitmap.wrapHardwareBuffer(hwBuffer, ColorSpace.get(Named.SRGB));
        }

        // Buffer is closed at this point. Ensure bitmap still works by drawing it
        assertEquals(128, bitmap.getWidth());
        assertEquals(128, bitmap.getHeight());
        assertEquals(Config.HARDWARE, bitmap.getConfig());

        // Copy bitmap to target bitmap we can read from
        Bitmap dstBitmap = bitmap.copy(Config.ARGB_8888, false);
        bitmap.recycle();

        // Ensure that the bitmap has valid contents
        int pixel = dstBitmap.getPixel(0, 0);
        assertEquals(255 << 24 | 42, pixel);
        dstBitmap.recycle();
    }

    @Test
    public void testWrapHardwareBufferPreservesColors() {
        try (HardwareBuffer hwBuffer = createTestBuffer(128, 128, true)) {
            // Fill buffer with colors (x, y, 42, 255)
            nFillRgbaHwBuffer(hwBuffer);

            // Create HW bitmap from this buffer
            Bitmap srcBitmap = Bitmap.wrapHardwareBuffer(hwBuffer, ColorSpace.get(Named.SRGB));
            assertNotNull(srcBitmap);

            // Copy it to target non-HW bitmap
            Bitmap dstBitmap = srcBitmap.copy(Config.ARGB_8888, false);
            srcBitmap.recycle();

            // Ensure all colors are as expected (matches the nFillRgbaHwBuffer call used above).
            for (int y = 0; y < 128; ++y) {
                for (int x = 0; x < 128; ++x) {
                    int pixel = dstBitmap.getPixel(x, y);
                    short a = 255;
                    short r = (short) (x % 255);
                    short g = (short) (y % 255);
                    short b = 42;
                    assertEquals(a << 24 | r << 16 | g << 8 | b, pixel);
                }
            }
            dstBitmap.recycle();
        }
    }

    @Test
    public void testNdkFormats() {
        for (ConfigToFormat pair : CONFIG_TO_FORMAT) {
            Bitmap bm = Bitmap.createBitmap(10, 10, pair.config);
            assertNotNull(bm);
            int nativeFormat = nGetFormat(bm);
            assertEquals("Config: " + pair.config, pair.format, nativeFormat);
        }
    }

    @Test
    public void testNdkFormatsHardware() {
        for (ConfigToFormat pair : CONFIG_TO_FORMAT) {
            Bitmap bm = Bitmap.createBitmap(10, 10, pair.config);
            bm = bm.copy(Bitmap.Config.HARDWARE, false);

            // ALPHA_8 is not supported in HARDWARE.
            if (bm == null) {
                assertEquals(Bitmap.Config.ALPHA_8, pair.config);
                continue;
            }
            assertNotEquals(Bitmap.Config.ALPHA_8, pair.config);

            int nativeFormat = nGetFormat(bm);
            if (pair.config == Bitmap.Config.RGBA_F16) {
                // It is possible the system does not support RGBA_F16 in HARDWARE.
                // In that case, it will fall back to ARGB_8888.
                assertTrue(nativeFormat == ANDROID_BITMAP_FORMAT_RGBA_8888
                        || nativeFormat == ANDROID_BITMAP_FORMAT_RGBA_F16);
            } else {
                assertEquals("Config: " + pair.config, pair.format, nativeFormat);
            }
        }
    }

    @Test
    public void testNullBitmapNdk() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        nTestNullBitmap(bitmap);
    }

    private Object[] parametersForTestNdkInfo() {
        return new Object[] {
            new Object[] { Config.ALPHA_8,   ANDROID_BITMAP_FORMAT_A_8  },
            new Object[] { Config.ARGB_8888, ANDROID_BITMAP_FORMAT_RGBA_8888 },
            new Object[] { Config.RGB_565,   ANDROID_BITMAP_FORMAT_RGB_565 },
            new Object[] { Config.RGBA_F16,  ANDROID_BITMAP_FORMAT_RGBA_F16 },
        };
    }

    @Test
    @Parameters(method = "parametersForTestNdkInfo")
    public void testNdkInfo(Config config, final int expectedFormat) {
        // Arbitrary width and height.
        final int width = 13;
        final int height = 7;
        boolean[] trueFalse = new boolean[] { true, false };
        for (boolean hasAlpha : trueFalse) {
            for (boolean premultiplied : trueFalse) {
                Bitmap bm = Bitmap.createBitmap(width, height, config, hasAlpha);
                bm.setPremultiplied(premultiplied);
                nTestInfo(bm, expectedFormat, width, height, bm.hasAlpha(),
                        bm.isPremultiplied(), false);
                Bitmap hwBitmap = bm.copy(Bitmap.Config.HARDWARE, false);
                if (config == Bitmap.Config.ALPHA_8) {
                    // ALPHA_8 is not supported in HARDWARE. b/141480329
                    assertNull(hwBitmap);
                } else {
                    assertNotNull(hwBitmap);

                    // Some devices do not support F16 + HARDWARE. These fall back to 8888, and can
                    // be identified by their use of SRGB instead of EXTENDED_SRGB.
                    int tempExpectedFormat = expectedFormat;
                    if (config == Config.RGBA_F16 && hwBitmap.getColorSpace() == ColorSpace.get(
                            ColorSpace.Named.SRGB)) {
                        tempExpectedFormat = ANDROID_BITMAP_FORMAT_RGBA_8888;
                    }
                    nTestInfo(hwBitmap, tempExpectedFormat, width, height, hwBitmap.hasAlpha(),
                            hwBitmap.isPremultiplied(), true);
                    hwBitmap.recycle();
                }
                bm.recycle();
            }
        }
    }

    @Test
    public void testNdkDataSpaceF16Extended() {
        // In RGBA_F16 we force EXTENDED in these cases.
        for (ColorSpace colorSpace : new ColorSpace[] {
                ColorSpace.get(Named.SRGB),
                ColorSpace.get(Named.EXTENDED_SRGB),
        }) {
            Bitmap bm = Bitmap.createBitmap(10, 10, Config.RGBA_F16, false, colorSpace);
            assertNotNull(bm);

            assertEquals(ColorSpace.get(Named.EXTENDED_SRGB), bm.getColorSpace());
            assertEquals(DataSpace.ADATASPACE_SCRGB, nGetDataSpace(bm));
        }

        for (ColorSpace colorSpace : new ColorSpace[] {
                ColorSpace.get(Named.LINEAR_SRGB),
                ColorSpace.get(Named.LINEAR_EXTENDED_SRGB),
        }) {
            Bitmap bm = Bitmap.createBitmap(10, 10, Config.RGBA_F16, false, colorSpace);
            assertNotNull(bm);

            assertEquals(ColorSpace.get(Named.LINEAR_EXTENDED_SRGB), bm.getColorSpace());
            assertEquals(DataSpace.ADATASPACE_SCRGB_LINEAR, nGetDataSpace(bm));
        }
    }

    @Test
    public void testNdkDataSpaceNonExtended() {
        // In 565 and 8888, these force non-extended.
        for (ColorSpace colorSpace : new ColorSpace[] {
                ColorSpace.get(Named.SRGB),
                ColorSpace.get(Named.EXTENDED_SRGB),
        }) {
            for (Config c: new Config[] { Config.ARGB_8888, Config.RGB_565 }) {
                Bitmap bm = Bitmap.createBitmap(10, 10, c, false, colorSpace);
                assertNotNull(bm);

                assertEquals(ColorSpace.get(Named.SRGB), bm.getColorSpace());
                assertEquals(DataSpace.ADATASPACE_SRGB, nGetDataSpace(bm));
            }
        }

        for (ColorSpace colorSpace : new ColorSpace[] {
                ColorSpace.get(Named.LINEAR_SRGB),
                ColorSpace.get(Named.LINEAR_EXTENDED_SRGB),
        }) {
            for (Config c: new Config[] { Config.ARGB_8888, Config.RGB_565 }) {
                Bitmap bm = Bitmap.createBitmap(10, 10, c, false, colorSpace);
                assertNotNull(bm);

                assertEquals(ColorSpace.get(Named.LINEAR_SRGB), bm.getColorSpace());
                assertEquals(DataSpace.ADATASPACE_SRGB_LINEAR, nGetDataSpace(bm));
            }
        }
    }

    @Test
    public void testNdkDataSpace() {
        // DataSpace.ADATASPACEs that do not depend on the Config.
        for (ColorSpace colorSpace : new ColorSpace[] {
                // These have corresponding DataSpace.ADATASPACEs that are independent of the Config
                ColorSpace.get(Named.DISPLAY_P3),
                ColorSpace.get(Named.BT2020),
                ColorSpace.get(Named.ADOBE_RGB),
                ColorSpace.get(Named.BT709),
                ColorSpace.get(Named.DCI_P3),

                // These have no public ADATASPACE.
                ColorSpace.get(Named.ACES),
                ColorSpace.get(Named.ACESCG),
                ColorSpace.get(Named.NTSC_1953),
                ColorSpace.get(Named.PRO_PHOTO_RGB),
                ColorSpace.get(Named.SMPTE_C),
        }) {
            for (Config c: new Config[] { Config.ARGB_8888, Config.RGB_565, Config.RGBA_F16 }) {
                Bitmap bm = Bitmap.createBitmap(10, 10, c, false, colorSpace);
                assertNotNull(bm);

                int dataSpace = nGetDataSpace(bm);
                assertEquals("Bitmap with " + c + " and " + bm.getColorSpace()
                        + " has unexpected data space", DataSpace.fromColorSpace(colorSpace),
                        dataSpace);
            }
        }
    }

    @Test
    public void testNdkDataSpaceAlpha8() {
        // ALPHA_8 doesn't support ColorSpaces
        Bitmap bm = Bitmap.createBitmap(10, 10, Config.ALPHA_8);
        assertNotNull(bm);
        assertNull(bm.getColorSpace());
        int dataSpace = nGetDataSpace(bm);
        assertEquals(DataSpace.ADATASPACE_UNKNOWN, dataSpace);
    }

    @Test
    public void testNdkDataSpaceNullBitmap() {
        assertEquals(DataSpace.ADATASPACE_UNKNOWN, nGetDataSpace(null));
    }

    private static native int nGetDataSpace(Bitmap bm);

    // These match the NDK APIs.
    private static final int ANDROID_BITMAP_COMPRESS_FORMAT_JPEG = 0;
    private static final int ANDROID_BITMAP_COMPRESS_FORMAT_PNG = 1;
    private static final int ANDROID_BITMAP_COMPRESS_FORMAT_WEBP_LOSSY = 3;
    private static final int ANDROID_BITMAP_COMPRESS_FORMAT_WEBP_LOSSLESS = 4;

    private int nativeCompressFormat(CompressFormat format) {
        switch (format) {
            case JPEG:
                return ANDROID_BITMAP_COMPRESS_FORMAT_JPEG;
            case PNG:
                return ANDROID_BITMAP_COMPRESS_FORMAT_PNG;
            case WEBP_LOSSY:
                return ANDROID_BITMAP_COMPRESS_FORMAT_WEBP_LOSSY;
            case WEBP_LOSSLESS:
                return ANDROID_BITMAP_COMPRESS_FORMAT_WEBP_LOSSLESS;
            default:
                fail("format " + format + " has no corresponding native compress format!");
                return -1;
        }
    }

    private static Object[] parametersForNdkCompress() {
        // Skip WEBP, which has no corresponding native compress format.
        Object[] formats = new Object[] {
                CompressFormat.JPEG,
                CompressFormat.PNG,
                CompressFormat.WEBP_LOSSY,
                CompressFormat.WEBP_LOSSLESS,
        };
        // These are the ColorSpaces with corresponding ADataSpaces
        Object[] colorSpaces = new Object[] {
                ColorSpace.get(Named.SRGB),
                ColorSpace.get(Named.EXTENDED_SRGB),
                ColorSpace.get(Named.LINEAR_SRGB),
                ColorSpace.get(Named.LINEAR_EXTENDED_SRGB),

                ColorSpace.get(Named.DISPLAY_P3),
                ColorSpace.get(Named.DCI_P3),
                ColorSpace.get(Named.BT2020),
                ColorSpace.get(Named.BT709),
                ColorSpace.get(Named.ADOBE_RGB),
        };

        Object[] configs = new Object[] {
                Config.ARGB_8888,
                Config.RGB_565,
                Config.RGBA_F16,
        };

        return crossProduct(formats, colorSpaces, configs);
    }

    private static Object[] crossProduct(Object[] a, Object[] b, Object[] c) {
        final int length = a.length * b.length * c.length;
        Object[] ret = new Object[length];
        for (int i = 0; i < a.length; i++) {
            for (int j = 0; j < b.length; j++) {
                for (int k = 0; k < c.length; k++) {
                    int index = i * (b.length * c.length) + j * c.length + k;
                    assertNull(ret[index]);
                    ret[index] = new Object[] { a[i], b[j], c[k] };
                }
            }
        }
        return ret;
    }

    private static boolean isSrgb(ColorSpace cs) {
        return cs == ColorSpace.get(Named.SRGB)
                || cs == ColorSpace.get(Named.EXTENDED_SRGB)
                || cs == ColorSpace.get(Named.LINEAR_SRGB)
                || cs == ColorSpace.get(Named.LINEAR_EXTENDED_SRGB);
    }

    @Test
    @Parameters(method = "parametersForNdkCompress")
    public void testNdkCompress(CompressFormat format, ColorSpace cs, Config config)
            throws IOException {
        // Verify that ndk compress behaves the same as Bitmap#compress
        Bitmap bitmap = Bitmap.createBitmap(10, 10, config, true /* hasAlpha */, cs);
        assertNotNull(bitmap);

        {
            // Use different colors and alphas.
            Canvas canvas = new Canvas(bitmap);
            long color0 = Color.pack(0, 0, 1, 1, cs);
            long color1 = Color.pack(1, 0, 0, 0, cs);
            LinearGradient gradient = new LinearGradient(0, 0, 10, 10, color0, color1,
                    Shader.TileMode.CLAMP);
            Paint paint = new Paint();
            paint.setShader(gradient);
            canvas.drawPaint(paint);
        }

        byte[] storage = new byte[16 * 1024];
        for (int quality : new int[] { 50, 80, 100 }) {
            byte[] expected = null;
            try (ByteArrayOutputStream stream = new ByteArrayOutputStream()) {
                assertTrue("Failed to encode a Bitmap with " + cs + " to " + format + " at quality "
                        + quality + " from Java API", bitmap.compress(format, quality, stream));
                expected = stream.toByteArray();
            }

            try (ByteArrayOutputStream stream = new ByteArrayOutputStream()) {
                boolean success = nCompress(bitmap, nativeCompressFormat(format),
                        quality, stream, storage);
                assertTrue("Failed to encode pixels with " + cs + " to " + format + " at quality "
                        + quality + " from NDK API", success);
                byte[] actual = stream.toByteArray();

                if (isSrgb(cs)) {
                    if (!Arrays.equals(expected, actual)) {
                        fail("NDK compression did not match for " + cs + " and format " + format
                                + " at quality " + quality);
                    }
                } else {
                    // The byte arrays will match exactly for SRGB and its variants, because those
                    // are treated specially. For the others, there are some small differences
                    // between Skia's and ColorSpace's values that result in the ICC profiles being
                    // written slightly differently. They should still look the same, though.
                    Bitmap expectedBitmap = decodeBytes(expected);
                    Bitmap actualBitmap = decodeBytes(actual);
                    boolean matched = BitmapUtils.compareBitmapsMse(expectedBitmap, actualBitmap,
                              5, true, false);
                    expectedBitmap.recycle();
                    actualBitmap.recycle();
                    assertTrue("NDK compression did not match for " + cs + " and format " + format
                                + " at quality " + quality, matched);
                }
            }
        }
    }

    @Test
    public void testNdkCompressBadParameter() throws IOException {
        try (ByteArrayOutputStream stream = new ByteArrayOutputStream()) {
            nTestNdkCompressBadParameter(mBitmap, stream, new byte[16 * 1024]);
        }
    }

    private static native boolean nCompress(Bitmap bitmap, int format, int quality,
            OutputStream stream, byte[] storage);
    private static native void nTestNdkCompressBadParameter(Bitmap bitmap,
            OutputStream stream, byte[] storage);

    private void strictModeTest(Runnable runnable) {
        StrictMode.ThreadPolicy originalPolicy = StrictMode.getThreadPolicy();
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                .detectCustomSlowCalls().penaltyDeath().build());
        try {
            runnable.run();
            fail("Shouldn't reach it");
        } catch (RuntimeException expected){
            // expect to receive StrictModeViolation
        } finally {
            StrictMode.setThreadPolicy(originalPolicy);
        }
    }

    private static native void nValidateBitmapInfo(Bitmap bitmap, int width, int height,
            boolean is565);
    private static native void nValidateNdkAccessFails(Bitmap bitmap);

    private static native void nFillRgbaHwBuffer(HardwareBuffer hwBuffer);
    private static native void nTestNullBitmap(Bitmap bitmap);

    static final int ANDROID_BITMAP_FORMAT_RGBA_8888 = 1;
    private static final int ANDROID_BITMAP_FORMAT_RGB_565 = 4;
    private static final int ANDROID_BITMAP_FORMAT_A_8 = 8;
    private static final int ANDROID_BITMAP_FORMAT_RGBA_F16 = 9;

    private static class ConfigToFormat {
        public final Config config;
        public final int format;

        ConfigToFormat(Config c, int f) {
            this.config = c;
            this.format = f;
        }
    }

    private static final ConfigToFormat[] CONFIG_TO_FORMAT = new ConfigToFormat[] {
        new ConfigToFormat(Bitmap.Config.ARGB_8888, ANDROID_BITMAP_FORMAT_RGBA_8888),
        // ARGB_4444 is deprecated, and createBitmap converts to 8888.
        new ConfigToFormat(Bitmap.Config.ARGB_4444, ANDROID_BITMAP_FORMAT_RGBA_8888),
        new ConfigToFormat(Bitmap.Config.RGB_565, ANDROID_BITMAP_FORMAT_RGB_565),
        new ConfigToFormat(Bitmap.Config.ALPHA_8, ANDROID_BITMAP_FORMAT_A_8),
        new ConfigToFormat(Bitmap.Config.RGBA_F16, ANDROID_BITMAP_FORMAT_RGBA_F16),
    };

    static native int nGetFormat(Bitmap bitmap);

    private static native void nTestInfo(Bitmap bm, int androidBitmapFormat, int width, int height,
            boolean hasAlpha, boolean premultiplied, boolean hardware);

    private static HardwareBuffer createTestBuffer(int width, int height, boolean cpuAccess) {
        long usage = HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE;
        if (cpuAccess) {
            usage |= HardwareBuffer.USAGE_CPU_WRITE_RARELY;
        }
        // We can assume that RGBA_8888 format is supported for every platform.
        HardwareBuffer hwBuffer = HardwareBuffer.create(width, height, HardwareBuffer.RGBA_8888,
                1, usage);
        return hwBuffer;
    }

    private static int scaleFromDensity(int size, int sdensity, int tdensity) {
        if (sdensity == Bitmap.DENSITY_NONE || sdensity == tdensity) {
            return size;
        }

        // Scale by tdensity / sdensity, rounding up.
        return ((size * tdensity) + (sdensity >> 1)) / sdensity;
    }

    private static int[] createColors(int size) {
        int[] colors = new int[size];

        for (int i = 0; i < size; i++) {
            colors[i] = (0xFF << 24) | (i << 16) | (i << 8) | i;
        }

        return colors;
    }

    private static BitmapFactory.Options createHardwareBitmapOptions() {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Config.HARDWARE;
        return options;
    }
}
