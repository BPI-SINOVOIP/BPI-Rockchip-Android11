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

package android.server.wm.app;

import static android.server.wm.app.Components.ClickableToastActivity.ACTION_TOAST_DISPLAYED;
import static android.server.wm.app.Components.ClickableToastActivity.ACTION_TOAST_TAP_DETECTED;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager.LayoutParams;
import android.widget.Toast;

import java.lang.ref.WeakReference;
import java.lang.reflect.Field;

public class ClickableToastActivity extends Activity {
    private static final int DETECT_TOAST_TIMEOUT_MS = 15000;
    private static final int DETECT_TOAST_POOLING_INTERVAL_MS = 200;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Handler handler = new Handler();
        Toast toast = getToast(this);
        long deadline = SystemClock.uptimeMillis() + DETECT_TOAST_TIMEOUT_MS;
        handler.post(new DetectToastRunnable(getApplicationContext(), toast.getView(), deadline,
                handler));
        toast.show();
    }

    private Toast getToast(Context context) {
        Context applicationContext = context.getApplicationContext();
        View view = LayoutInflater.from(context).inflate(R.layout.toast, null);
        view.setOnTouchListener((v, event) -> {
            applicationContext.sendBroadcast(new Intent(ACTION_TOAST_TAP_DETECTED));
            return false;
        });
        Toast toast = getClickableToast(context);
        toast.setView(view);
        toast.setGravity(Gravity.FILL_HORIZONTAL | Gravity.FILL_VERTICAL, 0, 0);
        toast.setDuration(Toast.LENGTH_LONG);
        return toast;
    }

    /**
     * Purposely creating a toast without FLAG_NOT_TOUCHABLE in the client-side (via reflection) to
     * test enforcement on the server-side.
     */
    private Toast getClickableToast(Context context) {
        try {
            Toast toast = new Toast(context);
            Field tnField = Toast.class.getDeclaredField("mTN");
            tnField.setAccessible(true);
            Object tnObject = tnField.get(toast);
            Field paramsField = Class.forName(
                    Toast.class.getCanonicalName() + "$TN").getDeclaredField("mParams");
            paramsField.setAccessible(true);
            LayoutParams params = (LayoutParams) paramsField.get(tnObject);
            params.flags = LayoutParams.FLAG_KEEP_SCREEN_ON | LayoutParams.FLAG_NOT_FOCUSABLE;
            return toast;
        } catch (NoSuchFieldException | IllegalAccessException | ClassNotFoundException e) {
            throw new IllegalStateException("Toast reflection failed", e);
        }
    }

    private static class DetectToastRunnable implements Runnable {
        private final Context mContext;
        private final WeakReference<View> mToastViewRef;
        private final long mDeadline;
        private final Handler mHandler;

        private DetectToastRunnable(
                Context applicationContext, View toastView, long deadline, Handler handler) {
            mContext = applicationContext;
            mToastViewRef = new WeakReference<>(toastView);
            mDeadline = deadline;
            mHandler = handler;
        }

        @Override
        public void run() {
            View toastView = mToastViewRef.get();
            if (SystemClock.uptimeMillis() > mDeadline || toastView == null) {
                return;
            }
            if (toastView.getParent() != null) {
                mContext.sendBroadcast(new Intent(ACTION_TOAST_DISPLAYED));
                return;
            }
            mHandler.postDelayed(this, DETECT_TOAST_POOLING_INTERVAL_MS);
        }
    }
}
