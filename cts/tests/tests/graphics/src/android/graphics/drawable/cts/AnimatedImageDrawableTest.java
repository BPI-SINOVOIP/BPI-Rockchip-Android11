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

package android.graphics.drawable.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.ImageDecoder;
import android.graphics.LightingColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.cts.R;
import android.graphics.cts.Utils;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.View;
import android.widget.ImageView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.FlakyTest;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.BitmapUtils;
import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.WidgetTestUtils;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.function.BiFunction;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

@RunWith(JUnitParamsRunner.class)
public class AnimatedImageDrawableTest {
    private ImageView mImageView;

    private static final int RES_ID = R.drawable.animated;
    private static final int WIDTH = 278;
    private static final int HEIGHT = 183;
    private static final int NUM_FRAMES = 4;
    private static final int FRAME_DURATION = 250; // in milliseconds
    private static final int DURATION = NUM_FRAMES * FRAME_DURATION;

    @Rule
    public ActivityTestRule<AnimatedImageActivity> mActivityRule =
            new ActivityTestRule<AnimatedImageActivity>(AnimatedImageActivity.class);
    private Activity mActivity;

    private Resources getResources() {
        return InstrumentationRegistry.getTargetContext().getResources();
    }

    private ContentResolver getContentResolver() {
        return InstrumentationRegistry.getTargetContext().getContentResolver();
    }

    private void setupActivity() {
        mActivity = mActivityRule.getActivity();
        PollingCheck.waitFor(mActivity::hasWindowFocus);
        mImageView = mActivity.findViewById(R.id.animated_image);
    }

    @Test
    public void testEmptyConstructor() {
        new AnimatedImageDrawable();
    }

    @Test
    public void testMutate() {
        Resources res = getResources();
        AnimatedImageDrawable aid1 = (AnimatedImageDrawable) res.getDrawable(R.drawable.animated);
        AnimatedImageDrawable aid2 = (AnimatedImageDrawable) res.getDrawable(R.drawable.animated);

        final int originalAlpha = aid1.getAlpha();
        assertEquals(255, originalAlpha);
        assertEquals(255, aid2.getAlpha());

        try {
            aid1.mutate();
            aid1.setAlpha(100);
            assertEquals(originalAlpha, aid2.getAlpha());
        } finally {
            res.getDrawable(R.drawable.animated).setAlpha(originalAlpha);
        }
    }

    private AnimatedImageDrawable createFromImageDecoder(int resId) {
        Uri uri = null;
        try {
            uri = Utils.getAsResourceUri(resId);
            ImageDecoder.Source source = ImageDecoder.createSource(getContentResolver(), uri);
            Drawable drawable = ImageDecoder.decodeDrawable(source);
            assertTrue(drawable instanceof AnimatedImageDrawable);
            return (AnimatedImageDrawable) drawable;
        } catch (IOException e) {
            fail("failed to create image from " + uri);
            return null;
        }
    }

    @Test
    public void testDecodeAnimatedImageDrawable() {
        Drawable drawable = createFromImageDecoder(RES_ID);
        assertEquals(WIDTH,  drawable.getIntrinsicWidth());
        assertEquals(HEIGHT, drawable.getIntrinsicHeight());
    }

    private static class Callback extends Animatable2Callback {
        private final Drawable mDrawable;

        public Callback(Drawable d) {
            mDrawable = d;
        }

        @Override
        public void onAnimationStart(Drawable drawable) {
            assertNotNull(drawable);
            assertEquals(mDrawable, drawable);
            super.onAnimationStart(drawable);
        }

        @Override
        public void onAnimationEnd(Drawable drawable) {
            assertNotNull(drawable);
            assertEquals(mDrawable, drawable);
            super.onAnimationEnd(drawable);
        }
    };

    @Test(expected=IllegalStateException.class)
    public void testRegisterWithoutLooper() {
        AnimatedImageDrawable drawable = createFromImageDecoder(R.drawable.animated);

        // registerAnimationCallback must be run on a thread with a Looper,
        // which the test thread does not have.
        Callback cb = new Callback(drawable);
        drawable.registerAnimationCallback(cb);
    }

    @Test
    public void testRegisterCallback() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(R.drawable.animated);

