/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static android.system.OsConstants.SEEK_SET;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import android.content.ContentResolver;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.ColorSpace.Named;
import android.graphics.ImageDecoder;
import android.graphics.Rect;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.system.ErrnoException;
import android.system.Os;

import androidx.test.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

@RunWith(JUnitParamsRunner.class)
public class AImageDecoderTest {
    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    private static AssetManager getAssetManager() {
        return InstrumentationRegistry.getTargetContext().getAssets();
    }

    private static Resources getResources() {
        return InstrumentationRegistry.getTargetContext().getResources();
    }

    private static ContentResolver getContentResolver() {
        return InstrumentationRegistry.getTargetContext().getContentResolver();
    }

    // These match the formats in the NDK.
    // ANDROID_BITMAP_FORMAT_NONE is used by nTestDecode to signal using the default.
    private static final int ANDROID_BITMAP_FORMAT_NONE = 0;
    private static final int ANDROID_BITMAP_FORMAT_RGB_565 = 4;
    private static final int ANDROID_BITMAP_FORMAT_A_8 = 8;
    private static final int ANDROID_BITMAP_FORMAT_RGBA_F16 = 9;

    @Test
    public void testEmptyCreate() {
        nTestEmptyCreate();
    }

    private static Object[] getAssetRecords() {
        return ImageDecoderTest.getAssetRecords();
    }

    private static Object[] getRecords() {
        return ImageDecoderTest.getRecords();
    }

    // For testing all of the assets as premul and unpremul.
    private static Object[] getAssetRecordsUnpremul() {
        return Utils.crossProduct(getAssetRecords(), new Object[] { true, false });
    }

    private static Object[] getRecordsUnpremul() {
        return Utils.crossProduct(getRecords(), new Object[] { true, false });
    }

    // For testing all of the assets at different sample sizes.
    private static Object[] getAssetRecordsSample() {
        return Utils.crossProduct(getAssetRecords(), new Object[] { 2, 3, 4, 8, 16 });
    }

