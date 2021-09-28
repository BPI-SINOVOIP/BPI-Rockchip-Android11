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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.ImageDecoder;
import android.graphics.ImageDecoder.DecodeException;
import android.graphics.ImageDecoder.OnPartialImageListener;
import android.graphics.PixelFormat;
import android.graphics.PostProcessor;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.NinePatchDrawable;
import android.net.Uri;
import android.util.DisplayMetrics;
import android.util.Size;
import android.util.TypedValue;

import androidx.core.content.FileProvider;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import com.android.compatibility.common.util.BitmapUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.concurrent.Callable;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import java.util.function.ToIntFunction;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

@RunWith(JUnitParamsRunner.class)
public class ImageDecoderTest {
    static final class Record {
        public final int resId;
        public final int width;
        public final int height;
        public final boolean isGray;
        public final boolean hasAlpha;
        public final String mimeType;
        public final ColorSpace colorSpace;

        Record(int resId, int width, int height, String mimeType, boolean isGray,
                boolean hasAlpha, ColorSpace colorSpace) {
            this.resId    = resId;
            this.width    = width;
            this.height   = height;
            this.mimeType = mimeType;
            this.isGray   = isGray;
            this.hasAlpha = hasAlpha;
            this.colorSpace = colorSpace;
        }
    }

    private static final ColorSpace sSRGB = ColorSpace.get(ColorSpace.Named.SRGB);

    static Object[] getRecords() {
        return new Record[] {
            new Record(R.drawable.baseline_jpeg, 1280, 960, "image/jpeg", false, false, sSRGB),
            new Record(R.drawable.grayscale_jpg, 128, 128, "image/jpeg", true, false, sSRGB),
            new Record(R.drawable.png_test, 640, 480, "image/png", false, false, sSRGB),
            new Record(R.drawable.gif_test, 320, 240, "image/gif", false, false, sSRGB),
            new Record(R.drawable.bmp_test, 320, 240, "image/bmp", false, false, sSRGB),
            new Record(R.drawable.webp_test, 640, 480, "image/webp", false, false, sSRGB),
            new Record(R.drawable.google_chrome, 256, 256, "image/x-ico", false, true, sSRGB),
            new Record(R.drawable.color_wheel, 128, 128, "image/x-ico", false, true, sSRGB),
            new Record(R.raw.sample_1mp, 600, 338, "image/x-adobe-dng", false, false, sSRGB),
            new Record(R.raw.heifwriter_input, 1920, 1080, "image/heif", false, false, sSRGB),
        };
    }

    // offset is how many bytes to offset the beginning of the image.
    // extra is how many bytes to append at the end.
    private static byte[] getAsByteArray(int resId, int offset, int extra) {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        writeToStream(output, resId, offset, extra);
        return output.toByteArray();
    }

    static void writeToStream(OutputStream output, int resId, int offset, int extra) {
        InputStream input = getResources().openRawResource(resId);
        byte[] buffer = new byte[4096];
        int bytesRead;
        try {
            for (int i = 0; i < offset; ++i) {
                output.write(0);
            }

            while ((bytesRead = input.read(buffer)) != -1) {
                output.write(buffer, 0, bytesRead);
            }

            for (int i = 0; i < extra; ++i) {
                output.write(0);
            }

            input.close();
        } catch (IOException e) {
            fail();
        }
    }

    static byte[] getAsByteArray(int resId) {
        return getAsByteArray(resId, 0, 0);
    }

    private ByteBuffer getAsByteBufferWrap(int resId) {
        byte[] buffer = getAsByteArray(resId);
        return ByteBuffer.wrap(buffer);
    }

    private ByteBuffer getAsDirectByteBuffer(int resId) {
        byte[] buffer = getAsByteArray(resId);
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(buffer.length);
        byteBuffer.put(buffer);
        byteBuffer.position(0);
        return byteBuffer;
    }

    private ByteBuffer getAsReadOnlyByteBuffer(int resId) {
        return getAsByteBufferWrap(resId).asReadOnlyBuffer();
    }

    private File getAsFile(int resId) {
        File file = null;
        try {
            Context context = InstrumentationRegistry.getTargetContext();
            File dir = new File(context.getFilesDir(), "images");
            dir.mkdirs();
            file = new File(dir, "test_file" + resId);
            if (!file.createNewFile()) {
                if (file.exists()) {
                    return file;
                }
                fail("Failed to create new File!");
            }

            FileOutputStream output = new FileOutputStream(file);
            writeToStream(output, resId, 0, 0);
            output.close();

        } catch (IOException e) {
            fail("Failed with exception " + e);
            return null;
        }
        return file;
    }

    private Uri getAsFileUri(int resId) {
        return Uri.fromFile(getAsFile(resId));
    }

    private Uri getAsContentUri(int resId) {
        Context context = InstrumentationRegistry.getTargetContext();
        return FileProvider.getUriForFile(context,
                "android.graphics.cts.fileprovider", getAsFile(resId));
    }

    private Callable<AssetFileDescriptor> getAsCallable(int resId) {
        final Context context = InstrumentationRegistry.getTargetContext();
        final Uri uri = getAsContentUri(resId);
        return () -> {
            return context.getContentResolver().openAssetFileDescriptor(uri, "r");
        };
    }

    private interface SourceCreator extends IntFunction<ImageDecoder.Source> {};

    private SourceCreator[] mCreators = new SourceCreator[] {
            resId -> ImageDecoder.createSource(getAsByteBufferWrap(resId)),
            resId -> ImageDecoder.createSource(getAsDirectByteBuffer(resId)),
            resId -> ImageDecoder.createSource(getAsReadOnlyByteBuffer(resId)),
            resId -> ImageDecoder.createSource(getAsFile(resId)),
            resId -> ImageDecoder.createSource(getAsCallable(resId)),
    };

    private interface UriCreator extends IntFunction<Uri> {};

    private UriCreator[] mUriCreators = new UriCreator[] {
            resId -> Utils.getAsResourceUri(resId),
            resId -> getAsFileUri(resId),
            resId -> getAsContentUri(resId),
    };

