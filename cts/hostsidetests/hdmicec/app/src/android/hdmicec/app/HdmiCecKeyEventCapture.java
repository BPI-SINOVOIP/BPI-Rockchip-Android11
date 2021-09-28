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

package android.hdmicec.app;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TableRow;
import android.widget.TextView;

import java.lang.Override;

/**
 * A simple app that captures the key press events and logs them.
 */
public class HdmiCecKeyEventCapture extends Activity {

    private static final String TAG = HdmiCecKeyEventCapture.class.getSimpleName();
    private TextView text;
    private boolean longPressed = false;

    static final String LONG_PRESS_PREFIX = "Long press ";
    static final String SHORT_PRESS_PREFIX = "Short press ";

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        ScrollView sv = new ScrollView(this);
        LinearLayout layout = new LinearLayout(this);
        TableRow.LayoutParams layoutParams =
                new TableRow.LayoutParams(TableRow.LayoutParams.WRAP_CONTENT,
                        TableRow.LayoutParams.WRAP_CONTENT, 1.0f);
        text = new TextView(this);
        text.setGravity(Gravity.CENTER);
        text.setText("No key pressed!");
        text.setLayoutParams(layoutParams);
        layout.addView(text);
        this.setContentView(layout);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        event.startTracking();
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {

        text.setText((longPressed ? "Long press " : "Short press ") +
                event.keyCodeToString(keyCode));

        Log.d(TAG, event.toString());
        Log.i(TAG, (longPressed ? "Long press " : "Short press ") + event.keyCodeToString(keyCode));

        longPressed = false;
        return true;
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        longPressed = true;
        return true;
    }

}