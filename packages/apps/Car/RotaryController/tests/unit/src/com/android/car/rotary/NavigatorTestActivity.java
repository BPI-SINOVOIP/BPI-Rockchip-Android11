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

package com.android.car.rotary;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;

/** An activity used for testing {@link com.android.car.rotary.Navigator}. */
public class NavigatorTestActivity extends Activity {

    /** Key used to indicate which resource layout to inflate. */
    static final String KEY_LAYOUT_ID = "com.android.car.rotary.KEY_LAYOUT_ID";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        if (intent != null) {
            @LayoutRes int layoutId = intent.getIntExtra(KEY_LAYOUT_ID, /* defaultValue= */ -1);
            if (layoutId != -1) {
                setContentView(layoutId);
            }
        }
    }
}