    @Test
    @Parameters(method = "getRecords")
    public void testUris(Record record) {
        int resId = record.resId;
        String name = getResources().getResourceEntryName(resId);
        for (UriCreator f : mUriCreators) {
            ImageDecoder.Source src = null;
            Uri uri = f.apply(resId);
            String fullName = name + ": " + uri.toString();
            src = ImageDecoder.createSource(getContentResolver(), uri);

            assertNotNull("failed to create Source for " + fullName, src);
            try {
                Drawable d = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                    decoder.setOnPartialImageListener((e) -> {
                        fail("error for image " + fullName + ":\n" + e);
                        return false;
                    });
                });
                assertNotNull("failed to create drawable for " + fullName, d);
            } catch (IOException e) {
                fail("exception for image " + fullName + ":\n" + e);
            }
        }
    }

    private static Resources getResources() {
        return InstrumentationRegistry.getTargetContext().getResources();
    }

    private static ContentResolver getContentResolver() {
        return InstrumentationRegistry.getTargetContext().getContentResolver();
    }

    @Test
    @Parameters(method = "getRecords")
    public void testInfo(Record record) {
        for (SourceCreator f : mCreators) {
            ImageDecoder.Source src = f.apply(record.resId);
            assertNotNull(src);
            try {
                ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                    assertEquals(record.width,  info.getSize().getWidth());
                    assertEquals(record.height, info.getSize().getHeight());
                    assertEquals(record.mimeType, info.getMimeType());
                    assertSame(record.colorSpace, info.getColorSpace());
                });
            } catch (IOException e) {
                fail("Failed " + Utils.getAsResourceUri(record.resId) + " with exception " + e);
            }
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testDecodeDrawable(Record record) {
        for (SourceCreator f : mCreators) {
            ImageDecoder.Source src = f.apply(record.resId);
            assertNotNull(src);

            try {
                Drawable drawable = ImageDecoder.decodeDrawable(src);
                assertNotNull(drawable);
                assertEquals(record.width,  drawable.getIntrinsicWidth());
                assertEquals(record.height, drawable.getIntrinsicHeight());
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testDecodeBitmap(Record record) {
        for (SourceCreator f : mCreators) {
            ImageDecoder.Source src = f.apply(record.resId);
            assertNotNull(src);

            try {
                Bitmap bm = ImageDecoder.decodeBitmap(src);
                assertNotNull(bm);
                assertEquals(record.width, bm.getWidth());
                assertEquals(record.height, bm.getHeight());
                assertFalse(bm.isMutable());
                // FIXME: This may change for small resources, etc.
                assertEquals(Bitmap.Config.HARDWARE, bm.getConfig());
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
        }
    }

    // Return a single Record for simple tests.
    private Record getRecord() {
        return ((Record[]) getRecords())[0];
    }

    @Test(expected = IllegalArgumentException.class)
    public void testSetBogusAllocator() {
        ImageDecoder.Source src = mCreators[0].apply(getRecord().resId);
        try {
            ImageDecoder.decodeBitmap(src, (decoder, info, s) -> decoder.setAllocator(15));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    private static final int[] ALLOCATORS = new int[] {
        ImageDecoder.ALLOCATOR_SOFTWARE,
        ImageDecoder.ALLOCATOR_SHARED_MEMORY,
        ImageDecoder.ALLOCATOR_HARDWARE,
        ImageDecoder.ALLOCATOR_DEFAULT,
    };

    @Test
    public void testGetAllocator() {
        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertEquals(ImageDecoder.ALLOCATOR_DEFAULT, decoder.getAllocator());
                for (int allocator : ALLOCATORS) {
                    decoder.setAllocator(allocator);
                    assertEquals(allocator, decoder.getAllocator());
                }
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testSetAllocatorDecodeBitmap(Record record) {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            public int allocator;
            public boolean doCrop;
            public boolean doScale;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                decoder.setAllocator(allocator);
                if (doScale) {
                    decoder.setTargetSampleSize(2);
                }
                if (doCrop) {
                    decoder.setCrop(new Rect(1, 1, info.getSize().getWidth()  / 2 - 1,
                                                   info.getSize().getHeight() / 2 - 1));
                }
            }
        };
        Listener l = new Listener();

        boolean trueFalse[] = new boolean[] { true, false };
        Resources res = getResources();
        ImageDecoder.Source src = ImageDecoder.createSource(res, record.resId);
        assertNotNull(src);
        for (int allocator : ALLOCATORS) {
            for (boolean doCrop : trueFalse) {
                for (boolean doScale : trueFalse) {
                    l.doCrop = doCrop;
                    l.doScale = doScale;
                    l.allocator = allocator;

                    Bitmap bm = null;
                    try {
                        bm = ImageDecoder.decodeBitmap(src, l);
                    } catch (IOException e) {
                        fail("Failed " + Utils.getAsResourceUri(record.resId)
                                + " with exception " + e);
                    }
                    assertNotNull(bm);

                    switch (allocator) {
                        case ImageDecoder.ALLOCATOR_SOFTWARE:
                        // TODO: Once Bitmap provides access to its
                        // SharedMemory, confirm that ALLOCATOR_SHARED_MEMORY
                        // worked.
                        case ImageDecoder.ALLOCATOR_SHARED_MEMORY:
                            assertNotEquals(Bitmap.Config.HARDWARE, bm.getConfig());

                            if (!doScale && !doCrop) {
                                BitmapFactory.Options options = new BitmapFactory.Options();
                                options.inScaled = false;
                                Bitmap reference = BitmapFactory.decodeResource(res,
                                        record.resId, options);
                                assertNotNull(reference);
                                assertTrue(BitmapUtils.compareBitmaps(bm, reference));
                            }
                            break;
                        default:
                            String name = Utils.getAsResourceUri(record.resId).toString();
                            assertEquals("image " + name + "; allocator: " + allocator,
                                         Bitmap.Config.HARDWARE, bm.getConfig());
                            break;
                    }
                }
            }
        }
    }

    @Test
    public void testGetUnpremul() {
        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                assertFalse(decoder.isUnpremultipliedRequired());

                decoder.setUnpremultipliedRequired(true);
                assertTrue(decoder.isUnpremultipliedRequired());

                decoder.setUnpremultipliedRequired(false);
                assertFalse(decoder.isUnpremultipliedRequired());
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    public void testUnpremul() {
        int[] resIds = new int[] { R.drawable.png_test, R.drawable.alpha };
        boolean[] hasAlpha = new boolean[] { false,     true };
        for (int i = 0; i < resIds.length; ++i) {
            for (SourceCreator f : mCreators) {
                // Normal decode
                ImageDecoder.Source src = f.apply(resIds[i]);
                assertNotNull(src);

                try {
                    Bitmap normal = ImageDecoder.decodeBitmap(src);
                    assertNotNull(normal);
                    assertEquals(normal.hasAlpha(), hasAlpha[i]);
                    assertEquals(normal.isPremultiplied(), hasAlpha[i]);

                    // Require unpremul
                    src = f.apply(resIds[i]);
                    assertNotNull(src);

                    Bitmap unpremul = ImageDecoder.decodeBitmap(src,
                            (decoder, info, s) -> decoder.setUnpremultipliedRequired(true));
                    assertNotNull(unpremul);
                    assertEquals(unpremul.hasAlpha(), hasAlpha[i]);
                    assertFalse(unpremul.isPremultiplied());
                } catch (IOException e) {
                    fail("Failed with exception " + e);
                }
            }
        }
    }

    @Test
    public void testGetPostProcessor() {
        PostProcessor[] processors = new PostProcessor[] {
                (canvas) -> PixelFormat.UNKNOWN,
                (canvas) -> PixelFormat.UNKNOWN,
                null,
        };
        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertNull(decoder.getPostProcessor());

                for (PostProcessor pp : processors) {
                    decoder.setPostProcessor(pp);
                    assertSame(pp, decoder.getPostProcessor());
                }
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testPostProcessor(Record record) {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            public boolean requireSoftware;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                if (requireSoftware) {
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }
                decoder.setPostProcessor((canvas) -> {
                    canvas.drawColor(Color.BLACK);
                    return PixelFormat.OPAQUE;
                });
            }
        };
        Listener l = new Listener();
        boolean trueFalse[] = new boolean[] { true, false };
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(), record.resId);
        assertNotNull(src);
        for (boolean requireSoftware : trueFalse) {
            l.requireSoftware = requireSoftware;

            Bitmap bitmap = null;
            try {
                bitmap = ImageDecoder.decodeBitmap(src, l);
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
            assertNotNull(bitmap);
            assertFalse(bitmap.isMutable());
            if (requireSoftware) {
                assertNotEquals(Bitmap.Config.HARDWARE, bitmap.getConfig());
                for (int x = 0; x < bitmap.getWidth(); ++x) {
                    for (int y = 0; y < bitmap.getHeight(); ++y) {
                        int color = bitmap.getPixel(x, y);
                        assertEquals("pixel at (" + x + ", " + y + ") does not match!",
                                color, Color.BLACK);
                    }
                }
            } else {
                assertEquals(bitmap.getConfig(), Bitmap.Config.HARDWARE);
            }
        }
    }

    @Test
    public void testNinepatchWithDensityNone() {
        Resources res = getResources();
        TypedValue value = new TypedValue();
        InputStream is = res.openRawResource(R.drawable.ninepatch_nodpi, value);
        // This does not call ImageDecoder directly because this entry point is not public.
        Drawable dr = Drawable.createFromResourceStream(res, value, is, null, null);
        assertNotNull(dr);
        assertEquals(5, dr.getIntrinsicWidth());
        assertEquals(5, dr.getIntrinsicHeight());
    }

    @Test
    public void testPostProcessorOverridesNinepatch() {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            public boolean requireSoftware;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                if (requireSoftware) {
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }
                decoder.setPostProcessor((c) -> PixelFormat.UNKNOWN);
            }
        };
        Listener l = new Listener();
        int resIds[] = new int[] { R.drawable.ninepatch_0,
                                   R.drawable.ninepatch_1 };
        boolean trueFalse[] = new boolean[] { true, false };
        for (int resId : resIds) {
            for (SourceCreator f : mCreators) {
                for (boolean requireSoftware : trueFalse) {
                    l.requireSoftware = requireSoftware;
                    ImageDecoder.Source src = f.apply(resId);
                    try {
                        Drawable drawable = ImageDecoder.decodeDrawable(src, l);
                        assertFalse(drawable instanceof NinePatchDrawable);

                        src = f.apply(resId);
                        Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                        assertNull(bm.getNinePatchChunk());
                    } catch (IOException e) {
                        fail("Failed with exception " + e);
                    }
                }
            }
        }
    }

    @Test
    public void testPostProcessorAndMadeOpaque() {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            public boolean requireSoftware;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                if (requireSoftware) {
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }
                decoder.setPostProcessor((c) -> PixelFormat.OPAQUE);
            }
        };
        Listener l = new Listener();
        boolean trueFalse[] = new boolean[] { true, false };
        int resIds[] = new int[] { R.drawable.alpha, R.drawable.google_logo_2 };
        for (int resId : resIds) {
            for (SourceCreator f : mCreators) {
                for (boolean requireSoftware : trueFalse) {
                    l.requireSoftware = requireSoftware;
                    ImageDecoder.Source src = f.apply(resId);
                    try {
                        Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                        assertFalse(bm.hasAlpha());
                        assertFalse(bm.isPremultiplied());
                    } catch (IOException e) {
                        fail("Failed with exception " + e);
                    }
                }
            }
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testPostProcessorAndAddedTransparency(Record record) {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            public boolean requireSoftware;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                if (requireSoftware) {
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }
                decoder.setPostProcessor((c) -> PixelFormat.TRANSLUCENT);
            }
        };
        Listener l = new Listener();
        boolean trueFalse[] = new boolean[] { true, false };
        for (SourceCreator f : mCreators) {
            for (boolean requireSoftware : trueFalse) {
                l.requireSoftware = requireSoftware;
                ImageDecoder.Source src = f.apply(record.resId);
                try {
                    Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                    assertTrue(bm.hasAlpha());
                    assertTrue(bm.isPremultiplied());
                } catch (IOException e) {
                    fail("Failed with exception " + e);
                }
            }
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testPostProcessorTRANSPARENT() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setPostProcessor((c) -> PixelFormat.TRANSPARENT);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testPostProcessorInvalidReturn() {
        ImageDecoder.Source src = mCreators[0].apply(getRecord().resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setPostProcessor((c) -> 42);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testPostProcessorAndUnpremul() {
        ImageDecoder.Source src = mCreators[0].apply(getRecord().resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setUnpremultipliedRequired(true);
                decoder.setPostProcessor((c) -> PixelFormat.UNKNOWN);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testPostProcessorAndScale(Record record) {
        class PostProcessorWithSize implements PostProcessor {
            public int width;
            public int height;
            @Override
            public int onPostProcess(Canvas canvas) {
                assertEquals(this.width,  width);
                assertEquals(this.height, height);
                return PixelFormat.UNKNOWN;
            };
        };
        final PostProcessorWithSize pp = new PostProcessorWithSize();
        pp.width =  record.width  / 2;
        pp.height = record.height / 2;
        for (SourceCreator f : mCreators) {
            ImageDecoder.Source src = f.apply(record.resId);
            try {
                Drawable drawable = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                    decoder.setTargetSize(pp.width, pp.height);
                    decoder.setPostProcessor(pp);
                });
                assertEquals(pp.width,  drawable.getIntrinsicWidth());
                assertEquals(pp.height, drawable.getIntrinsicHeight());
            } catch (IOException e) {
                fail("Failed " + Utils.getAsResourceUri(record.resId) + " with exception " + e);
            }
        }
    }

    private void checkSampleSize(String name, int originalDimension, int sampleSize, int result) {
        if (originalDimension % sampleSize == 0) {
            assertEquals("Mismatch for " + name + ": " + originalDimension + " / " + sampleSize
                         + " != " + result, originalDimension / sampleSize, result);
        } else if (originalDimension <= sampleSize) {
            assertEquals(1, result);
        } else {
            // Rounding may result in differences.
            int size = result * sampleSize;
            assertTrue("Rounding mismatch for " + name + ": " + originalDimension + " / "
                       + sampleSize + " = " + result,
                       Math.abs(size - originalDimension) < sampleSize);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testSampleSize(Record record) {
        final String name = Utils.getAsResourceUri(record.resId).toString();
        for (int sampleSize : new int[] { 2, 3, 4, 8, 32 }) {
            ImageDecoder.Source src = mCreators[0].apply(record.resId);
            try {
                Drawable dr = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                    decoder.setTargetSampleSize(sampleSize);
                });

                checkSampleSize(name, record.width, sampleSize, dr.getIntrinsicWidth());
                checkSampleSize(name, record.height, sampleSize, dr.getIntrinsicHeight());
            } catch (IOException e) {
                fail("Failed " + name + " with exception " + e);
            }
        }
    }

    private interface SampleSizeSupplier extends ToIntFunction<Size> {};

    @Test
    @Parameters(method = "getRecords")
    public void testLargeSampleSize(Record record) {
        ImageDecoder.Source src = mCreators[0].apply(record.resId);
        for (SampleSizeSupplier supplySampleSize : new SampleSizeSupplier[] {
                (size) -> size.getWidth(),
                (size) -> size.getWidth() + 5,
                (size) -> size.getWidth() * 5,
        }) {
            try {
                Drawable dr = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                    int sampleSize = supplySampleSize.applyAsInt(info.getSize());
                    decoder.setTargetSampleSize(sampleSize);
                });
                assertEquals(1, dr.getIntrinsicWidth());
            } catch (Exception e) {
                String file = Utils.getAsResourceUri(record.resId).toString();
                fail("Failed to decode " + file + " with exception " + e);
            }
        }
    }

    @Test
    public void testResizeTransparency() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.animated);
        Drawable dr = null;
        try {
            dr = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                Size size = info.getSize();
                decoder.setTargetSize(size.getWidth() - 5, size.getHeight() - 5);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }

        final int width = dr.getIntrinsicWidth();
        final int height = dr.getIntrinsicHeight();

        // Draw to a fully transparent Bitmap. Pixels that are transparent in the image will be
        // transparent.
        Bitmap normal = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        {
            Canvas canvas = new Canvas(normal);
            dr.draw(canvas);
        }

        // Draw to a BLUE Bitmap. Any pixels that are transparent in the image remain BLUE.
        Bitmap blended = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        {
            Canvas canvas = new Canvas(blended);
            canvas.drawColor(Color.BLUE);
            dr.draw(canvas);
        }

        boolean hasTransparency = false;
        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {
                int normalColor = normal.getPixel(i, j);
                int blendedColor = blended.getPixel(i, j);
                if (normalColor == Color.TRANSPARENT) {
                    hasTransparency = true;
                    assertEquals(Color.BLUE, blendedColor);
                } else if (Color.alpha(normalColor) == 255) {
                    assertEquals(normalColor, blendedColor);
                }
            }
        }

        // Verify that the image has transparency. Otherwise the test is not useful.
        assertTrue(hasTransparency);
    }

    @Test
    public void testGetOnPartialImageListener() {
        OnPartialImageListener[] listeners = new OnPartialImageListener[] {
                (e) -> true,
                (e) -> false,
                null,
        };

        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertNull(decoder.getOnPartialImageListener());

                for (OnPartialImageListener l : listeners) {
                    decoder.setOnPartialImageListener(l);
                    assertSame(l, decoder.getOnPartialImageListener());
                }
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    public void testEarlyIncomplete() {
        byte[] bytes = getAsByteArray(R.raw.basi6a16);
        // This is too early to create a partial image, so we throw the Exception
        // without calling the listener.
        int truncatedLength = 49;
        ImageDecoder.Source src = ImageDecoder.createSource(
                ByteBuffer.wrap(bytes, 0, truncatedLength));
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setOnPartialImageListener((e) -> {
                    fail("No need to call listener; no partial image to display!"
                            + " Exception: " + e);
                    return false;
                });
            });
        } catch (DecodeException e) {
            assertEquals(DecodeException.SOURCE_INCOMPLETE, e.getError());
            assertSame(src, e.getSource());
        } catch (IOException ioe) {
            fail("Threw some other exception: " + ioe);
        }
    }

    private class ExceptionStream extends InputStream {
        private final InputStream mInputStream;
        private final int mExceptionPosition;
        int mPosition;

        ExceptionStream(int resId, int exceptionPosition) {
            mInputStream = getResources().openRawResource(resId);
            mExceptionPosition = exceptionPosition;
            mPosition = 0;
        }

        @Override
        public int read() throws IOException {
            if (mPosition >= mExceptionPosition) {
                throw new IOException();
            }

            int value = mInputStream.read();
            mPosition++;
            return value;
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            if (mPosition + len <= mExceptionPosition) {
                final int bytesRead = mInputStream.read(b, off, len);
                mPosition += bytesRead;
                return bytesRead;
            }

            len = mExceptionPosition - mPosition;
            mPosition += mInputStream.read(b, off, len);
            throw new IOException();
        }
    }

    @Test
    public void testExceptionInStream() throws Throwable {
        InputStream is = new ExceptionStream(R.drawable.animated, 27570);
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(), is,
                Bitmap.DENSITY_NONE);
        Drawable dr = null;
        try {
            dr = ImageDecoder.decodeDrawable(src);
            fail("Expected to throw an exception!");
        } catch (IOException ioe) {
            assertTrue(ioe instanceof DecodeException);
            DecodeException decodeException = (DecodeException) ioe;
            assertEquals(DecodeException.SOURCE_EXCEPTION, decodeException.getError());
            Throwable throwable = decodeException.getCause();
            assertNotNull(throwable);
            assertTrue(throwable instanceof IOException);
        }
        assertNull(dr);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testOnPartialImage(Record record) {
        class PartialImageCallback implements OnPartialImageListener {
            public boolean wasCalled;
            public boolean returnDrawable;
            public ImageDecoder.Source source;
            @Override
            public boolean onPartialImage(DecodeException e) {
                wasCalled = true;
                assertEquals(DecodeException.SOURCE_INCOMPLETE, e.getError());
                assertSame(source, e.getSource());
                return returnDrawable;
            }
        };
        final PartialImageCallback callback = new PartialImageCallback();
        boolean abortDecode[] = new boolean[] { true, false };
        byte[] bytes = getAsByteArray(record.resId);
        int truncatedLength = bytes.length / 2;
        if (record.mimeType.equals("image/x-ico")
                || record.mimeType.equals("image/x-adobe-dng")
                || record.mimeType.equals("image/heif")) {
            // FIXME (scroggo): Some codecs currently do not support incomplete images.
            return;
        }
        if (record.resId == R.drawable.grayscale_jpg) {
            // FIXME (scroggo): This is a progressive jpeg. If Skia switches to
            // decoding jpegs progressively, this image can be partially decoded.
            return;
        }
        for (boolean abort : abortDecode) {
            ImageDecoder.Source src = ImageDecoder.createSource(
                    ByteBuffer.wrap(bytes, 0, truncatedLength));
            callback.wasCalled = false;
            callback.returnDrawable = !abort;
            callback.source = src;
            try {
                Drawable drawable = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                    decoder.setOnPartialImageListener(callback);
                });
                assertFalse(abort);
                assertNotNull(drawable);
                assertEquals(record.width,  drawable.getIntrinsicWidth());
                assertEquals(record.height, drawable.getIntrinsicHeight());
            } catch (IOException e) {
                assertTrue(abort);
            }
            assertTrue(callback.wasCalled);
        }

        // null listener behaves as if onPartialImage returned false.
        ImageDecoder.Source src = ImageDecoder.createSource(
                ByteBuffer.wrap(bytes, 0, truncatedLength));
        try {
            ImageDecoder.decodeDrawable(src);
            fail("Should have thrown an exception!");
        } catch (DecodeException incomplete) {
            // This is the correct behavior.
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testCorruptException() {
        class PartialImageCallback implements OnPartialImageListener {
            public boolean wasCalled = false;
            public ImageDecoder.Source source;
            @Override
            public boolean onPartialImage(DecodeException e) {
                wasCalled = true;
                assertEquals(DecodeException.SOURCE_MALFORMED_DATA, e.getError());
                assertSame(source, e.getSource());
                return true;
            }
        };
        final PartialImageCallback callback = new PartialImageCallback();
        byte[] bytes = getAsByteArray(R.drawable.png_test);
        // The four bytes starting with byte 40,000 represent the CRC. Changing
        // them will cause the decode to fail.
        for (int i = 0; i < 4; ++i) {
            bytes[40000 + i] = 'X';
        }
        ImageDecoder.Source src = ImageDecoder.createSource(ByteBuffer.wrap(bytes));
        callback.wasCalled = false;
        callback.source = src;
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setOnPartialImageListener(callback);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
        assertTrue(callback.wasCalled);
    }

    private static class DummyException extends RuntimeException {};

    @Test
    public void  testPartialImageThrowException() {
        byte[] bytes = getAsByteArray(R.drawable.png_test);
        ImageDecoder.Source src = ImageDecoder.createSource(
                ByteBuffer.wrap(bytes, 0, bytes.length / 2));
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setOnPartialImageListener((e) -> {
                    throw new DummyException();
                });
            });
            fail("Should have thrown an exception");
        } catch (DummyException dummy) {
            // This is correct.
        } catch (Throwable t) {
            fail("Should have thrown DummyException - threw " + t + " instead");
        }
    }

    @Test
    public void testGetMutable() {
        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertFalse(decoder.isMutableRequired());

                decoder.setMutableRequired(true);
                assertTrue(decoder.isMutableRequired());

                decoder.setMutableRequired(false);
                assertFalse(decoder.isMutableRequired());
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testMutable(Record record) {
        int allocators[] = new int[] { ImageDecoder.ALLOCATOR_DEFAULT,
                                       ImageDecoder.ALLOCATOR_SOFTWARE,
                                       ImageDecoder.ALLOCATOR_SHARED_MEMORY };
        class HeaderListener implements ImageDecoder.OnHeaderDecodedListener {
            int allocator;
            boolean postProcess;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder,
                                        ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                decoder.setMutableRequired(true);
                decoder.setAllocator(allocator);
                if (postProcess) {
                    decoder.setPostProcessor((c) -> PixelFormat.UNKNOWN);
                }
            }
        };
        HeaderListener l = new HeaderListener();
        boolean trueFalse[] = new boolean[] { true, false };
        ImageDecoder.Source src = mCreators[0].apply(record.resId);
        for (boolean postProcess : trueFalse) {
            for (int allocator : allocators) {
                l.allocator = allocator;
                l.postProcess = postProcess;

                try {
                    Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                    assertTrue(bm.isMutable());
                    assertNotEquals(Bitmap.Config.HARDWARE, bm.getConfig());
                } catch (Exception e) {
                    String file = Utils.getAsResourceUri(record.resId).toString();
                    fail("Failed to decode " + file + " with exception " + e);
                }
            }
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testMutableHardware() {
        ImageDecoder.Source src = mCreators[0].apply(getRecord().resId);
        try {
            ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                decoder.setMutableRequired(true);
                decoder.setAllocator(ImageDecoder.ALLOCATOR_HARDWARE);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testMutableDrawable() {
        ImageDecoder.Source src = mCreators[0].apply(getRecord().resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setMutableRequired(true);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    private interface EmptyByteBufferCreator {
        public ByteBuffer apply();
    };

    @Test
    public void testEmptyByteBuffer() {
        class Direct implements EmptyByteBufferCreator {
            @Override
            public ByteBuffer apply() {
                return ByteBuffer.allocateDirect(0);
            }
        };
        class Wrap implements EmptyByteBufferCreator {
            @Override
            public ByteBuffer apply() {
                byte[] bytes = new byte[0];
                return ByteBuffer.wrap(bytes);
            }
        };
        class ReadOnly implements EmptyByteBufferCreator {
            @Override
            public ByteBuffer apply() {
                byte[] bytes = new byte[0];
                return ByteBuffer.wrap(bytes).asReadOnlyBuffer();
            }
        };
        EmptyByteBufferCreator creators[] = new EmptyByteBufferCreator[] {
            new Direct(), new Wrap(), new ReadOnly() };
        for (EmptyByteBufferCreator creator : creators) {
            try {
                ImageDecoder.decodeDrawable(
                        ImageDecoder.createSource(creator.apply()));
                fail("This should have thrown an exception");
            } catch (IOException e) {
                // This is correct.
            }
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testZeroSampleSize() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> decoder.setTargetSampleSize(0));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNegativeSampleSize() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> decoder.setTargetSampleSize(-2));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testTargetSize(Record record) {
        class ResizeListener implements ImageDecoder.OnHeaderDecodedListener {
            public int width;
            public int height;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                decoder.setTargetSize(width, height);
            }
        };
        ResizeListener l = new ResizeListener();

        float[] scales = new float[] { .0625f, .125f, .25f, .5f, .75f, 1.1f, 2.0f };
        ImageDecoder.Source src = mCreators[0].apply(record.resId);
        for (int j = 0; j < scales.length; ++j) {
            l.width  = (int) (scales[j] * record.width);
            l.height = (int) (scales[j] * record.height);

            try {
                Drawable drawable = ImageDecoder.decodeDrawable(src, l);
                assertEquals(l.width,  drawable.getIntrinsicWidth());
                assertEquals(l.height, drawable.getIntrinsicHeight());

                Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                assertEquals(l.width,  bm.getWidth());
                assertEquals(l.height, bm.getHeight());
            } catch (IOException e) {
                fail("Failed " + Utils.getAsResourceUri(record.resId) + " with exception " + e);
            }
        }

        try {
            // Arbitrary square.
            l.width  = 50;
            l.height = 50;
            Drawable drawable = ImageDecoder.decodeDrawable(src, l);
            assertEquals(50,  drawable.getIntrinsicWidth());
            assertEquals(50, drawable.getIntrinsicHeight());

            // Swap width and height, for different scales.
            l.height = record.width;
            l.width  = record.height;
            drawable = ImageDecoder.decodeDrawable(src, l);
            assertEquals(record.height, drawable.getIntrinsicWidth());
            assertEquals(record.width,  drawable.getIntrinsicHeight());
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testResizeWebp() {
        // libwebp supports unpremultiplied for downscaled output
        class ResizeListener implements ImageDecoder.OnHeaderDecodedListener {
            public int width;
            public int height;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                decoder.setTargetSize(width, height);
                decoder.setUnpremultipliedRequired(true);
            }
        };
        ResizeListener l = new ResizeListener();

        float[] scales = new float[] { .0625f, .125f, .25f, .5f, .75f };
        for (SourceCreator f : mCreators) {
            for (int j = 0; j < scales.length; ++j) {
                l.width  = (int) (scales[j] * 240);
                l.height = (int) (scales[j] *  87);

                ImageDecoder.Source src = f.apply(R.drawable.google_logo_2);
                try {
                    Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                    assertEquals(l.width,  bm.getWidth());
                    assertEquals(l.height, bm.getHeight());
                    assertTrue(bm.hasAlpha());
                    assertFalse(bm.isPremultiplied());
                } catch (IOException e) {
                    fail("Failed with exception " + e);
                }
            }
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testResizeWebpLarger() {
        // libwebp does not upscale, so there is no way to get unpremul.
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.google_logo_2);
        try {
            ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                Size size = info.getSize();
                decoder.setTargetSize(size.getWidth() * 2, size.getHeight() * 2);
                decoder.setUnpremultipliedRequired(true);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testResizeUnpremul() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.alpha);
        try {
            ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                // Choose a width and height that cannot be achieved with sampling.
                Size size = info.getSize();
                int width = size.getWidth() / 2 + 3;
                int height = size.getHeight() / 2 + 3;
                decoder.setTargetSize(width, height);
                decoder.setUnpremultipliedRequired(true);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testGetCrop() {
        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertNull(decoder.getCrop());

                Rect r = new Rect(0, 0, info.getSize().getWidth() / 2, 5);
                decoder.setCrop(r);
                assertEquals(r, decoder.getCrop());

                r = new Rect(0, 0, 5, 10);
                decoder.setCrop(r);
                assertEquals(r, decoder.getCrop());
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testCrop(Record record) {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            public boolean doScale;
            public boolean requireSoftware;
            public Rect cropRect;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                int width  = info.getSize().getWidth();
                int height = info.getSize().getHeight();
                if (doScale) {
                    width  /= 2;
                    height /= 2;
                    decoder.setTargetSize(width, height);
                }
                // Crop to the middle:
                int quarterWidth  = width  / 4;
                int quarterHeight = height / 4;
                cropRect = new Rect(quarterWidth, quarterHeight,
                        quarterWidth * 3, quarterHeight * 3);
                decoder.setCrop(cropRect);

                if (requireSoftware) {
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }
            }
        };
        Listener l = new Listener();
        boolean trueFalse[] = new boolean[] { true, false };
        for (SourceCreator f : mCreators) {
            for (boolean doScale : trueFalse) {
                l.doScale = doScale;
                for (boolean requireSoftware : trueFalse) {
                    l.requireSoftware = requireSoftware;
                    ImageDecoder.Source src = f.apply(record.resId);

                    try {
                        Drawable drawable = ImageDecoder.decodeDrawable(src, l);
                        assertEquals(l.cropRect.width(),  drawable.getIntrinsicWidth());
                        assertEquals(l.cropRect.height(), drawable.getIntrinsicHeight());
                    } catch (IOException e) {
                        fail("Failed " + Utils.getAsResourceUri(record.resId)
                                + " with exception " + e);
                    }
                }
            }
        }
    }

    @Test
    public void testScaleAndCrop() {
        class CropListener implements ImageDecoder.OnHeaderDecodedListener {
            public boolean doCrop = true;
            public Rect outScaledRect = null;
            public Rect outCropRect = null;

            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                // Use software for pixel comparison.
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);

                // Scale to a size that is not directly supported by sampling.
                Size originalSize = info.getSize();
                int scaledWidth = originalSize.getWidth() * 2 / 3;
                int scaledHeight = originalSize.getHeight() * 2 / 3;
                decoder.setTargetSize(scaledWidth, scaledHeight);

                outScaledRect = new Rect(0, 0, scaledWidth, scaledHeight);

                if (doCrop) {
                    outCropRect = new Rect(scaledWidth / 2, scaledHeight / 2,
                            scaledWidth, scaledHeight);
                    decoder.setCrop(outCropRect);
                }
            }
        }
        CropListener l = new CropListener();
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);

        // Scale and crop in a single step.
        Bitmap oneStepBm = null;
        try {
            oneStepBm = ImageDecoder.decodeBitmap(src, l);
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
        assertNotNull(oneStepBm);
        assertNotNull(l.outCropRect);
        assertEquals(l.outCropRect.width(), oneStepBm.getWidth());
        assertEquals(l.outCropRect.height(), oneStepBm.getHeight());
        Rect cropRect = new Rect(l.outCropRect);

        assertNotNull(l.outScaledRect);
        Rect scaledRect = new Rect(l.outScaledRect);

        // Now just scale with ImageDecoder, and crop afterwards.
        l.doCrop = false;
        Bitmap twoStepBm = null;
        try {
            twoStepBm = ImageDecoder.decodeBitmap(src, l);
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
        assertNotNull(twoStepBm);
        assertEquals(scaledRect.width(), twoStepBm.getWidth());
        assertEquals(scaledRect.height(), twoStepBm.getHeight());

        Bitmap cropped = Bitmap.createBitmap(twoStepBm, cropRect.left, cropRect.top,
                cropRect.width(), cropRect.height());
        assertNotNull(cropped);

        // The two should look the same.
        assertTrue(BitmapUtils.compareBitmaps(cropped, oneStepBm, .99));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testResizeZeroX() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) ->
                    decoder.setTargetSize(0, info.getSize().getHeight()));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testResizeZeroY() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) ->
                    decoder.setTargetSize(info.getSize().getWidth(), 0));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testResizeNegativeX() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) ->
                    decoder.setTargetSize(-10, info.getSize().getHeight()));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testResizeNegativeY() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) ->
                    decoder.setTargetSize(info.getSize().getWidth(), -10));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testStoreImageDecoder() {
        class CachingCallback implements ImageDecoder.OnHeaderDecodedListener {
            ImageDecoder cachedDecoder;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                cachedDecoder = decoder;
            }
        };
        CachingCallback l = new CachingCallback();
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, l);
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
        l.cachedDecoder.setTargetSampleSize(2);
    }

    @Test(expected = IllegalStateException.class)
    public void testDecodeUnpremulDrawable() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.png_test);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) ->
                    decoder.setUnpremultipliedRequired(true));
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    // One static PNG and one animated GIF to test setting invalid crop rects,
    // to test both paths (animated and non-animated) through ImageDecoder.
    private static Object[] resourcesForCropTests() {
        return new Object[] { R.drawable.png_test, R.drawable.animated };
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testInvertCropWidth(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                // This rect is unsorted.
                decoder.setCrop(new Rect(info.getSize().getWidth(), 0, 0,
                                         info.getSize().getHeight()));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testInvertCropHeight(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                // This rect is unsorted.
                decoder.setCrop(new Rect(0, info.getSize().getWidth(),
                                         info.getSize().getHeight(), 0));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testEmptyCrop(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setCrop(new Rect(1, 1, 1, 1));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testCropNegativeLeft(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setCrop(new Rect(-1, 0, info.getSize().getWidth(),
                                                info.getSize().getHeight()));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testCropNegativeTop(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setCrop(new Rect(0, -1, info.getSize().getWidth(),
                                                info.getSize().getHeight()));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testCropTooWide(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setCrop(new Rect(1, 0, info.getSize().getWidth() + 1,
                                               info.getSize().getHeight()));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }


    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testCropTooTall(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setCrop(new Rect(0, 1, info.getSize().getWidth(),
                                               info.getSize().getHeight() + 1));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test(expected = IllegalStateException.class)
    @Parameters(method = "resourcesForCropTests")
    public void testCropResize(int resId) {
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                Size size = info.getSize();
                decoder.setTargetSize(size.getWidth() / 2, size.getHeight() / 2);
                decoder.setCrop(new Rect(0, 0, size.getWidth(),
                                               size.getHeight()));
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testAlphaMaskNonGray() {
        // It is safe to call setDecodeAsAlphaMaskEnabled on a non-gray image.
        SourceCreator f = mCreators[0];
        ImageDecoder.Source src = f.apply(R.drawable.png_test);
        assertNotNull(src);
        try {
            Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                decoder.setDecodeAsAlphaMaskEnabled(true);
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
            });
            assertNotNull(bm);
            assertNotEquals(Bitmap.Config.ALPHA_8, bm.getConfig());
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testAlphaPlusSetTargetColorSpace() {
        // TargetColorSpace is ignored for ALPHA_8
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.grayscale_png);
        for (ColorSpace cs : BitmapTest.getRgbColorSpaces()) {
            try {
                Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                    decoder.setDecodeAsAlphaMaskEnabled(true);
                    decoder.setTargetColorSpace(cs);
                });
                assertNotNull(bm);
                assertEquals(Bitmap.Config.ALPHA_8, bm.getConfig());
                assertNull(bm.getColorSpace());
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testAlphaMaskPlusHardware() {
        SourceCreator f = mCreators[0];
        ImageDecoder.Source src = f.apply(R.drawable.png_test);
        assertNotNull(src);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setDecodeAsAlphaMaskEnabled(true);
                decoder.setAllocator(ImageDecoder.ALLOCATOR_HARDWARE);
            });
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testAlphaMaskPlusHardwareAnimated() {
        // AnimatedImageDrawable ignores both of these settings, so it is okay
        // to combine them.
        SourceCreator f = mCreators[0];
        ImageDecoder.Source src = f.apply(R.drawable.animated);
        assertNotNull(src);
        try {
            Drawable d = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setDecodeAsAlphaMaskEnabled(true);
                decoder.setAllocator(ImageDecoder.ALLOCATOR_HARDWARE);
            });
            assertNotNull(d);
        } catch (IOException e) {
            fail("Failed with exception " + e);
        }
    }

    @Test
    public void testGetAlphaMask() {
        final int resId = R.drawable.grayscale_png;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertFalse(decoder.isDecodeAsAlphaMaskEnabled());

                decoder.setDecodeAsAlphaMaskEnabled(true);
                assertTrue(decoder.isDecodeAsAlphaMaskEnabled());

                decoder.setDecodeAsAlphaMaskEnabled(false);
                assertFalse(decoder.isDecodeAsAlphaMaskEnabled());
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    public void testAlphaMask() {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            boolean doCrop;
            boolean doScale;
            boolean doPostProcess;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                decoder.setDecodeAsAlphaMaskEnabled(true);
                Size size = info.getSize();
                if (doScale) {
                    decoder.setTargetSize(size.getWidth() / 2,
                                          size.getHeight() / 2);
                }
                if (doCrop) {
                    decoder.setCrop(new Rect(0, 0, size.getWidth() / 4,
                                                   size.getHeight() / 4));
                }
                if (doPostProcess) {
                    decoder.setPostProcessor((c) -> {
                        c.drawColor(Color.BLACK);
                        return PixelFormat.UNKNOWN;
                    });
                }
            }
        };
        Listener l = new Listener();
        // Both of these are encoded as single channel gray images.
        int resIds[] = new int[] { R.drawable.grayscale_png, R.drawable.grayscale_jpg };
        boolean trueFalse[] = new boolean[] { true, false };
        SourceCreator f = mCreators[0];
        for (int resId : resIds) {
            // By default, this will decode to HARDWARE
            ImageDecoder.Source src = f.apply(resId);
            try {
                Bitmap bm = ImageDecoder.decodeBitmap(src);
                assertEquals(Bitmap.Config.HARDWARE, bm.getConfig());
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }

            // Now set alpha mask, which is incompatible with HARDWARE
            for (boolean doCrop : trueFalse) {
                for (boolean doScale : trueFalse) {
                    for (boolean doPostProcess : trueFalse) {
                        l.doCrop = doCrop;
                        l.doScale = doScale;
                        l.doPostProcess = doPostProcess;
                        src = f.apply(resId);
                        try {
                            Bitmap bm = ImageDecoder.decodeBitmap(src, l);
                            assertEquals(Bitmap.Config.ALPHA_8, bm.getConfig());
                            assertNull(bm.getColorSpace());
                        } catch (IOException e) {
                            fail("Failed with exception " + e);
                        }
                    }
                }
            }
        }
    }

    @Test
    public void testGetConserveMemory() {
        final int resId = getRecord().resId;
        ImageDecoder.Source src = mCreators[0].apply(resId);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                assertEquals(ImageDecoder.MEMORY_POLICY_DEFAULT, decoder.getMemorySizePolicy());

                decoder.setMemorySizePolicy(ImageDecoder.MEMORY_POLICY_LOW_RAM);
                assertEquals(ImageDecoder.MEMORY_POLICY_LOW_RAM, decoder.getMemorySizePolicy());

                decoder.setMemorySizePolicy(ImageDecoder.MEMORY_POLICY_DEFAULT);
                assertEquals(ImageDecoder.MEMORY_POLICY_DEFAULT, decoder.getMemorySizePolicy());
            });
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with exception " + e);
        }
    }

    @Test
    public void testConserveMemoryPlusHardware() {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            int allocator;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                decoder.setMemorySizePolicy(ImageDecoder.MEMORY_POLICY_LOW_RAM);
                decoder.setAllocator(allocator);
            }
        };
        Listener l = new Listener();
        SourceCreator f = mCreators[0];
        for (int resId : new int[] { R.drawable.png_test, R.raw.f16 }) {
            Bitmap normal = null;
            try {
                normal = ImageDecoder.decodeBitmap(f.apply(resId), ((decoder, info, source) -> {
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }));
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
            assertNotNull(normal);
            int normalByteCount = normal.getAllocationByteCount();
            int[] allocators = { ImageDecoder.ALLOCATOR_HARDWARE, ImageDecoder.ALLOCATOR_DEFAULT };
            for (int allocator : allocators) {
                l.allocator = allocator;
                Bitmap test = null;
                try {
                    test = ImageDecoder.decodeBitmap(f.apply(resId), l);
                } catch (IOException e) {
                    fail("Failed with exception " + e);
                }
                assertNotNull(test);
                int byteCount = test.getAllocationByteCount();

                if (resId == R.drawable.png_test) {
                    // We do not support 565 in HARDWARE, so no RAM savings
                    // are possible.
                    assertEquals(normalByteCount, byteCount);
                } else { // R.raw.f16
                    // This image defaults to F16. MEMORY_POLICY_LOW_RAM
                    // forces "test" to decode to 8888.
                    assertTrue(byteCount < normalByteCount);
                }
            }
        }
    }

    @Test
    public void testConserveMemory() {
        class Listener implements ImageDecoder.OnHeaderDecodedListener {
            boolean doPostProcess;
            boolean preferRamOverQuality;
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source src) {
                if (preferRamOverQuality) {
                    decoder.setMemorySizePolicy(ImageDecoder.MEMORY_POLICY_LOW_RAM);
                }
                if (doPostProcess) {
                    decoder.setPostProcessor((c) -> {
                        c.drawColor(Color.BLACK);
                        return PixelFormat.TRANSLUCENT;
                    });
                }
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
            }
        };
        Listener l = new Listener();
        // All of these images are opaque, so they can save RAM with
        // setConserveMemory.
        int resIds[] = new int[] { R.drawable.png_test, R.drawable.baseline_jpeg,
                                   // If this were stored in drawable/, it would
                                   // be converted from 16-bit to 8. FIXME: Is
                                   // behavior still desirable now that we have
                                   // F16? b/119760146
                                   R.raw.f16 };
        // An opaque image can be converted to 565, but postProcess will promote
        // to 8888 in case alpha is added. The third image defaults to F16, so
        // even with postProcess it will only be promoted to 8888.
        boolean postProcessCancels[] = new boolean[] { true, true, false };
        boolean trueFalse[] = new boolean[] { true, false };
        SourceCreator f = mCreators[0];
        for (int i = 0; i < resIds.length; ++i) {
            int resId = resIds[i];
            l.doPostProcess = false;
            l.preferRamOverQuality = false;
            Bitmap normal = null;
            try {
                normal = ImageDecoder.decodeBitmap(f.apply(resId), l);
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
            int normalByteCount = normal.getAllocationByteCount();
            for (boolean doPostProcess : trueFalse) {
                l.doPostProcess = doPostProcess;
                l.preferRamOverQuality = true;
                Bitmap saveRamOverQuality = null;
                try {
                    saveRamOverQuality = ImageDecoder.decodeBitmap(f.apply(resId), l);
                } catch (IOException e) {
                    fail("Failed with exception " + e);
                }
                int saveByteCount = saveRamOverQuality.getAllocationByteCount();
                if (doPostProcess && postProcessCancels[i]) {
                    // Promoted to normal.
                    assertEquals(normalByteCount, saveByteCount);
                } else {
                    assertTrue(saveByteCount < normalByteCount);
                }
            }
        }
    }

    @Test
    public void testRespectOrientation() {
        boolean isWebp = false;
        // These 8 images test the 8 EXIF orientations. If the orientation is
        // respected, they all have the same dimensions: 100 x 80.
        // They are also identical (after adjusting), so compare them.
        Bitmap reference = null;
        for (int resId : new int[] { R.drawable.orientation_1,
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
        }) {
            if (resId == R.drawable.webp_orientation1) {
                // The webp files may not look exactly the same as the jpegs.
                // Recreate the reference.
                reference.recycle();
                reference = null;
                isWebp = true;
            }
            Uri uri = Utils.getAsResourceUri(resId);
            ImageDecoder.Source src = ImageDecoder.createSource(getContentResolver(), uri);
            try {
                Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                    // Use software allocator so we can compare.
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                });
                assertNotNull(bm);
                assertEquals(100, bm.getWidth());
                assertEquals(80,  bm.getHeight());

                if (reference == null) {
                    reference = bm;
                } else {
                    int mse = isWebp ? 70 : 1;
                    BitmapUtils.assertBitmapsMse(bm, reference, mse, true, false);
                    bm.recycle();
                }
            } catch (IOException e) {
                fail("Decoding " + uri.toString() + " yielded " + e);
            }
        }
    }

    @Test(expected = IOException.class)
    public void testZeroLengthByteBuffer() throws IOException {
        Drawable drawable = ImageDecoder.decodeDrawable(
            ImageDecoder.createSource(ByteBuffer.wrap(new byte[10], 0, 0)));
        fail("should not have reached here!");
    }

    private interface ByteBufferSupplier extends Supplier<ByteBuffer> {};

    @Test
    @Parameters(method = "getRecords")
    public void testOffsetByteArray(Record record) {
        int offset = 10;
        int extra = 15;
        byte[] array = getAsByteArray(record.resId, offset, extra);
        int length = array.length - extra - offset;
        // Used for SourceCreators that set both a position and an offset.
        int myOffset = 3;
        int myPosition = 7;
        assertEquals(offset, myOffset + myPosition);

        ByteBufferSupplier[] suppliers = new ByteBufferSupplier[] {
                // Internally, this gives the buffer a position, but not an offset.
                () -> ByteBuffer.wrap(array, offset, length),
                // Same, but make it readOnly to ensure that we test the
                // ByteBufferSource rather than the ByteArraySource.
                () -> ByteBuffer.wrap(array, offset, length).asReadOnlyBuffer(),
                () -> {
                    // slice() to give the buffer an offset.
                    ByteBuffer buf = ByteBuffer.wrap(array, 0, array.length - extra);
                    buf.position(offset);
                    return buf.slice();
                },
                () -> {
                    // Same, but make it readOnly to ensure that we test the
                    // ByteBufferSource rather than the ByteArraySource.
                    ByteBuffer buf = ByteBuffer.wrap(array, 0, array.length - extra);
                    buf.position(offset);
                    return buf.slice().asReadOnlyBuffer();
                },
                () -> {
                    // Use both a position and an offset.
                    ByteBuffer buf = ByteBuffer.wrap(array, myOffset,
                            array.length - extra - myOffset);
                    buf = buf.slice();
                    buf.position(myPosition);
                    return buf;
                },
                () -> {
                    // Same, as readOnly.
                    ByteBuffer buf = ByteBuffer.wrap(array, myOffset,
                            array.length - extra - myOffset);
                    buf = buf.slice();
                    buf.position(myPosition);
                    return buf.asReadOnlyBuffer();
                },
                () -> {
                    // Direct ByteBuffer with a position.
                    ByteBuffer buf = ByteBuffer.allocateDirect(array.length);
                    buf.put(array);
                    buf.position(offset);
                    return buf;
                },
                () -> {
                    // Sliced direct ByteBuffer, for an offset.
                    ByteBuffer buf = ByteBuffer.allocateDirect(array.length);
                    buf.put(array);
                    buf.position(offset);
                    return buf.slice();
                },
                () -> {
                    // Direct ByteBuffer with position and offset.
                    ByteBuffer buf = ByteBuffer.allocateDirect(array.length);
                    buf.put(array);
                    buf.position(myOffset);
                    buf = buf.slice();
                    buf.position(myPosition);
                    return buf;
                },
        };
        for (int i = 0; i < suppliers.length; ++i) {
            ByteBuffer buffer = suppliers[i].get();
            final int position = buffer.position();
            ImageDecoder.Source src = ImageDecoder.createSource(buffer);
            try {
                Drawable drawable = ImageDecoder.decodeDrawable(src);
                assertNotNull(drawable);
            } catch (IOException e) {
                fail("Failed with exception " + e);
            }
            assertEquals("Mismatch for supplier " + i,
                    position, buffer.position());
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testResourceSource(Record record) {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(), record.resId);
        try {
            Drawable drawable = ImageDecoder.decodeDrawable(src);
            assertNotNull(drawable);
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(record.resId) + " with " + e);
        }
    }

    private BitmapDrawable decodeBitmapDrawable(int resId) {
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(), resId);
        try {
            Drawable drawable = ImageDecoder.decodeDrawable(src);
            assertNotNull(drawable);
            assertTrue(drawable instanceof BitmapDrawable);
            return (BitmapDrawable) drawable;
        } catch (IOException e) {
            fail("Failed " + Utils.getAsResourceUri(resId) + " with " + e);
            return null;
        }
    }

    @Test
    @Parameters(method = "getRecords")
    public void testUpscale(Record record) {
        Resources res = getResources();
        final int originalDensity = res.getDisplayMetrics().densityDpi;

        try {
            final int resId = record.resId;

            // Set a high density. This will result in a larger drawable, but
            // not a larger Bitmap.
            res.getDisplayMetrics().densityDpi = DisplayMetrics.DENSITY_XXXHIGH;
            BitmapDrawable drawable = decodeBitmapDrawable(resId);

            Bitmap bm = drawable.getBitmap();
            assertEquals(record.width, bm.getWidth());
            assertEquals(record.height, bm.getHeight());

            assertTrue(drawable.getIntrinsicWidth() > record.width);
            assertTrue(drawable.getIntrinsicHeight() > record.height);

            // Set a low density. This will result in a smaller drawable and
            // Bitmap, unless the true density is DENSITY_MEDIUM, which matches
            // the density of the asset.
            res.getDisplayMetrics().densityDpi = DisplayMetrics.DENSITY_LOW;
            drawable = decodeBitmapDrawable(resId);
            bm = drawable.getBitmap();

            if (originalDensity == DisplayMetrics.DENSITY_MEDIUM) {
                // Although we've modified |densityDpi|, ImageDecoder knows the
                // true density matches the asset, so it will not downscale at
                // decode time.
                assertEquals(bm.getWidth(), record.width);
                assertEquals(bm.getHeight(), record.height);

                // The drawable should still be smaller.
                assertTrue(bm.getWidth() > drawable.getIntrinsicWidth());
                assertTrue(bm.getHeight() > drawable.getIntrinsicHeight());
            } else {
                // The bitmap is scaled down at decode time, so it matches the
                // drawable size, and is smaller than the original.
                assertTrue(bm.getWidth() < record.width);
                assertTrue(bm.getHeight() < record.height);

                assertEquals(bm.getWidth(), drawable.getIntrinsicWidth());
                assertEquals(bm.getHeight(), drawable.getIntrinsicHeight());
            }
        } finally {
            res.getDisplayMetrics().densityDpi = originalDensity;
        }
    }

    static class AssetRecord {
        public final String name;
        public final int width;
        public final int height;
        public final boolean isF16;
        public final boolean isGray;
        public final boolean hasAlpha;
        private final ColorSpace mColorSpace;

        AssetRecord(String name, int width, int height, boolean isF16,
                boolean isGray, boolean hasAlpha, ColorSpace colorSpace) {
            this.name = name;
            this.width = width;
            this.height = height;
            this.isF16 = isF16;
            this.isGray = isGray;
            this.hasAlpha = hasAlpha;
            mColorSpace = colorSpace;
        }

        public ColorSpace getColorSpace() {
            return mColorSpace;
        }

        public void checkColorSpace(ColorSpace requested, ColorSpace actual) {
            assertNotNull("Null ColorSpace for " + this.name, actual);
            if (this.isF16 && requested != null) {
                if (requested == ColorSpace.get(ColorSpace.Named.LINEAR_SRGB)) {
                    assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB), actual);
                } else if (requested == ColorSpace.get(ColorSpace.Named.SRGB)) {
                    assertSame(ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), actual);
                } else {
                    assertSame(requested, actual);
                }
            } else if (requested != null) {
                // If the asset is *not* 16 bit, requesting EXTENDED will promote to 16 bit.
                assertSame(requested, actual);
            } else if (mColorSpace == null) {
                assertEquals(this.name, "Unknown", actual.getName());
            } else {
                assertSame(this.name, mColorSpace, actual);
            }
        }
    }

    static Object[] getAssetRecords() {
        return new Object [] {
            // A null ColorSpace means that the color space is "Unknown".
            new AssetRecord("almost-red-adobe.png", 1, 1, false, false, false, null),
            new AssetRecord("green-p3.png", 64, 64, false, false, false,
                    ColorSpace.get(ColorSpace.Named.DISPLAY_P3)),
            new AssetRecord("green-srgb.png", 64, 64, false, false, false, sSRGB),
            new AssetRecord("blue-16bit-prophoto.png", 100, 100, true, false, true,
                    ColorSpace.get(ColorSpace.Named.PRO_PHOTO_RGB)),
            new AssetRecord("blue-16bit-srgb.png", 64, 64, true, false, false,
                    ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB)),
            new AssetRecord("purple-cmyk.png", 64, 64, false, false, false, sSRGB),
            new AssetRecord("purple-displayprofile.png", 64, 64, false, false, false, null),
            new AssetRecord("red-adobergb.png", 64, 64, false, false, false,
                    ColorSpace.get(ColorSpace.Named.ADOBE_RGB)),
            new AssetRecord("translucent-green-p3.png", 64, 64, false, false, true,
                    ColorSpace.get(ColorSpace.Named.DISPLAY_P3)),
            new AssetRecord("grayscale-linearSrgb.png", 32, 32, false, true, false,
                    ColorSpace.get(ColorSpace.Named.LINEAR_SRGB)),
            new AssetRecord("grayscale-16bit-linearSrgb.png", 32, 32, true, false, true,
                    ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB)),
        };
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testAssetSource(AssetRecord record) {
        AssetManager assets = getResources().getAssets();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        try {
            Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                if (record.isF16) {
                    // CTS infrastructure fails to create F16 HARDWARE Bitmaps, so this
                    // switches to using software.
                    decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                }

                record.checkColorSpace(null, info.getColorSpace());
            });
            assertEquals(record.name, record.width, bm.getWidth());
            assertEquals(record.name, record.height, bm.getHeight());
            record.checkColorSpace(null, bm.getColorSpace());
            assertEquals(record.hasAlpha, bm.hasAlpha());
        } catch (IOException e) {
            fail("Failed to decode asset " + record.name + " with " + e);
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testTargetColorSpace(AssetRecord record) {
        AssetManager assets = getResources().getAssets();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        for (ColorSpace cs : BitmapTest.getRgbColorSpaces()) {
            try {
                Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                    if (record.isF16 || isExtended(cs)) {
                        // CTS infrastructure and some devices fail to create F16
                        // HARDWARE Bitmaps, so this switches to using software.
                        decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                    }
                    decoder.setTargetColorSpace(cs);
                });
                record.checkColorSpace(cs, bm.getColorSpace());
            } catch (IOException e) {
                fail("Failed to decode asset " + record.name + " to " + cs + " with " + e);
            }
        }
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testTargetColorSpaceNoF16HARDWARE(AssetRecord record) {
        final ColorSpace EXTENDED_SRGB = ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB);
        final ColorSpace LINEAR_EXTENDED_SRGB = ColorSpace.get(
                ColorSpace.Named.LINEAR_EXTENDED_SRGB);
        AssetManager assets = getResources().getAssets();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        for (ColorSpace cs : new ColorSpace[] { EXTENDED_SRGB, LINEAR_EXTENDED_SRGB }) {
            try {
                Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                    decoder.setTargetColorSpace(cs);
                });
                // If the ColorSpace does not match the request, it should be because
                // F16 + HARDWARE is not supported. In that case, it should match the non-
                // EXTENDED variant.
                ColorSpace actual = bm.getColorSpace();
                if (actual != cs) {
                    assertEquals(BitmapTest.ANDROID_BITMAP_FORMAT_RGBA_8888,
                                 BitmapTest.nGetFormat(bm));
                    if (cs == EXTENDED_SRGB) {
                        assertSame(ColorSpace.get(ColorSpace.Named.SRGB), actual);
                    } else {
                        assertSame(ColorSpace.get(ColorSpace.Named.LINEAR_SRGB), actual);
                    }
                }
            } catch (IOException e) {
                fail("Failed to decode asset " + record.name + " to " + cs + " with " + e);
            }
        }
    }

    private boolean isExtended(ColorSpace colorSpace) {
        return colorSpace == ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB)
            || colorSpace == ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB);
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testTargetColorSpaceUpconvert(AssetRecord record) {
        // Verify that decoding an asset to EXTENDED upconverts to F16.
        AssetManager assets = getResources().getAssets();
        boolean[] trueFalse = new boolean[] { true, false };
        final ColorSpace linearExtended = ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB);
        final ColorSpace linearSrgb = ColorSpace.get(ColorSpace.Named.LINEAR_SRGB);

        if (record.isF16) {
            // These assets decode to F16 by default.
            return;
        }
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        for (ColorSpace cs : BitmapTest.getRgbColorSpaces()) {
            for (boolean alphaMask : trueFalse) {
                try {
                    Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                        // Force software so we can check the Config.
                        decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                        decoder.setTargetColorSpace(cs);
                        // This has no effect on non-gray assets.
                        decoder.setDecodeAsAlphaMaskEnabled(alphaMask);
                    });

                    if (record.isGray && alphaMask) {
                        assertSame(Bitmap.Config.ALPHA_8, bm.getConfig());
                        assertNull(bm.getColorSpace());
                    } else {
                        assertSame(cs, bm.getColorSpace());
                        if (isExtended(cs)) {
                            assertSame(Bitmap.Config.RGBA_F16, bm.getConfig());
                        } else {
                            assertSame(Bitmap.Config.ARGB_8888, bm.getConfig());
                        }
                    }
                } catch (IOException e) {
                    fail("Failed to decode asset " + record.name + " to " + cs + " with " + e);
                }

                // Using MEMORY_POLICY_LOW_RAM prevents upconverting.
                try {
                    Bitmap bm = ImageDecoder.decodeBitmap(src, (decoder, info, s) -> {
                        // Force software so we can check the Config.
                        decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                        decoder.setTargetColorSpace(cs);
                        decoder.setMemorySizePolicy(ImageDecoder.MEMORY_POLICY_LOW_RAM);
                        // This has no effect on non-gray assets.
                        decoder.setDecodeAsAlphaMaskEnabled(alphaMask);
                    });

                    assertNotEquals(Bitmap.Config.RGBA_F16, bm.getConfig());

                    if (record.isGray && alphaMask) {
                        assertSame(Bitmap.Config.ALPHA_8, bm.getConfig());
                        assertNull(bm.getColorSpace());
                    } else {
                        ColorSpace actual = bm.getColorSpace();
                        if (isExtended(cs)) {
                            if (cs == ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB)) {
                                assertSame(ColorSpace.get(ColorSpace.Named.SRGB), actual);
                            } else if (cs == linearExtended) {
                                assertSame(linearSrgb, actual);
                            } else {
                                fail("Test error: did isExtended() change?");
                            }
                        } else {
                            assertSame(cs, actual);
                            if (bm.hasAlpha()) {
                                assertSame(Bitmap.Config.ARGB_8888, bm.getConfig());
                            } else {
                                assertSame(Bitmap.Config.RGB_565, bm.getConfig());
                            }
                        }
                    }
                } catch (IOException e) {
                    fail("Failed to decode asset " + record.name
                            + " with MEMORY_POLICY_LOW_RAM to " + cs + " with " + e);
                }
            }
        }
    }

    @Test
    public void testTargetColorSpaceIllegal() {
        ColorSpace noTransferParamsCS = new ColorSpace.Rgb("NoTransferParams",
                new float[]{ 0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f },
                ColorSpace.ILLUMINANT_D50,
                x -> Math.pow(x, 1.0f / 2.2f), x -> Math.pow(x, 2.2f),
                0, 1);
        for (int resId : new int[] { R.drawable.png_test, R.drawable.animated }) {
            ImageDecoder.Source src = mCreators[0].apply(resId);
            for (ColorSpace cs : new ColorSpace[] {
                    ColorSpace.get(ColorSpace.Named.CIE_LAB),
                    ColorSpace.get(ColorSpace.Named.CIE_XYZ),
                    noTransferParamsCS,
            }) {
                try {
                    ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                        decoder.setTargetColorSpace(cs);
                    });
                    fail("Should have thrown an IllegalArgumentException for setTargetColorSpace("
                            + cs + ")!");
                } catch (IOException e) {
                    fail("Failed to decode png_test with " + e);
                } catch (IllegalArgumentException illegal) {
                    // This is expected.
                }
            }
        }
    }

    private Bitmap drawToBitmap(Drawable dr) {
        Bitmap bm = Bitmap.createBitmap(dr.getIntrinsicWidth(), dr.getIntrinsicHeight(),
                Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bm);
        dr.draw(canvas);
        return bm;
    }

    private void testReuse(ImageDecoder.Source src, String name) {
        Drawable first = null;
        try {
            first = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
            });
        } catch (IOException e) {
            fail("Failed on first decode of " + name + " using " + src + "!");
        }

        Drawable second = null;
        try {
            second = ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
            });
        } catch (IOException e) {
            fail("Failed on second decode of " + name + " using " + src + "!");
        }

        assertEquals(first.getIntrinsicWidth(), second.getIntrinsicWidth());
        assertEquals(first.getIntrinsicHeight(), second.getIntrinsicHeight());

        Bitmap bm1 = drawToBitmap(first);
        Bitmap bm2 = drawToBitmap(second);
        assertTrue(BitmapUtils.compareBitmaps(bm1, bm2));
    }

    @Test
    public void testJpegInfiniteLoop() {
        ImageDecoder.Source src = mCreators[0].apply(R.raw.b78329453);
        try {
            ImageDecoder.decodeDrawable(src, (decoder, info, s) -> {
                decoder.setTargetSampleSize(19);
            });
        } catch (IOException e) {
            fail();
        }
    }

    private Object[] getRecordsAsSources() {
        return Utils.crossProduct(getRecords(), mCreators);
    }

    @Test
    @LargeTest
    @Parameters(method = "getRecordsAsSources")
    public void testReuse(Record record, SourceCreator f) {
        if (record.mimeType.equals("image/heif")) {
            // This image takes too long for this test.
            return;
        }

        String name = Utils.getAsResourceUri(record.resId).toString();
        ImageDecoder.Source src = f.apply(record.resId);
        testReuse(src, name);
    }

    @Test
    @Parameters(method = "getRecords")
    public void testReuse2(Record record) {
        if (record.mimeType.equals("image/heif")) {
            // This image takes too long for this test.
            return;
        }

        String name = Utils.getAsResourceUri(record.resId).toString();
        ImageDecoder.Source src = ImageDecoder.createSource(getResources(), record.resId);
        testReuse(src, name);

        src = ImageDecoder.createSource(getAsFile(record.resId));
        testReuse(src, name);
    }

    private Object[] getRecordsAsUris() {
        return Utils.crossProduct(getRecords(), mUriCreators);
    }


    @Test
    @Parameters(method = "getRecordsAsUris")
    public void testReuseUri(Record record, UriCreator f) {
        if (record.mimeType.equals("image/heif")) {
            // This image takes too long for this test.
            return;
        }

        Uri uri = f.apply(record.resId);
        ImageDecoder.Source src = ImageDecoder.createSource(getContentResolver(), uri);
        testReuse(src, uri.toString());
    }

    @Test
    @Parameters(method = "getAssetRecords")
    public void testReuseAssetRecords(AssetRecord record) {
        AssetManager assets = getResources().getAssets();
        ImageDecoder.Source src = ImageDecoder.createSource(assets, record.name);
        testReuse(src, record.name);
    }


    @Test
    public void testReuseAnimated() {
        ImageDecoder.Source src = mCreators[0].apply(R.drawable.animated);
        testReuse(src, "animated.gif");
    }

    @Test
    public void testIsMimeTypeSupported() {
        for (Object r : getRecords()) {
            Record record = (Record) r;
            assertTrue(record.mimeType, ImageDecoder.isMimeTypeSupported(record.mimeType));
        }

        for (String mimeType : new String[] {
                "image/heic",
                "image/vnd.wap.wbmp",
                "image/x-sony-arw",
                "image/x-canon-cr2",
                "image/x-adobe-dng",
                "image/x-nikon-nef",
                "image/x-nikon-nrw",
                "image/x-olympus-orf",
                "image/x-fuji-raf",
                "image/x-panasonic-rw2",
                "image/x-pentax-pef",
                "image/x-samsung-srw",
        }) {
            assertTrue(mimeType, ImageDecoder.isMimeTypeSupported(mimeType));
        }

        assertFalse(ImageDecoder.isMimeTypeSupported("image/x-does-not-exist"));
    }

    @Test(expected = FileNotFoundException.class)
    public void testBadUri() throws IOException {
        Uri uri = new Uri.Builder()
                .scheme(ContentResolver.SCHEME_ANDROID_RESOURCE)
                .authority("authority")
                .appendPath("drawable")
                .appendPath("bad")
                .build();
        ImageDecoder.Source src = ImageDecoder.createSource(getContentResolver(), uri);
        ImageDecoder.decodeDrawable(src);
    }

    @Test(expected = FileNotFoundException.class)
    public void testBadUri2() throws IOException {
        // This URI will attempt to open a file from EmptyProvider, which always
        // returns null. This test ensures that we throw FileNotFoundException,
        // instead of a NullPointerException when attempting to dereference null.
        Uri uri = Uri.parse(ContentResolver.SCHEME_CONTENT + "://"
                + "android.graphics.cts.assets/bad");
        ImageDecoder.Source src = ImageDecoder.createSource(getContentResolver(), uri);
        ImageDecoder.decodeDrawable(src);
    }

    @Test(expected = FileNotFoundException.class)
    public void testUriWithoutScheme() throws IOException {
        Uri uri = new Uri.Builder()
                .authority("authority")
                .appendPath("missing")
                .appendPath("scheme")
                .build();
        ImageDecoder.Source src = ImageDecoder.createSource(getContentResolver(), uri);
        ImageDecoder.decodeDrawable(src);
    }

    @Test(expected = FileNotFoundException.class)
    public void testBadCallable() throws IOException {
        ImageDecoder.Source src = ImageDecoder.createSource(() -> null);
        ImageDecoder.decodeDrawable(src);
    }
}