    private static Object[] getRecordsSample() {
        return Utils.crossProduct(getRecords(), new Object[] { 2, 3, 4, 8, 16 });
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testNullDecoder(ImageDecoderTest.AssetRecord record) {
        nTestNullDecoder(getAssetManager(), record.name);
    }

    private static int nativeDataSpace(ColorSpace cs) {
        if (cs == null) {
            return DataSpace.ADATASPACE_UNKNOWN;
        }
        return DataSpace.fromColorSpace(cs);
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testCreateBuffer(ImageDecoderTest.AssetRecord record) {
        // Note: This uses an asset for simplicity, but in native it gets a
        // buffer.
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAssetBuffer(asset);

        nTestInfo(aimagedecoder, record.width, record.height, "image/png",
                record.isF16, nativeDataSpace(record.getColorSpace()));
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testCreateFd(ImageDecoderTest.AssetRecord record) {
        // Note: This uses an asset for simplicity, but in native it gets a
        // file descriptor.
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAssetFd(asset);

        nTestInfo(aimagedecoder, record.width, record.height, "image/png",
                record.isF16, nativeDataSpace(record.getColorSpace()));
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testCreateAsset(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestInfo(aimagedecoder, record.width, record.height, "image/png",
                record.isF16, nativeDataSpace(record.getColorSpace()));
        nCloseAsset(asset);
    }

    private static ParcelFileDescriptor open(int resId, int offset) throws FileNotFoundException {
        File file = Utils.obtainFile(resId, offset);
        assertNotNull(file);

        ParcelFileDescriptor pfd = ParcelFileDescriptor.open(file,
                ParcelFileDescriptor.MODE_READ_ONLY);
        assertNotNull(pfd);
        return pfd;
    }

    private static ParcelFileDescriptor open(int resId) throws FileNotFoundException {
        return open(resId, 0);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testCreateFdResources(ImageDecoderTest.Record record) throws IOException {
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestInfo(aimagedecoder, record.width, record.height, record.mimeType,
                    false /*isF16*/, nativeDataSpace(record.colorSpace));
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testCreateFdOffset(ImageDecoderTest.Record record) throws IOException {
        // Use an arbitrary offset. This ensures that we rewind to the correct offset.
        final int offset = 15;
        try (ParcelFileDescriptor pfd = open(record.resId, offset)) {
            FileDescriptor fd = pfd.getFileDescriptor();
            Os.lseek(fd, offset, SEEK_SET);
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestInfo(aimagedecoder, record.width, record.height, record.mimeType,
                    false /*isF16*/, nativeDataSpace(record.colorSpace));
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        } catch (ErrnoException err) {
            fail("Failed to seek " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    public void testCreateIncomplete() {
        String file = "green-srgb.png";
        // This truncates the file before the IDAT.
        nTestCreateIncomplete(getAssetManager(), file, 823);
    }

    @Test
    @Parameters({"shaders/tri.frag", "test_video.mp4"})
    public void testUnsupportedFormat(String file) {
        nTestCreateUnsupported(getAssetManager(), file);
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testSetFormat(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestSetFormat(aimagedecoder, record.isF16, record.isGray);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testSetFormatResources(ImageDecoderTest.Record record) throws IOException {
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestSetFormat(aimagedecoder, false /* isF16 */, record.isGray);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testSetUnpremul(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestSetUnpremul(aimagedecoder, record.hasAlpha);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testSetUnpremulResources(ImageDecoderTest.Record record) throws IOException {
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestSetUnpremul(aimagedecoder, record.hasAlpha);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testGetMinimumStride(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestGetMinimumStride(aimagedecoder, record.isF16, record.isGray);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testGetMinimumStrideResources(ImageDecoderTest.Record record) throws IOException {
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestGetMinimumStride(aimagedecoder, false /* isF16 */, record.isGray);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    private static Bitmap decode(ImageDecoder.Source src, boolean unpremul) {
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, source) -> {
                // So we can compare pixels to the native decode.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                decoder.setUnpremultipliedRequired(unpremul);
            });
        } catch (IOException e) {
            fail("Failed to decode in Java with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getAssetRecordsUnpremul")
    public void testDecode(ImageDecoderTest.AssetRecord record, boolean unpremul) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Bitmap bm = decode(src, unpremul);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_NONE, unpremul, bm);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecordsUnpremul")
    public void testDecodeResources(ImageDecoderTest.Record record, boolean unpremul)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        Bitmap bm = decode(src, unpremul);
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_NONE, unpremul, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    private static Bitmap decode(ImageDecoder.Source src, Bitmap.Config config) {
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, source) -> {
                // So we can compare pixels to the native decode.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                switch (config) {
                    case RGB_565:
                        decoder.setMemorySizePolicy(ImageDecoder.MEMORY_POLICY_LOW_RAM);
                        break;
                    case ALPHA_8:
                        decoder.setDecodeAsAlphaMaskEnabled(true);
                        break;
                    default:
                        fail("Unexpected Config " + config);
                        break;
                }
            });
        } catch (IOException e) {
            fail("Failed to decode in Java with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testDecode565(ImageDecoderTest.AssetRecord record) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Bitmap bm = decode(src, Bitmap.Config.RGB_565);

        if (bm.getConfig() != Bitmap.Config.RGB_565) {
            bm = null;
        }

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_RGB_565, false, bm);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testDecode565Resources(ImageDecoderTest.Record record)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        Bitmap bm = decode(src, Bitmap.Config.RGB_565);

        if (bm.getConfig() != Bitmap.Config.RGB_565) {
            bm = null;
        }

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_RGB_565, false, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    @Parameters("grayscale-linearSrgb.png")
    public void testDecodeA8(String name) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, name);
        Bitmap bm = decode(src, Bitmap.Config.ALPHA_8);

        assertNotNull(bm);
        assertNull(bm.getColorSpace());
        assertEquals(Bitmap.Config.ALPHA_8, bm.getConfig());

        long asset = nOpenAsset(assets, name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_A_8, false, bm);
        nCloseAsset(asset);
    }

    @Test
    public void testDecodeA8Resources()
            throws IOException {
        final int resId = R.drawable.grayscale_jpg;
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                resId);
        Bitmap bm = decode(src, Bitmap.Config.ALPHA_8);

        assertNotNull(bm);
        assertNull(bm.getColorSpace());
        assertEquals(Bitmap.Config.ALPHA_8, bm.getConfig());

        try (ParcelFileDescriptor pfd = open(resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_A_8, false, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(resId));
        }
    }

    @Test
    @Parameters(method = "getAssetRecordsUnpremul")
    public void testDecodeF16(ImageDecoderTest.AssetRecord record, boolean unpremul) {
        AssetManager assets = getAssetManager();

        // ImageDecoder doesn't allow forcing a decode to F16, so use BitmapFactory.
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.RGBA_F16;
        options.inPremultiplied = !unpremul;

        InputStream is = null;
        try {
            is = assets.open(record.name);
        } catch (IOException e) {
            fail("Failed to open " + record.name + " with " + e);
        }
        assertNotNull(is);

        Bitmap bm = BitmapFactory.decodeStream(is, null, options);
        assertNotNull(bm);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_RGBA_F16, unpremul, bm);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecordsUnpremul")
    public void testDecodeF16Resources(ImageDecoderTest.Record record, boolean unpremul)
            throws IOException {
        // ImageDecoder doesn't allow forcing a decode to F16, so use BitmapFactory.
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.RGBA_F16;
        options.inPremultiplied = !unpremul;
        options.inScaled = false;

        Bitmap bm = BitmapFactory.decodeResource(getResources(),
                record.resId, options);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_RGBA_F16, unpremul, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testDecodeStride(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);
        nTestDecodeStride(aimagedecoder);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testDecodeStrideResources(ImageDecoderTest.Record record)
            throws IOException {
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecodeStride(aimagedecoder);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testSetTargetSize(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);
        nTestSetTargetSize(aimagedecoder);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testSetTargetSizeResources(ImageDecoderTest.Record record)
            throws IOException {
        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestSetTargetSize(aimagedecoder);
        } catch (FileNotFoundException e) {
            fail("Could not open " + Utils.getAsResourceUri(record.resId));
        }
    }

    private Bitmap decodeSampled(String name, ImageDecoder.Source src, int sampleSize) {
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, source) -> {
                // So we can compare pixels to the native decode.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                decoder.setTargetSampleSize(sampleSize);
            });
        } catch (IOException e) {
            fail("Failed to decode " + name + " in Java (sampleSize: "
                    + sampleSize + ") with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getAssetRecordsSample")
    public void testDecodeSampled(ImageDecoderTest.AssetRecord record, int sampleSize) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Bitmap bm = decodeSampled(record.name, src, sampleSize);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecodeScaled(aimagedecoder, bm);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecordsSample")
    public void testDecodeResourceSampled(ImageDecoderTest.Record record, int sampleSize)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        String name = Utils.getAsResourceUri(record.resId).toString();
        Bitmap bm = decodeSampled(name, src, sampleSize);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecodeScaled(aimagedecoder, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + name + ": " + e);
        }
    }

    @Test
    @Parameters(method = "getRecordsSample")
    public void testComputeSampledSize(ImageDecoderTest.Record record, int sampleSize)
            throws IOException {
        if (record.mimeType.equals("image/x-adobe-dng")) {
            // SkRawCodec does not support sampling.
            return;
        }
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        String name = Utils.getAsResourceUri(record.resId).toString();
        Bitmap bm = decodeSampled(name, src, sampleSize);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestComputeSampledSize(aimagedecoder, bm, sampleSize);
        } catch (FileNotFoundException e) {
            fail("Could not open " + name + ": " + e);
        }
    }

    private Bitmap decodeScaled(String name, ImageDecoder.Source src) {
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, source) -> {
                // So we can compare pixels to the native decode.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);

                // Scale to an arbitrary width and height.
                decoder.setTargetSize(300, 300);
            });
        } catch (IOException e) {
            fail("Failed to decode " + name + " in Java (size: "
                    + "300 x 300) with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testDecodeScaled(ImageDecoderTest.AssetRecord record) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Bitmap bm = decodeScaled(record.name, src);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecodeScaled(aimagedecoder, bm);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testDecodeResourceScaled(ImageDecoderTest.Record record)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        String name = Utils.getAsResourceUri(record.resId).toString();
        Bitmap bm = decodeScaled(name, src);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecodeScaled(aimagedecoder, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + name + ": " + e);
        }
    }

    private Bitmap decodeScaleUp(String name, ImageDecoder.Source src) {
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, source) -> {
                // So we can compare pixels to the native decode.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);

                decoder.setTargetSize(info.getSize().getWidth() * 2,
                        info.getSize().getHeight() * 2);
            });
        } catch (IOException e) {
            fail("Failed to decode " + name + " in Java (scaled up) with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testDecodeScaleUp(ImageDecoderTest.AssetRecord record) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Bitmap bm = decodeScaleUp(record.name, src);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecodeScaled(aimagedecoder, bm);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testDecodeResourceScaleUp(ImageDecoderTest.Record record)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        String name = Utils.getAsResourceUri(record.resId).toString();
        Bitmap bm = decodeScaleUp(name, src);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecodeScaled(aimagedecoder, bm);
        } catch (FileNotFoundException e) {
            fail("Could not open " + name + ": " + e);
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testSetCrop(ImageDecoderTest.AssetRecord record) {
        nTestSetCrop(getAssetManager(), record.name);
    }

    private static class Cropper implements ImageDecoder.OnHeaderDecodedListener {
        Cropper(boolean scale) {
            mScale = scale;
        }

        public boolean withScale() {
            return mScale;
        }

        public int getWidth() {
            return mWidth;
        }

        public int getHeight() {
            return mHeight;
        }

        public Rect getCropRect() {
            return mCropRect;
        }

        @Override
        public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                ImageDecoder.Source source) {
            mWidth = info.getSize().getWidth();
            mHeight = info.getSize().getHeight();
            if (mScale) {
                mWidth = 40;
                mHeight = 40;
                decoder.setTargetSize(mWidth, mHeight);
            }

            mCropRect = new Rect(mWidth / 2, mHeight / 2, mWidth, mHeight);
            decoder.setCrop(mCropRect);

            // So we can compare pixels to the native decode.
            decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
        }

        private final boolean mScale;
        private Rect mCropRect;
        private int mWidth;
        private int mHeight;
    }

    private static Bitmap decodeCropped(String name, Cropper cropper, ImageDecoder.Source src) {
        try {
            return ImageDecoder.decodeBitmap(src, cropper);
        } catch (IOException e) {
            fail("Failed to decode " + name + " in Java with "
                    + (cropper.withScale() ? "scale and " : "a ") + "crop ("
                    + cropper.getCropRect() + "): " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testCrop(ImageDecoderTest.AssetRecord record) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Cropper cropper = new Cropper(false /* scale */);
        Bitmap bm = decodeCropped(record.name, cropper, src);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        Rect crop = cropper.getCropRect();
        nTestDecodeCrop(aimagedecoder, bm, 0, 0, crop.left, crop.top, crop.right, crop.bottom);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testCropResource(ImageDecoderTest.Record record)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        String name = Utils.getAsResourceUri(record.resId).toString();
        Cropper cropper = new Cropper(false /* scale */);
        Bitmap bm = decodeCropped(name, cropper, src);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            Rect crop = cropper.getCropRect();
            nTestDecodeCrop(aimagedecoder, bm, 0, 0, crop.left, crop.top, crop.right, crop.bottom);
        } catch (FileNotFoundException e) {
            fail("Could not open " + name + ": " + e);
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testCropAndScale(ImageDecoderTest.AssetRecord record) {
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        Cropper cropper = new Cropper(true /* scale */);
        Bitmap bm = decodeCropped(record.name, cropper, src);

        long asset = nOpenAsset(assets, record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        Rect crop = cropper.getCropRect();
        nTestDecodeCrop(aimagedecoder, bm, cropper.getWidth(), cropper.getHeight(),
                crop.left, crop.top, crop.right, crop.bottom);
        nCloseAsset(asset);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testCropAndScaleResource(ImageDecoderTest.Record record)
            throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(),
                record.resId);
        String name = Utils.getAsResourceUri(record.resId).toString();
        Cropper cropper = new Cropper(true /* scale */);
        Bitmap bm = decodeCropped(name, cropper, src);
        assertNotNull(bm);

        try (ParcelFileDescriptor pfd = open(record.resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            Rect crop = cropper.getCropRect();
            nTestDecodeCrop(aimagedecoder, bm, cropper.getWidth(), cropper.getHeight(),
                    crop.left, crop.top, crop.right, crop.bottom);
        } catch (FileNotFoundException e) {
            fail("Could not open " + name + ": " + e);
        }
    }

    private static Object[] getExifImages() {
        return new Object[] {
            R.drawable.orientation_1,
            R.drawable.orientation_2,
            R.drawable.orientation_3,
            R.drawable.orientation_4,
            R.drawable.orientation_5,
            R.drawable.orientation_6,
            R.drawable.orientation_7,
            R.drawable.orientation_8,
            R.drawable.webp_orientation1,
            R.drawable.webp_orientation2,
            R.drawable.webp_orientation3,
            R.drawable.webp_orientation4,
            R.drawable.webp_orientation5,
            R.drawable.webp_orientation6,
            R.drawable.webp_orientation7,
            R.drawable.webp_orientation8,
        };
    }

    @Test
    @Parameters(method = "getExifImages")
    public void testRespectOrientation(int resId) throws IOException {
        Uri uri = Utils.getAsResourceUri(resId);
        ImageDecoder.Source src = ImageDecoder.createSource(getContentResolver(),
                uri);
        Bitmap bm = decode(src, false /* unpremul */);
        assertNotNull(bm);
        assertEquals(100, bm.getWidth());
        assertEquals(80,  bm.getHeight());

        try (ParcelFileDescriptor pfd = open(resId)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());

            nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_NONE, false /* unpremul */, bm);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            fail("Could not open " + uri);
        }
        bm.recycle();
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testScalePlusUnpremul(ImageDecoderTest.AssetRecord record) {
        long asset = nOpenAsset(getAssetManager(), record.name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestScalePlusUnpremul(aimagedecoder);
        nCloseAsset(asset);
    }

    private static File createCompressedBitmap(int width, int height, ColorSpace colorSpace,
            Bitmap.CompressFormat format) {
        File dir = InstrumentationRegistry.getTargetContext().getFilesDir();
        dir.mkdirs();

        File file = new File(dir, colorSpace.getName());
        try {
            file.createNewFile();
        } catch (IOException e) {
            e.printStackTrace();
            // If the file does not exist it will be handled below.
        }
        if (!file.exists()) {
            fail("Failed to create new File for " + file + "!");
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888, true,
                colorSpace);
        bitmap.eraseColor(Color.BLUE);

        try (FileOutputStream fOutput = new FileOutputStream(file)) {
            bitmap.compress(format, 80, fOutput);
            return file;
        } catch (IOException e) {
            e.printStackTrace();
            fail("Failed to create file \"" + file + "\" with exception " + e);
            return null;
        }
    }

    private static Object[] rgbColorSpaces() {
        return BitmapTest.getRgbColorSpaces().toArray();
    }

    private static Object[] rgbColorSpacesAndCompressFormats() {
        return Utils.crossProduct(rgbColorSpaces(), Bitmap.CompressFormat.values());
    }

    String toMimeType(Bitmap.CompressFormat format) {
        switch (format) {
            case JPEG:
                return "image/jpeg";
            case PNG:
                return "image/png";
            case WEBP:
            case WEBP_LOSSY:
            case WEBP_LOSSLESS:
                return "image/webp";
            default:
                return "";
        }
    }

    @Test
    @Parameters(method = "rgbColorSpacesAndCompressFormats")
    public void testGetDataSpace(ColorSpace colorSpace, Bitmap.CompressFormat format) {
        if (colorSpace == ColorSpace.get(Named.EXTENDED_SRGB)
                || colorSpace == ColorSpace.get(Named.LINEAR_EXTENDED_SRGB)) {
            // These will only be reported when the default AndroidBitmapFormat is F16.
            // Bitmap.compress will not compress to an image that will be decoded as F16 by default,
            // so these are covered by the AssetRecord tests.
            return;
        }

        final int width = 10;
        final int height = 10;
        File file = createCompressedBitmap(width, height, colorSpace, format);
        assertNotNull(file);

        int dataSpace = DataSpace.fromColorSpace(colorSpace);

        try (ParcelFileDescriptor pfd = ParcelFileDescriptor.open(file,
                ParcelFileDescriptor.MODE_READ_ONLY)) {
            long aimagedecoder = nCreateFromFd(pfd.getFd());
            nTestInfo(aimagedecoder, width, height, toMimeType(format), false, dataSpace);
        } catch (IOException e) {
            e.printStackTrace();
            fail("Could not read " + file);
        }
    }

    private static Bitmap decode(ImageDecoder.Source src, ColorSpace colorSpace) {
        try {
            return ImageDecoder.decodeBitmap(src, (decoder, info, source) -> {
                // So we can compare pixels to the native decode.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);

                decoder.setTargetColorSpace(colorSpace);
            });
        } catch (IOException e) {
            fail("Failed to decode in Java with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "rgbColorSpaces")
    public void testSetDataSpace(ColorSpace colorSpace) {
        int dataSpace = DataSpace.fromColorSpace(colorSpace);
        if (dataSpace == DataSpace.ADATASPACE_UNKNOWN) {
            // AImageDecoder cannot decode to these ADATASPACEs
            return;
        }

        String name = "translucent-green-p3.png";
        AssetManager assets = getAssetManager();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, name);
        Bitmap bm = decode(src, colorSpace);
        assertEquals(colorSpace, bm.getColorSpace());

        long asset = nOpenAsset(assets, name);
        long aimagedecoder = nCreateFromAsset(asset);

        nTestDecode(aimagedecoder, bm, dataSpace);
        nCloseAsset(asset);
    }

    @Test
    @Parameters({ "cmyk_yellow_224_224_32.jpg", "wide_gamut_yellow_224_224_64.jpeg" })
    public void testNonStandardDataSpaces(String name) {
        AssetManager assets = getAssetManager();
        long asset = nOpenAsset(assets, name);
        long aimagedecoder = nCreateFromAsset(asset);

        // These images have profiles that do not map to ADataSpaces (or even SkColorSpaces).
        // Verify that by default, AImageDecoder will treat them as ADATASPACE_UNKNOWN.
        nTestInfo(aimagedecoder, 32, 32, "image/jpeg", false, DataSpace.ADATASPACE_UNKNOWN);
        nCloseAsset(asset);
    }

    @Test
    @Parameters({ "cmyk_yellow_224_224_32.jpg,#FFD8FC04",
                  "wide_gamut_yellow_224_224_64.jpeg,#FFE0E040" })
    public void testNonStandardDataSpacesDecode(String name, String color) {
        AssetManager assets = getAssetManager();
        long asset = nOpenAsset(assets, name);
        long aimagedecoder = nCreateFromAsset(asset);

        // These images are each a solid color. If we correctly do no color correction, they should
        // match |color|.
        int colorInt = Color.parseColor(color);
        Bitmap bm = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
        bm.eraseColor(colorInt);

        nTestDecode(aimagedecoder, ANDROID_BITMAP_FORMAT_NONE, false /* unpremul */, bm);
        nCloseAsset(asset);
    }

    // Return a pointer to the native AAsset named |file|. Must be closed with nCloseAsset.
    // Throws an Exception on failure.
    private static native long nOpenAsset(AssetManager assets, String file);
    private static native void nCloseAsset(long asset);

    // Methods for creating and returning a pointer to an AImageDecoder. All
    // throw an Exception on failure.
    private static native long nCreateFromFd(int fd);
    private static native long nCreateFromAsset(long asset);
    private static native long nCreateFromAssetFd(long asset);
    private static native long nCreateFromAssetBuffer(long asset);

    private static native void nTestEmptyCreate();
    private static native void nTestNullDecoder(AssetManager assets, String file);
    private static native void nTestCreateIncomplete(AssetManager assets,
            String file, int truncatedLength);
    private static native void nTestCreateUnsupported(AssetManager assets, String file);

    // For convenience, all methods that take aimagedecoder as a parameter delete
    // it.
    private static native void nTestInfo(long aimagedecoder, int width, int height,
            String mimeType, boolean isF16, int dataspace);
    private static native void nTestSetFormat(long aimagedecoder, boolean isF16, boolean isGray);
    private static native void nTestSetUnpremul(long aimagedecoder, boolean hasAlpha);
    private static native void nTestGetMinimumStride(long aimagedecoder,
            boolean isF16, boolean isGray);
    private static native void nTestDecode(long aimagedecoder,
            int requestedAndroidBitmapFormat, boolean unpremul, Bitmap bitmap);
    private static native void nTestDecodeStride(long aimagedecoder);
    private static native void nTestSetTargetSize(long aimagedecoder);
    // Decode with the target width and height to match |bitmap|.
    private static native void nTestDecodeScaled(long aimagedecoder, Bitmap bitmap);
    private static native void nTestComputeSampledSize(long aimagedecoder, Bitmap bm,
            int sampleSize);
    private static native void nTestSetCrop(AssetManager assets, String file);
    // Decode and compare to |bitmap|, where they both use the specified target
    // size and crop rect. target size of 0 means to skip scaling.
    private static native void nTestDecodeCrop(long aimagedecoder,
            Bitmap bitmap, int targetWidth, int targetHeight,
            int cropLeft, int cropTop, int cropRight, int cropBottom);
    private static native void nTestScalePlusUnpremul(long aimagedecoder);
    private static native void nTestDecode(long aimagedecoder, Bitmap bm, int dataSpace);
}
