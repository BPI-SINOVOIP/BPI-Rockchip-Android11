/**
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.accessibilityservice.cts.utils;

import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY;
import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC;
import static android.view.Display.DEFAULT_DISPLAY;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.ImageReader;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Window;

import com.android.compatibility.common.util.TestUtils;

/**
 * Utilities needed when interacting with the display
 */
public class DisplayUtils {
    private static final int DISPLAY_ADDED_TIMEOUT_MS = 5000;

    public static int getStatusBarHeight(Activity activity) {
        final Rect rect = new Rect();
        Window window = activity.getWindow();
        window.getDecorView().getWindowVisibleDisplayFrame(rect);
        return rect.top;
    }

    public static class VirtualDisplaySession implements AutoCloseable {
        private VirtualDisplay mVirtualDisplay;
        private ImageReader mReader;

        public Display createDisplay(Context context, int width, int height, int density,
                boolean isPrivate) {
            if (mReader != null) {
                throw new IllegalStateException(
                        "Only one display can be created during this session.");
            }
            mReader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888,
                    1 /* maxImages */);
            int flags = isPrivate ? 0
                    :(VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY | VIRTUAL_DISPLAY_FLAG_PUBLIC);
            mVirtualDisplay = context.getSystemService(DisplayManager.class).createVirtualDisplay(
                    "A11yDisplay", width, height, density, mReader.getSurface(), flags);
            return mVirtualDisplay.getDisplay();
        }

        @Override
        public void close() {
            if (mVirtualDisplay != null) {
                mVirtualDisplay.release();
            }
            if (mReader != null) {
                mReader.close();
            }
        }

        /**
         * Creates a virtual display having same size with default display and waits until it's
         * in display list. The density of the virtual display is based on
         * {@link DisplayMetrics#xdpi} so that the threshold of gesture detection is same as
         * the default display's.
         *
         * @param context
         * @param isPrivate if this display is a private display.
         * @return virtual display.
         *
         * @throws IllegalStateException if called from main thread.
         */
        public Display createDisplayWithDefaultDisplayMetricsAndWait(Context context,
                boolean isPrivate) {
            if (Looper.myLooper() == Looper.getMainLooper()) {
                throw new IllegalStateException("Should not call from main thread");
            }

            final Object waitObject = new Object();
            final DisplayManager.DisplayListener listener = new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int i) {
                    synchronized (waitObject) {
                        waitObject.notifyAll();
                    }
                }

                @Override
                public void onDisplayRemoved(int i) {
                }

                @Override
                public void onDisplayChanged(int i) {
                }
            };
            final DisplayManager displayManager = (DisplayManager) context.getSystemService(
                    Context.DISPLAY_SERVICE);
            displayManager.registerDisplayListener(listener, null);

            final DisplayMetrics metrics = new DisplayMetrics();
            displayManager.getDisplay(DEFAULT_DISPLAY).getRealMetrics(metrics);
            final Display display = createDisplay(context, metrics.widthPixels,
                    metrics.heightPixels, (int) metrics.xdpi, isPrivate);

            try {
                TestUtils.waitOn(waitObject,
                        () -> displayManager.getDisplay(display.getDisplayId()) != null,
                        DISPLAY_ADDED_TIMEOUT_MS,
                        String.format("wait for virtual display %d adding", display.getDisplayId()));
            } finally {
                displayManager.unregisterDisplayListener(listener);
            }
            return display;
        }
    }
}
