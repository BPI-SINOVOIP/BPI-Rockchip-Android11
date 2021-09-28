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

package com.android.car.settings.common.rotary;

import android.view.KeyEvent;
import android.view.View;
import android.widget.NumberPicker;

import java.util.HashMap;
import java.util.Map;

/**
 * A nudge handler to deal with {@link NumberPicker} instances. Ensures that nudging keeps focus
 * within the parent container of the {@link NumberPicker}.
 */
public class NumberPickerNudgeHandler implements View.OnKeyListener {

    private static final Map<Integer, Integer> KEYCODE_TO_DIRECTION_MAP =
            new HashMap<Integer, Integer>() {{
                put(KeyEvent.KEYCODE_DPAD_UP, View.FOCUS_UP);
                put(KeyEvent.KEYCODE_DPAD_DOWN, View.FOCUS_DOWN);
                put(KeyEvent.KEYCODE_DPAD_LEFT, View.FOCUS_LEFT);
                put(KeyEvent.KEYCODE_DPAD_RIGHT, View.FOCUS_RIGHT);
            }};

    @Override
    public boolean onKey(View view, int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
                // Disable by consuming the event and not doing anything.
                return true;
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                if (event.getAction() == KeyEvent.ACTION_UP) {
                    int direction = KEYCODE_TO_DIRECTION_MAP.get(keyCode);
                    View nextView = view.focusSearch(direction);
                    if (NumberPickerUtils.hasCommonNumberPickerParent(view, nextView)) {
                        nextView.requestFocus(direction);
                    }
                }
                return true;
            default:
                return false;
        }
    }
}
