/*
 * Copyright (C) 2020 The Android Open Source Project.
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
package com.android.car.ui.paintbooth.toolbar;

import android.os.Bundle;

import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.paintbooth.R;
import com.android.car.ui.toolbar.Toolbar;

public class OldToolbarActivity extends ToolbarActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Toolbar toolbar = requireViewById(R.id.toolbar);
        RecyclerView rv = requireViewById(R.id.list);
        toolbar.registerToolbarHeightChangeListener(height -> {
            rv.setPadding(0, height, 0, 0);
        });
    }

    @Override
    protected int getLayout() {
        return R.layout.car_ui_recycler_view_activity_with_old_toolbar;
    }
}
