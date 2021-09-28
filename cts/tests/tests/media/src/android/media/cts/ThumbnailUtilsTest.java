/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.annotation.ColorInt;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.media.ThumbnailUtils;
import android.platform.test.annotations.AppModeFull;
import android.util.Size;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

@AppModeFull(reason = "Instant apps cannot access the SD card")
@RunWith(JUnitParamsRunner.class)
public class ThumbnailUtilsTest {
    private static final Size[] TEST_SIZES = new Size[] {
            new Size(50, 50),
            new Size(500, 500),
            new Size(5000, 5000),
    };

    private File mDir;

    private Object[] getSampleWithThumbnailForCreateImageThumbnail() {
        return new Object[] {
                R.raw.orientation_0,
                R.raw.orientation_90,
                R.raw.orientation_180,
                R.raw.orientation_270,
        };
    }

    private Object[] getStrippedSampleForCreateImageThumbnail() {
        return new Object[] {
                R.raw.orientation_stripped_0,
                R.raw.orientation_stripped_90,
                R.raw.orientation_stripped_180,
                R.raw.orientation_stripped_270

        };
    }

    private Object[] getHEICSampleForCreateImageThumbnail() {
        return new Object[] {
                R.raw.orientation_heic_0,
                R.raw.orientation_heic_90,
                R.raw.orientation_heic_180,
                R.raw.orientation_heic_270

        };
    }

    @Before
    public void setUp() {
        mDir = InstrumentationRegistry.getTargetContext().getExternalCacheDir();
        mDir.mkdirs();
        deleteContents(mDir);
    }

    @After
    public void tearDown() {
        deleteContents(mDir);
    }

    @Test
    public void testCreateAudioThumbnail() throws Exception {
        final File file = stageFile(R.raw.testmp3, new File(mDir, "cts.mp3"));
        for (Size size : TEST_SIZES) {
            assertSaneThumbnail(size, ThumbnailUtils.createAudioThumbnail(file, size, null));
        }
    }

    @Test
    public void testCreateAudioThumbnail_SeparateFile() throws Exception {
        final File file = stageFile(R.raw.monotestmp3, new File(mDir, "audio.mp3"));
        stageFile(R.raw.volantis, new File(mDir, "AlbumArt.jpg"));

        for (Size size : TEST_SIZES) {
            assertSaneThumbnail(size, ThumbnailUtils.createAudioThumbnail(file, size, null));
        }
    }

    @Test
    public void testCreateAudioThumbnail_None() throws Exception {
        final File file = stageFile(R.raw.monotestmp3, new File(mDir, "cts.mp3"));
        try {
            ThumbnailUtils.createAudioThumbnail(file, TEST_SIZES[0], null);
            fail("Somehow made a thumbnail out of nothing?");
        } catch (IOException expected) {
        }
    }

    @Test
    public void testCreateImageThumbnail() throws Exception {
        final File file = stageFile(R.raw.volantis, new File(mDir, "cts.jpg"));
        for (Size size : TEST_SIZES) {
            assertSaneThumbnail(size, ThumbnailUtils.createImageThumbnail(file, size, null));
        }
    }

    private static void assertOrientationForThumbnail(Bitmap bitmap) {
        // All callers are expected to pass a Bitmap with an image of a black cup in the middle
        // (left-to-right) upper portion, on a mostly non-black background. They use different
        // EXIF orientations to achieve the same final image, and this verifies that the EXIF
        // orientation was applied properly.
        assertColorMostlyInRange(bitmap.getPixel(bitmap.getWidth() / 2, bitmap.getHeight() / 3),
                0xFF202020 /* upperBound */, Color.BLACK);
    }

    @Test
    @Parameters(method = "getSampleWithThumbnailForCreateImageThumbnail")
    public void testCreateImageThumbnail_sampleWithThumbnail(int resId) throws Exception {
        final File file = stageFile(resId, new File(mDir, "cts.jpg"));
        final Bitmap bitmap = ThumbnailUtils.createImageThumbnail(file, TEST_SIZES[0], null);

        assertOrientationForThumbnail(bitmap);
    }

    @Test
    @Parameters(method = "getStrippedSampleForCreateImageThumbnail")
    public void testCreateImageThumbnail_strippedSample(int resId) throws Exception {
        final File file = stageFile(resId, new File(mDir, "cts.jpg"));
        final Bitmap bitmap = ThumbnailUtils.createImageThumbnail(file, TEST_SIZES[0], null);

        assertOrientationForThumbnail(bitmap);
    }

    @Test
    @Parameters(method = "getHEICSampleForCreateImageThumbnail")
    public void testCreateImageThumbnail_HEICSample(int resId) throws Exception {
        final File file = stageFile(resId, new File(mDir, "cts.heic"));
        final Bitmap bitmap = ThumbnailUtils.createImageThumbnail(file, TEST_SIZES[0], null);

        assertOrientationForThumbnail(bitmap);
    }

    @Test
    public void testCreateVideoThumbnail() throws Exception {
        final File file = stageFile(
                R.raw.bbb_s1_720x480_mp4_h264_mp3_2mbps_30fps_aac_lc_5ch_320kbps_48000hz,
                new File(mDir, "cts.mp4"));
        for (Size size : TEST_SIZES) {
            assertSaneThumbnail(size, ThumbnailUtils.createVideoThumbnail(file, size, null));
        }
    }

    private static File stageFile(int resId, File file) throws IOException {
        final Context context = InstrumentationRegistry.getTargetContext();
        try (InputStream source = context.getResources().openRawResource(resId);
                OutputStream target = new FileOutputStream(file)) {
            android.os.FileUtils.copy(source, target);
        }
        return file;
    }

    private static void deleteContents(File dir) {
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    deleteContents(file);
                }
                file.delete();
            }
        }
    }

    private static void assertSaneThumbnail(Size expected, Bitmap actualBitmap) {
        final Size actual = new Size(actualBitmap.getWidth(), actualBitmap.getHeight());
        final int maxWidth = (expected.getWidth() * 3) / 2;
        final int maxHeight = (expected.getHeight() * 3) / 2;
        if ((actual.getWidth() > maxWidth) || (actual.getHeight() > maxHeight)) {
            fail("Actual " + actual + " differs too much from expected " + expected);
        }
    }

    private static void assertColorMostlyInRange(@ColorInt int actual, @ColorInt int upperBound,
            @ColorInt int lowerBound) {
        assertTrue(Color.alpha(lowerBound) <= Color.alpha(actual)
                && Color.alpha(actual) <= Color.alpha(upperBound));
        assertTrue(Color.red(lowerBound) <= Color.red(actual)
                && Color.red(actual) <= Color.red(upperBound));
        assertTrue(Color.green(lowerBound) <= Color.green(actual)
                && Color.green(actual) <= Color.green(upperBound));
        assertTrue(Color.blue(lowerBound) <= Color.blue(actual)
                && Color.blue(actual) <= Color.blue(upperBound));
    }
}