        mActivityRule.runOnUiThread(() -> {
            // Register a callback.
            Callback cb = new Callback(drawable);
            drawable.registerAnimationCallback(cb);
            assertTrue(drawable.unregisterAnimationCallback(cb));

            // Now that it has been removed, it cannot be removed again.
            assertFalse(drawable.unregisterAnimationCallback(cb));
        });
    }

    @Test
    public void testClearCallbacks() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(R.drawable.animated);

        Callback[] callbacks = new Callback[] {
            new Callback(drawable),
            new Callback(drawable),
            new Callback(drawable),
            new Callback(drawable),
            new Callback(drawable),
            new Callback(drawable),
            new Callback(drawable),
            new Callback(drawable),
        };

        mActivityRule.runOnUiThread(() -> {
            for (Callback cb : callbacks) {
                drawable.registerAnimationCallback(cb);
            }
        });

        drawable.clearAnimationCallbacks();

        for (Callback cb : callbacks) {
            // It has already been removed.
            assertFalse(drawable.unregisterAnimationCallback(cb));
        }
    }

    @Test
    public void testUnregisterCallback() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(R.drawable.animated);

        Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.registerAnimationCallback(cb);
            assertTrue(drawable.unregisterAnimationCallback(cb));
            drawable.setRepeatCount(0);
            drawable.start();
        });

        cb.waitForStart();
        cb.assertStarted(false);

        cb.waitForEnd(DURATION * 2);
        cb.assertEnded(false);
    }

    @Test
    @FlakyTest (bugId = 120280954)
    public void testLifeCycle() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);

        // Only run the animation one time.
        drawable.setRepeatCount(0);

        Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.registerAnimationCallback(cb);
        });

        assertFalse(drawable.isRunning());
        cb.assertStarted(false);
        cb.assertEnded(false);

        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            drawable.start();
            assertTrue(drawable.isRunning());
        });
        cb.waitForStart();
        cb.assertStarted(true);

        // FIXME: Now that it seems the reason for the flakiness has been solved (b/129400990),
        // reduce this extra duration workaround.
        // Extra time, to wait for the message to post.
        cb.waitForEnd(DURATION * 20);
        cb.assertEnded(true);
        assertFalse(drawable.isRunning());
    }

    @Test
    public void testLifeCycleSoftware() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);

        Bitmap bm = Bitmap.createBitmap(drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight(),
                Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bm);

        Callback cb = new Callback(drawable);
        mActivityRule.runOnUiThread(() -> {
            drawable.registerAnimationCallback(cb);
            drawable.draw(canvas);
        });

        assertFalse(drawable.isRunning());
        cb.assertStarted(false);
        cb.assertEnded(false);

        mActivityRule.runOnUiThread(() -> {
            drawable.start();
            assertTrue(drawable.isRunning());
            drawable.draw(canvas);
        });
        cb.waitForStart();
        cb.assertStarted(true);

        // Only run the animation one time.
        drawable.setRepeatCount(0);

        // The drawable will prevent skipping frames, so we actually have to
        // draw each frame. (Start with 1, since we already drew frame 0.)
        for (int i = 1; i < NUM_FRAMES; i++) {
            cb.waitForEnd(FRAME_DURATION);
            cb.assertEnded(false);
            mActivityRule.runOnUiThread(() -> {
                assertTrue(drawable.isRunning());
                drawable.draw(canvas);
            });
        }

        cb.waitForEnd(FRAME_DURATION);
        assertFalse(drawable.isRunning());
        cb.assertEnded(true);
    }

    @Test
    @FlakyTest (bugId = 72737527)
    public void testAddCallbackAfterStart() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.setRepeatCount(0);
            drawable.start();
            drawable.registerAnimationCallback(cb);
        });

        // FIXME: Now that it seems the reason for the flakiness has been solved (b/129400990),
        // reduce this extra duration workaround.
        // Add extra duration to wait for the message posted by the end of the
        // animation. This should help fix flakiness.
        cb.waitForEnd(DURATION * 10);
        cb.assertEnded(true);
    }

    @Test
    public void testStop() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.registerAnimationCallback(cb);

            drawable.start();
            assertTrue(drawable.isRunning());
        });

        cb.waitForStart();
        cb.assertStarted(true);

        mActivityRule.runOnUiThread(() -> {
            drawable.stop();
            assertFalse(drawable.isRunning());
        });

        // This duration may be overkill, but we need to wait for the message
        // to post. Increasing it should help with flakiness on bots.
        cb.waitForEnd(DURATION * 3);
        cb.assertEnded(true);
    }

    @Test
    @FlakyTest (bugId = 72737527)
    @Parameters({ "3", "5", "7", "16" })
    public void testRepeatCounts(int repeatCount) throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        assertEquals(AnimatedImageDrawable.REPEAT_INFINITE, drawable.getRepeatCount());

        Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.registerAnimationCallback(cb);
            drawable.setRepeatCount(repeatCount);
            assertEquals(repeatCount, drawable.getRepeatCount());
            drawable.start();
        });

        cb.waitForStart();
        cb.assertStarted(true);

        // The animation runs repeatCount + 1 total times.
        cb.waitForEnd(DURATION * repeatCount);
        cb.assertEnded(false);

        // FIXME: Now that it seems the reason for the flakiness has been solved (b/129400990),
        // reduce this extra duration workaround.
        cb.waitForEnd(DURATION * 20);
        cb.assertEnded(true);

        drawable.setRepeatCount(AnimatedImageDrawable.REPEAT_INFINITE);
        assertEquals(AnimatedImageDrawable.REPEAT_INFINITE, drawable.getRepeatCount());
    }

    @Test
    public void testRepeatCountInfinite() throws Throwable {
        setupActivity();
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.registerAnimationCallback(cb);
            drawable.setRepeatCount(AnimatedImageDrawable.REPEAT_INFINITE);
            drawable.start();
        });

        // There is no way to truly test infinite, but let it run for a long
        // time and verify that it's still running.
        cb.waitForEnd(DURATION * 30);
        cb.assertEnded(false);
        assertTrue(drawable.isRunning());
    }

    private static Object[] parametersForTestEncodedRepeats() {
        return new Object[] {
            new Object[] { R.drawable.animated, AnimatedImageDrawable.REPEAT_INFINITE },
            new Object[] { R.drawable.animated_one_loop, 1 },
            new Object[] { R.drawable.webp_animated, AnimatedImageDrawable.REPEAT_INFINITE },
            new Object[] { R.drawable.webp_animated_large, AnimatedImageDrawable.REPEAT_INFINITE },
            new Object[] { R.drawable.webp_animated_icc_xmp, 31999 },
            new Object[] { R.drawable.count_down_color_test, 0 },
        };
    }

    @Test
    @Parameters(method = "parametersForTestEncodedRepeats")
    public void testEncodedRepeats(int resId, int expectedRepeatCount) {
        AnimatedImageDrawable drawable = createFromImageDecoder(resId);
        assertEquals(expectedRepeatCount, drawable.getRepeatCount());
    }

    @Test
    public void testGetOpacity() {
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        assertEquals(PixelFormat.TRANSLUCENT, drawable.getOpacity());
    }

    @Test
    public void testColorFilter() {
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);

        ColorFilter filter = new LightingColorFilter(0, Color.RED);
        drawable.setColorFilter(filter);
        assertEquals(filter, drawable.getColorFilter());

        Bitmap actual = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        {
            Canvas canvas = new Canvas(actual);
            drawable.draw(canvas);
        }

        for (int i = 0; i < actual.getWidth(); ++i) {
            for (int j = 0; j < actual.getHeight(); ++j) {
                int color = actual.getPixel(i, j);
                // The LightingColorFilter does not affect the transparent pixels,
                // so all pixels should either remain transparent or turn red.
                if (color != Color.RED && color != Color.TRANSPARENT) {
                    fail("pixel at " + i + ", " + j + " does not match expected. "
                            + "expected: " + Color.RED + " OR " + Color.TRANSPARENT
                            + " actual: " + color);
                }
            }
        }
    }

    @Test
    public void testPostProcess() {
        // Compare post processing a Rect in the middle of the (not-animating)
        // image with drawing manually. They should be exactly the same.
        BiFunction<Integer, Integer, Rect> rectCreator = (width, height) -> {
            int quarterWidth  = width  / 4;
            int quarterHeight = height / 4;
            return new Rect(quarterWidth, quarterHeight,
                    3 * quarterWidth, 3 * quarterHeight);
        };

        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        Bitmap expected = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);

        Paint paint = new Paint();
        paint.setColor(Color.RED);

        {
            Rect r = rectCreator.apply(drawable.getIntrinsicWidth(),
                                       drawable.getIntrinsicHeight());
            Canvas canvas = new Canvas(expected);
            drawable.draw(canvas);

            for (int i = r.left; i < r.right; ++i) {
                for (int j = r.top; j < r.bottom; ++j) {
                    assertNotEquals(Color.RED, expected.getPixel(i, j));
                }
            }

            canvas.drawRect(r, paint);

            for (int i = r.left; i < r.right; ++i) {
                for (int j = r.top; j < r.bottom; ++j) {
                    assertEquals(Color.RED, expected.getPixel(i, j));
                }
            }
        }


        AnimatedImageDrawable testDrawable = null;
        Uri uri = null;
        try {
            uri = Utils.getAsResourceUri(RES_ID);
            ImageDecoder.Source source = ImageDecoder.createSource(getContentResolver(), uri);
            Drawable dr = ImageDecoder.decodeDrawable(source, (decoder, info, src) -> {
                decoder.setPostProcessor((canvas) -> {
                    canvas.drawRect(rectCreator.apply(canvas.getWidth(),
                                                      canvas.getHeight()), paint);
                    return PixelFormat.TRANSLUCENT;
                });
            });
            assertTrue(dr instanceof AnimatedImageDrawable);
            testDrawable = (AnimatedImageDrawable) dr;
        } catch (IOException e) {
            fail("failed to create image from " + uri);
        }

        Bitmap actual = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);

        {
            Canvas canvas = new Canvas(actual);
            testDrawable.draw(canvas);
        }

        assertTrue(BitmapUtils.compareBitmaps(expected, actual));
    }

    @Test
    public void testCreateFromXml() throws XmlPullParserException, IOException {
        Resources res = getResources();
        XmlPullParser parser = res.getXml(R.drawable.animatedimagedrawable_tag);
        Drawable drawable = Drawable.createFromXml(res, parser);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);
    }

    @Test
    public void testCreateFromXmlClass() throws XmlPullParserException, IOException {
        Resources res = getResources();
        XmlPullParser parser = res.getXml(R.drawable.animatedimagedrawable);
        Drawable drawable = Drawable.createFromXml(res, parser);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);
    }

    @Test
    public void testCreateFromXmlClassAttribute() throws XmlPullParserException, IOException {
        Resources res = getResources();
        XmlPullParser parser = res.getXml(R.drawable.animatedimagedrawable_class);
        Drawable drawable = Drawable.createFromXml(res, parser);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);
    }

    @Test(expected=XmlPullParserException.class)
    public void testMissingSrcInflate() throws XmlPullParserException, IOException  {
        Resources res = getResources();
        XmlPullParser parser = res.getXml(R.drawable.animatedimagedrawable_nosrc);
        Drawable drawable = Drawable.createFromXml(res, parser);
    }

    @Test
    public void testAutoMirrored() {
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        assertFalse(drawable.isAutoMirrored());

        drawable.setAutoMirrored(true);
        assertTrue(drawable.isAutoMirrored());

        drawable.setAutoMirrored(false);
        assertFalse(drawable.isAutoMirrored());
    }

    @Test
    public void testAutoMirroredFromXml() throws XmlPullParserException, IOException {
        AnimatedImageDrawable drawable = parseXml(R.drawable.animatedimagedrawable_tag);
        assertFalse(drawable.isAutoMirrored());

        drawable = parseXml(R.drawable.animatedimagedrawable_automirrored);
        assertTrue(drawable.isAutoMirrored());
    }

    private AnimatedImageDrawable parseXml(int resId) throws XmlPullParserException, IOException {
        Resources res = getResources();
        XmlPullParser parser = res.getXml(resId);
        Drawable drawable = Drawable.createFromXml(res, parser);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);
        return (AnimatedImageDrawable) drawable;
    }

    @Test
    public void testAutoStartFromXml() throws XmlPullParserException, IOException {
        AnimatedImageDrawable drawable = parseXml(R.drawable.animatedimagedrawable_tag);
        assertFalse(drawable.isRunning());

        drawable = parseXml(R.drawable.animatedimagedrawable_autostart_false);
        assertFalse(drawable.isRunning());

        drawable = parseXml(R.drawable.animatedimagedrawable_autostart);
        assertTrue(drawable.isRunning());
    }

    private void drawAndCompare(Bitmap expected, Drawable drawable) {
        Bitmap test = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(test);
        drawable.draw(canvas);
        assertTrue(BitmapUtils.compareBitmaps(expected, test));
    }

    @Test
    public void testAutoMirroredDrawing() {
        AnimatedImageDrawable drawable = createFromImageDecoder(RES_ID);
        assertFalse(drawable.isAutoMirrored());

        final int width = drawable.getIntrinsicWidth();
        final int height = drawable.getIntrinsicHeight();
        Bitmap normal = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        {
            Canvas canvas = new Canvas(normal);
            drawable.draw(canvas);
        }

        Bitmap flipped = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        {
            Canvas canvas = new Canvas(flipped);
            canvas.translate(width, 0);
            canvas.scale(-1, 1);
            drawable.draw(canvas);
        }

        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {
                assertEquals(normal.getPixel(i, j), flipped.getPixel(width - 1 - i, j));
            }
        }

        drawable.setAutoMirrored(true);
        drawAndCompare(normal, drawable);

        drawable.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        drawAndCompare(flipped, drawable);

        drawable.setAutoMirrored(false);
        drawAndCompare(normal, drawable);
    }

    @Test
    public void testRepeatCountFromXml() throws XmlPullParserException, IOException {
        Resources res = getResources();
        XmlPullParser parser = res.getXml(R.drawable.animatedimagedrawable_loop_count);
        Drawable drawable = Drawable.createFromXml(res, parser);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);

        AnimatedImageDrawable aid = (AnimatedImageDrawable) drawable;
        assertEquals(17, aid.getRepeatCount());
    }

    @Test
    public void testInfiniteRepeatCountFromXml() throws XmlPullParserException, IOException {
        // This image has an encoded repeat count of 1. Verify that.
        Resources res = getResources();
        Drawable drawable = res.getDrawable(R.drawable.animated_one_loop);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);
        AnimatedImageDrawable aid = (AnimatedImageDrawable) drawable;
        assertEquals(1, aid.getRepeatCount());

        // This layout uses the same image and overrides the repeat count to infinity.
        XmlPullParser parser = res.getXml(R.drawable.animatedimagedrawable_loop_count_infinite);
        drawable = Drawable.createFromXml(res, parser);
        assertNotNull(drawable);
        assertTrue(drawable instanceof AnimatedImageDrawable);

        aid = (AnimatedImageDrawable) drawable;
        assertEquals(AnimatedImageDrawable.REPEAT_INFINITE, aid.getRepeatCount());
    }

    // Verify that decoding on the AnimatedImageThread works.
    private void decodeInBackground(AnimatedImageDrawable drawable) throws Throwable {
        final Callback cb = new Callback(drawable);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mImageView, () -> {
            mImageView.setImageDrawable(drawable);

            drawable.registerAnimationCallback(cb);
            drawable.start();
        });

        // The first frame was decoded in the thread that created the
        // AnimatedImageDrawable. Wait long enough to decode further threads on
        // the AnimatedImageThread, which was not created with a JNI interface
        // pointer.
        cb.waitForStart();
        cb.waitForEnd(DURATION * 2);
    }

    @Test
    public void testInputStream() throws Throwable {
        setupActivity();
        Resources res = getResources();
        try (InputStream in = res.openRawResource(R.drawable.animated)) {
            ImageDecoder.Source src =
                    ImageDecoder.createSource(res, in, Bitmap.DENSITY_NONE);
            AnimatedImageDrawable drawable =
                    (AnimatedImageDrawable) ImageDecoder.decodeDrawable(src);
            decodeInBackground(drawable);
        }

    }

    private byte[] getAsByteArray() {
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        try (InputStream in = getResources().openRawResource(RES_ID)) {
            byte[] buf = new byte[4096];
            int bytesRead;
            while ((bytesRead = in.read(buf)) != -1) {
                outputStream.write(buf, 0, bytesRead);
            }
        } catch (IOException e) {
            fail("Failed to read resource: " + e);
        }

        return outputStream.toByteArray();
    }

    private ByteBuffer getAsDirectByteBuffer() {
        byte[] array = getAsByteArray();
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(array.length);
        byteBuffer.put(array);
        byteBuffer.position(0);
        return byteBuffer;
    }

    private AnimatedImageDrawable createFromByteBuffer(ByteBuffer byteBuffer) {
        ImageDecoder.Source src = ImageDecoder.createSource(byteBuffer);
        try {
            return (AnimatedImageDrawable) ImageDecoder.decodeDrawable(src);
        } catch (IOException e) {
            fail("Failed to create decoder: " + e);
            return null;
        }
    }

    @Test
    public void testByteBuffer() throws Throwable {
        setupActivity();
        // Natively, this tests ByteArrayStream.
        byte[] array = getAsByteArray();
        ByteBuffer byteBuffer = ByteBuffer.wrap(array);
        final AnimatedImageDrawable drawable = createFromByteBuffer(byteBuffer);
        decodeInBackground(drawable);
    }

    @Test
    public void testReadOnlyByteBuffer() throws Throwable {
        setupActivity();
        // Natively, this tests ByteBufferStream.
        byte[] array = getAsByteArray();
        ByteBuffer byteBuffer = ByteBuffer.wrap(array).asReadOnlyBuffer();
        final AnimatedImageDrawable drawable = createFromByteBuffer(byteBuffer);
        decodeInBackground(drawable);
    }

    @Test
    public void testDirectByteBuffer() throws Throwable {
        setupActivity();
        ByteBuffer byteBuffer = getAsDirectByteBuffer();
        final AnimatedImageDrawable drawable = createFromByteBuffer(byteBuffer);
        decodeInBackground(drawable);
    }
}
