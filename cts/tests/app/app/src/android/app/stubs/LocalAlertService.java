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
 * limitations under the License
 */
package android.app.stubs;

import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Point;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

import static android.view.Gravity.LEFT;
import static android.view.Gravity.TOP;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
import static android.view.WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH;

public class LocalAlertService extends Service {
    public static final String COMMAND_SHOW_ALERT = "show";
    public static final String COMMAND_HIDE_ALERT = "hide";

    private static View mAlertWindow = null;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent.getAction();

        if (COMMAND_SHOW_ALERT.equals(action)) {
            mAlertWindow = showAlertWindow(getPackageName());
        } else if (COMMAND_HIDE_ALERT.equals(action)) {
            hideAlertWindow(mAlertWindow);
            mAlertWindow = null;
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private View showAlertWindow(String windowName) {
        final Point size = new Point();
        final WindowManager wm = getSystemService(WindowManager.class);
        wm.getDefaultDisplay().getSize(size);

        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                TYPE_APPLICATION_OVERLAY, FLAG_NOT_FOCUSABLE | FLAG_WATCH_OUTSIDE_TOUCH |
                FLAG_NOT_TOUCHABLE);
        params.width = size.x / 3;
        params.height = size.y / 3;
        params.gravity = TOP | LEFT;
        params.setTitle(windowName);

        final TextView view = new TextView(this);
        view.setText(windowName);
        view.setBackgroundColor(Color.RED);
        wm.addView(view, params);
        return view;
    }

    private void hideAlertWindow(View window) {
        final WindowManager wm = getSystemService(WindowManager.class);
        wm.removeViewImmediate(window);
    }
}
