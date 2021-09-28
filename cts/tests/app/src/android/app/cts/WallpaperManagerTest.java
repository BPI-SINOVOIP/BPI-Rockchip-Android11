/*
 * Copyright (C) 2011 The Android Open Source Project
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

package android.app.cts;

import static android.opengl.cts.Egl14Utils.getMaxTextureSize;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.WallpaperColors;
import android.app.WallpaperManager;
import android.app.stubs.R;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.Point;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import java.io.IOException;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class WallpaperManagerTest {

    private static final boolean DEBUG = false;
    private static final String TAG = "WallpaperManagerTest";

    private WallpaperManager mWallpaperManager;
    private Context mContext;
    private Handler mHandler;
    private BroadcastReceiver mBroadcastReceiver;
    private CountDownLatch mCountDownLatch;
    private boolean mEnableWcg;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mWallpaperManager = WallpaperManager.getInstance(mContext);
        assumeTrue("Device does not support wallpapers", mWallpaperManager.isWallpaperSupported());

        MockitoAnnotations.initMocks(this);
        final HandlerThread handlerThread = new HandlerThread("TestCallbacks");
        handlerThread.start();
        mHandler = new Handler(handlerThread.getLooper());
        mCountDownLatch = new CountDownLatch(1);
        mBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mCountDownLatch.countDown();
                if (DEBUG) {
                    Log.d(TAG, "broadcast state count down: " + mCountDownLatch.getCount());
                }
            }
        };
        mContext.registerReceiver(mBroadcastReceiver,
                new IntentFilter(Intent.ACTION_WALLPAPER_CHANGED));
        mEnableWcg = mWallpaperManager.shouldEnableWideColorGamut();
    }

    @After
    public void tearDown() throws Exception {
        if (mBroadcastReceiver != null) {
            mContext.unregisterReceiver(mBroadcastReceiver);
        }
    }

    @Test
    public void setBitmapTest() {
        Bitmap tmpWallpaper = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(tmpWallpaper);
        canvas.drawColor(Color.RED);

        try {
            int which = WallpaperManager.FLAG_SYSTEM;
            mWallpaperManager.setBitmap(tmpWallpaper);
            int oldWallpaperId = mWallpaperManager.getWallpaperId(which);
            canvas.drawColor(Color.GREEN);
            mWallpaperManager.setBitmap(tmpWallpaper);
            int newWallpaperId = mWallpaperManager.getWallpaperId(which);
            Assert.assertNotEquals(oldWallpaperId, newWallpaperId);
        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            tmpWallpaper.recycle();
        }
    }

    @Test
    public void setResourceTest() {
        try {
            int which = WallpaperManager.FLAG_SYSTEM;
            int oldWallpaperId = mWallpaperManager.getWallpaperId(which);
            mWallpaperManager.setResource(R.drawable.robot);
            int newWallpaperId = mWallpaperManager.getWallpaperId(which);
            Assert.assertNotEquals(oldWallpaperId, newWallpaperId);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    public void wallpaperChangedBroadcastTest() {
        Bitmap tmpWallpaper = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(tmpWallpaper);
        canvas.drawColor(Color.BLACK);

        try {
            mWallpaperManager.setBitmap(tmpWallpaper);

            // Wait for up to 5 sec since this is an async call.
            // Should fail if Intent.ACTION_WALLPAPER_CHANGED isn't delivered.
            Assert.assertTrue(mCountDownLatch.await(5, TimeUnit.SECONDS));
        } catch (InterruptedException | IOException e) {
            throw new AssertionError("Intent.ACTION_WALLPAPER_CHANGED not received.");
        } finally {
            tmpWallpaper.recycle();
        }
    }

    @Test
    public void wallpaperClearBroadcastTest() {
        try {
            mWallpaperManager.clear(WallpaperManager.FLAG_LOCK | WallpaperManager.FLAG_SYSTEM);

            // Wait for 5 sec since this is an async call.
            // Should fail if Intent.ACTION_WALLPAPER_CHANGED isn't delivered.
            Assert.assertTrue(mCountDownLatch.await(5, TimeUnit.SECONDS));
        } catch (InterruptedException | IOException e) {
            throw new AssertionError(e);
        }
    }

    @Test
    public void invokeOnColorsChangedListenerTest_systemOnly() {
        int both = WallpaperManager.FLAG_LOCK | WallpaperManager.FLAG_SYSTEM;
        // Expect both since the first step is to migrate the current wallpaper
        // to the lock screen.
        verifyColorListenerInvoked(WallpaperManager.FLAG_SYSTEM, both);
    }

    @Test
    public void invokeOnColorsChangedListenerTest_lockOnly() {
        verifyColorListenerInvoked(WallpaperManager.FLAG_LOCK, WallpaperManager.FLAG_LOCK);
    }

    @Test
    public void invokeOnColorsChangedListenerTest_both() {
        int both = WallpaperManager.FLAG_LOCK | WallpaperManager.FLAG_SYSTEM;
        verifyColorListenerInvoked(both, both);
    }

    @Test
    public void invokeOnColorsChangedListenerTest_clearLock() throws IOException {
        verifyColorListenerInvokedClearing(WallpaperManager.FLAG_LOCK);
    }

    @Test
    public void invokeOnColorsChangedListenerTest_clearSystem() throws IOException {
        verifyColorListenerInvokedClearing(WallpaperManager.FLAG_SYSTEM);
    }

    /**
     * Removing a listener should not invoke it anymore
     */
    @Test
    public void addRemoveOnColorsChangedListenerTest_onlyInvokeAdded() throws IOException {
        ensureCleanState();

        final CountDownLatch latch = new CountDownLatch(1);
        WallpaperManager.OnColorsChangedListener counter = (colors, whichWp) -> latch.countDown();

        // Add and remove listener
        WallpaperManager.OnColorsChangedListener listener = getTestableListener();
        mWallpaperManager.addOnColorsChangedListener(listener, mHandler);
        mWallpaperManager.removeOnColorsChangedListener(listener);

        // Verify that the listener is not called
        mWallpaperManager.addOnColorsChangedListener(counter, mHandler);
        try {
            mWallpaperManager.setResource(R.drawable.robot);
            if (!latch.await(5, TimeUnit.SECONDS)) {
                throw new AssertionError("Registered listener not invoked");
            }
        } catch (InterruptedException | IOException e) {
            throw new RuntimeException(e);
        }
        verify(listener, never()).onColorsChanged(any(WallpaperColors.class), anyInt());
        mWallpaperManager.removeOnColorsChangedListener(counter);
    }

    /**
     * Suggesting desired dimensions is only a hint to the system that can be ignored.
     *
     * Test if the desired minimum width or height the WallpaperManager returns
     * is greater than 0. If so, then we check whether that the size is the dimension
     * that was suggested.
     */
    @Test
    public void suggestDesiredDimensionsTest() {
        final Point min = getScreenSize();
        int w = min.x * 3;
        int h = min.y * 2;

        // b/120847476: WallpaperManager limits at GL_MAX_TEXTURE_SIZE
        final int max = getMaxTextureSize();
        if (max > 0) {
            w = Math.min(w, max);
            h = Math.min(h, max);
        }

        assertDesiredDimension(new Point(min.x / 2, min.y / 2), new Point(min.x / 2, min.y / 2));

        assertDesiredDimension(new Point(w, h), new Point(w, h));

        assertDesiredDimension(new Point(min.x / 2, h), new Point(min.x / 2, h));

        assertDesiredDimension(new Point(w, min.y / 2), new Point(w, min.y / 2));
    }

    @Test
    public void wallpaperColors_primary() {
        Bitmap tmpWallpaper = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(tmpWallpaper);
        canvas.drawColor(Color.RED);

        try {
            mWallpaperManager.setBitmap(tmpWallpaper);
            WallpaperColors colors = mWallpaperManager.getWallpaperColors(
                    WallpaperManager.FLAG_SYSTEM);

            // Check that primary color is almost red
            Color primary = colors.getPrimaryColor();
            final float delta = 0.1f;
            Assert.assertEquals("red", 1f, primary.red(), delta);
            Assert.assertEquals("green", 0f, primary.green(), delta);
            Assert.assertEquals("blue", 0f, primary.blue(), delta);

            Assert.assertNull(colors.getSecondaryColor());
            Assert.assertNull(colors.getTertiaryColor());
        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            tmpWallpaper.recycle();
        }
    }


    @Test
    public void wallpaperColors_secondary() {
        Bitmap tmpWallpaper = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(tmpWallpaper);
        canvas.drawColor(Color.RED);
        // Make 20% of the wallpaper BLUE so that secondary color is BLUE
        canvas.clipRect(0, 0, 100, 20);
        canvas.drawColor(Color.BLUE);

        try {
            mWallpaperManager.setBitmap(tmpWallpaper);
            WallpaperColors colors = mWallpaperManager.getWallpaperColors(
                    WallpaperManager.FLAG_SYSTEM);

            // Check that the secondary color is almost blue
            Color secondary = colors.getSecondaryColor();
            final float delta = 0.1f;
            Assert.assertEquals("red", 0f, secondary.red(), delta);
            Assert.assertEquals("green", 0f, secondary.green(), delta);
            Assert.assertEquals("blue", 1f, secondary.blue(), delta);

            Assert.assertNull(colors.getTertiaryColor());
        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            tmpWallpaper.recycle();
        }
    }

    @Test
    public void highRatioWallpaper_largeWidth() throws Exception {
        Bitmap highRatioWallpaper = Bitmap.createBitmap(8000, 800, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(highRatioWallpaper);
        canvas.drawColor(Color.RED);

        try {
            mWallpaperManager.setBitmap(highRatioWallpaper);
            assertBitmapDimensions(mWallpaperManager.getBitmap());
        } finally {
            highRatioWallpaper.recycle();
        }
    }

    @Test
    public void highRatioWallpaper_largeHeight() throws Exception {
        Bitmap highRatioWallpaper = Bitmap.createBitmap(800, 8000, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(highRatioWallpaper);
        canvas.drawColor(Color.RED);

        try {
            mWallpaperManager.setBitmap(highRatioWallpaper);
            assertBitmapDimensions(mWallpaperManager.getBitmap());
        } finally {
            highRatioWallpaper.recycle();
        }
    }

    @Test
    public void highResolutionWallpaper() throws Exception {
        Bitmap highResolutionWallpaper = Bitmap.createBitmap(10000, 10000, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(highResolutionWallpaper);
        canvas.drawColor(Color.BLUE);

        try {
            mWallpaperManager.setBitmap(highResolutionWallpaper);
            assertBitmapDimensions(mWallpaperManager.getBitmap());
        } finally {
            highResolutionWallpaper.recycle();
        }
    }

    @Test
    public void testWideGamutWallpaper() throws IOException {
        final ColorSpace srgb = ColorSpace.get(ColorSpace.Named.SRGB);
        final ColorSpace p3 = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
        final Bitmap.Config config = Bitmap.Config.ARGB_8888;
        final Bitmap srgbBitmap = Bitmap.createBitmap(100, 100, config);
        final Bitmap p3Bitmap = Bitmap.createBitmap(100, 100, config, false, p3);

        try {
            // sRGB is the default color space
            mWallpaperManager.setBitmap(srgbBitmap);
            assertThat(mWallpaperManager.getBitmap().getColorSpace()).isEqualTo(srgb);

            // If wide gamut is enabled, Display-P3 should be supported.
            mWallpaperManager.setBitmap(p3Bitmap);

            final boolean isDisplayP3 = mWallpaperManager.getBitmap().getColorSpace().equals(p3);
            // Assert false only when device enabled WCG, but display does not support Display-P3
            assertThat(mEnableWcg && !isDisplayP3).isFalse();
        } finally {
            srgbBitmap.recycle();
            p3Bitmap.recycle();
        }
    }

    @Test
    public void testWallpaperSupportsWcg() throws IOException {
        final int sysWallpaper = WallpaperManager.FLAG_SYSTEM;

        final Bitmap srgbBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        final Bitmap p3Bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888, false,
                ColorSpace.get(ColorSpace.Named.DISPLAY_P3));

        try {
            mWallpaperManager.setBitmap(srgbBitmap);
            assertThat(mWallpaperManager.wallpaperSupportsWcg(sysWallpaper)).isFalse();

            mWallpaperManager.setBitmap(p3Bitmap);
            assertThat(mWallpaperManager.wallpaperSupportsWcg(sysWallpaper)).isEqualTo(mEnableWcg);
        } finally {
            srgbBitmap.recycle();
            p3Bitmap.recycle();
        }
    }

    private void assertBitmapDimensions(Bitmap bitmap) {
        int maxSize = getMaxTextureSize();
        boolean safe = false;
        if (bitmap != null) {
            safe = bitmap.getWidth() <= maxSize && bitmap.getHeight() <= maxSize;
        }
        assertThat(safe).isTrue();
    }

    private void assertDesiredDimension(Point suggestedSize, Point expectedSize) {
        mWallpaperManager.suggestDesiredDimensions(suggestedSize.x, suggestedSize.y);
        Point actualSize = new Point(mWallpaperManager.getDesiredMinimumWidth(),
                mWallpaperManager.getDesiredMinimumHeight());
        if (actualSize.x > 0 || actualSize.y > 0) {
            if ((actualSize.x != expectedSize.x || actualSize.y != expectedSize.y)) {
                throw new AssertionError("Expected x: " + expectedSize.x + " y: "
                        + expectedSize.y + ", got x: " + actualSize.x +
                        " y: " + actualSize.y);
            }
        }
    }

    private Point getScreenSize() {
        WindowManager wm = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        Display d = wm.getDefaultDisplay();
        Point p = new Point();
        d.getRealSize(p);
        return p;
    }

    /**
     * Helper to set a listener and verify if it was called with the same flags.
     * Executes operation synchronously.
     *
     * @param which FLAG_LOCK, FLAG_SYSTEM or a combination of both.
     */
    private void verifyColorListenerInvoked(int which, int whichExpected) {
        ensureCleanState();
        int expected = 0;
        if ((whichExpected & WallpaperManager.FLAG_LOCK) != 0) expected++;
        if ((whichExpected & WallpaperManager.FLAG_SYSTEM) != 0) expected++;
        ArrayList<Integer> received = new ArrayList<>();

        final CountDownLatch latch = new CountDownLatch(expected);
        Handler handler = new Handler(Looper.getMainLooper());

        WallpaperManager.OnColorsChangedListener listener = getTestableListener();
        WallpaperManager.OnColorsChangedListener counter = (colors, whichWp) -> {
            handler.post(()-> {
                received.add(whichWp);
                boolean ok = false;
                if ((whichWp & WallpaperManager.FLAG_LOCK) != 0 &&
                        (whichExpected & WallpaperManager.FLAG_LOCK) != 0) {
                    latch.countDown();
                    ok = true;
                }
                if ((whichWp & WallpaperManager.FLAG_SYSTEM) != 0 &&
                        (whichExpected & WallpaperManager.FLAG_SYSTEM) != 0) {
                    latch.countDown();
                    ok = true;
                }
                if (!ok) {
                    throw new AssertionError("Unexpected which flag: " + whichWp +
                            " should be: " + whichExpected);
                }
            });
        };

        mWallpaperManager.addOnColorsChangedListener(listener, mHandler);
        mWallpaperManager.addOnColorsChangedListener(counter, mHandler);

        try {
            mWallpaperManager.setResource(R.drawable.robot, which);
            if (!latch.await(5, TimeUnit.SECONDS)) {
                throw new AssertionError("Didn't receive all color events. Expected: " +
                        whichExpected + " received: " + received);
            }
        } catch (InterruptedException | IOException e) {
            throw new RuntimeException(e);
        }

        mWallpaperManager.removeOnColorsChangedListener(listener);
        mWallpaperManager.removeOnColorsChangedListener(counter);
    }

    /**
     * Helper to clear a wallpaper synchronously.
     *
     * @param which FLAG_LOCK, FLAG_SYSTEM or a combination of both.
     */
    private void verifyColorListenerInvokedClearing(int which) {
        ensureCleanState();

        final CountDownLatch latch = new CountDownLatch(1);

        WallpaperManager.OnColorsChangedListener listener = getTestableListener();
        WallpaperManager.OnColorsChangedListener counter = (colors, whichWp) -> {
            latch.countDown();
        };

        mWallpaperManager.addOnColorsChangedListener(listener, mHandler);
        mWallpaperManager.addOnColorsChangedListener(counter, mHandler);

        try {
            mWallpaperManager.clear(which);
            latch.await(5, TimeUnit.SECONDS);
        } catch (InterruptedException | IOException e) {
            throw new RuntimeException(e);
        }

        verify(listener, atLeast(1))
                .onColorsChanged(nullable(WallpaperColors.class), anyInt());

        mWallpaperManager.removeOnColorsChangedListener(listener);
        mWallpaperManager.removeOnColorsChangedListener(counter);
    }

    /**
     * Helper method to make sure a wallpaper is set for both FLAG_SYSTEM and FLAG_LOCK
     * and its callbacks were already called. Necessary to cleanup previous tests states.
     *
     * This is necessary to avoid race conditions between tests
     */
    private void ensureCleanState() {
        Bitmap bmp = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        // We expect 5 events to happen when we change a wallpaper:
        // • Wallpaper changed
        // • System colors are null
        // • Lock colors are null
        // • System colors are known
        // • Lock colors are known
        final int expectedEvents = 5;
        mCountDownLatch = new CountDownLatch(expectedEvents);
        if (DEBUG) {
            Log.d(TAG, "Started latch expecting: " + mCountDownLatch.getCount());
        }

        WallpaperManager.OnColorsChangedListener callback = (colors, which) -> {
            if ((which & WallpaperManager.FLAG_LOCK) != 0) {
                mCountDownLatch.countDown();
            }
            if ((which & WallpaperManager.FLAG_SYSTEM) != 0) {
                mCountDownLatch.countDown();
            }
            if (DEBUG) {
                Log.d(TAG, "color state count down: " + which + " - " + colors);
            }
        };
        mWallpaperManager.addOnColorsChangedListener(callback, mHandler);

        try {
            mWallpaperManager.setBitmap(bmp);

            // Wait for up to 10 sec since this is an async call.
            // Will pass as soon as the expected callbacks are executed.
            Assert.assertTrue(mCountDownLatch.await(10, TimeUnit.SECONDS));
            Assert.assertEquals(0, mCountDownLatch.getCount());
        } catch (InterruptedException | IOException e) {
            throw new RuntimeException("Can't ensure a clean state.");
        } finally {
            mWallpaperManager.removeOnColorsChangedListener(callback);
            bmp.recycle();
        }
    }

    public WallpaperManager.OnColorsChangedListener getTestableListener() {
        // Unfortunately mockito cannot mock anonymous classes or lambdas.
        return spy(new TestableColorListener());
    }

    public class TestableColorListener implements WallpaperManager.OnColorsChangedListener {
        @Override
        public void onColorsChanged(WallpaperColors colors, int which) {
        }
    }
}
