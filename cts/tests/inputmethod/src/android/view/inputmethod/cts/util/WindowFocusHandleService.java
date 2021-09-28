/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.view.inputmethod.cts.util;

import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
import static android.view.WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.hardware.display.DisplayManager;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.widget.EditText;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A Service class to create popup window with edit text by handler thread.
 * For verifying IME focus handle between windows on different UI thread.
 */
public class WindowFocusHandleService extends Service {
    private @Nullable static WindowFocusHandleService sInstance = null;
    private static final String TAG = WindowFocusHandleService.class.getSimpleName();

    private EditText mPopupTextView;
    private Handler mThreadHandler;

    @Override
    public void onCreate() {
        super.onCreate();
        // Create a popup text view with different UI thread.
        final HandlerThread localThread = new HandlerThread("TestThreadHandler");
        localThread.start();
        mThreadHandler = new Handler(localThread.getLooper());
        mThreadHandler.post(() -> mPopupTextView = createPopupTextView(new Point(150, 150)));
    }

    public @Nullable static WindowFocusHandleService getInstance() {
        return sInstance;
    }

    @Override
    public IBinder onBind(Intent intent) {
        sInstance = this;
        return new Binder();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        sInstance = null;
        return true;
    }

    @UiThread
    private EditText createPopupTextView(Point pos) {
        final Context windowContext = createWindowContext(DEFAULT_DISPLAY);
        final WindowManager wm = windowContext.getSystemService(WindowManager.class);
        final EditText editText = new EditText(windowContext) {
            @Override
            public void onWindowFocusChanged(boolean hasWindowFocus) {
                if (Log.isLoggable(TAG, Log.VERBOSE)) {
                    Log.v(TAG, "onWindowFocusChanged for view=" + this
                            + ", hasWindowfocus: " + hasWindowFocus);
                }
            }
        };
        editText.setOnFocusChangeListener((v, hasFocus) -> {
            if (v == editText) {
                WindowManager.LayoutParams params =
                        (WindowManager.LayoutParams) editText.getLayoutParams();
                if (hasFocus) {
                    params.flags &= ~FLAG_NOT_FOCUSABLE;
                    params.flags |= FLAG_LAYOUT_IN_SCREEN
                                    | FLAG_WATCH_OUTSIDE_TOUCH
                                    | FLAG_NOT_TOUCH_MODAL;
                    wm.updateViewLayout(editText, params);
                }
            }
        });
        editText.setOnTouchListener((v, event) -> {
            if (event.getAction() == MotionEvent.ACTION_OUTSIDE) {
                WindowManager.LayoutParams params =
                        (WindowManager.LayoutParams) editText.getLayoutParams();
                if ((params.flags & FLAG_NOT_FOCUSABLE) == 0) {
                    params.flags |= FLAG_NOT_FOCUSABLE;
                    wm.updateViewLayout(editText, params);
                }
            }
            return false;
        });
        editText.setBackground(new ColorDrawable(Color.CYAN));

        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                150, 150, pos.x, pos.y,
                TYPE_APPLICATION_OVERLAY, FLAG_NOT_FOCUSABLE,
                PixelFormat.OPAQUE);
        // Currently SOFT_INPUT_STATE_UNSPECIFIED isn't appropriate for CTS test because there is no
        // clear spec about how it behaves.  In order to make our tests deterministic, currently we
        // must use SOFT_INPUT_STATE_HIDDEN to make sure soft-keyboard will hide after navigating
        // forward to next window.
        // TODO(Bug 77152727): Remove the following code once we define how
        params.softInputMode = SOFT_INPUT_STATE_HIDDEN;
        wm.addView(editText, params);
        return editText;
    }

    private Context createWindowContext(int displayId) {
        final Display display = getSystemService(DisplayManager.class).getDisplay(displayId);
        return createDisplayContext(display).createWindowContext(TYPE_APPLICATION_OVERLAY,
                null /* options */);
    }

    @AnyThread
    public EditText getPopupTextView(@Nullable AtomicBoolean outPopupTextHasWindowFocusRef) {
        if (outPopupTextHasWindowFocusRef != null) {
            mPopupTextView.post(() -> {
                final ViewTreeObserver observerForPopupTextView =
                        mPopupTextView.getViewTreeObserver();
                observerForPopupTextView.addOnWindowFocusChangeListener((hasFocus) ->
                        outPopupTextHasWindowFocusRef.set(hasFocus));
            });
        }
        return mPopupTextView;
    }

    @MainThread
    public void handleReset() {
        if (mPopupTextView != null) {
            final WindowManager wm = mPopupTextView.getContext().getSystemService(
                    WindowManager.class);
            wm.removeView(mPopupTextView);
            mPopupTextView = null;
        }
    }

    @MainThread
    public void onDestroy() {
        super.onDestroy();
        handleReset();
    }
}
