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
 *
 */

package com.android.cts.forcestophelper;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

/**
 * Test activity to show up as a task in recents.
 */
public class RecentTaskActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        final Bundle extras = getIntent().getExtras();

        final Intent serviceIntent = new Intent();
        serviceIntent.setClass(this, TaskRemovedListenerService.class);
        if (extras != null) {
            serviceIntent.putExtras(extras);
        }
        startService(serviceIntent);

        final TextView status = findViewById(R.id.status_message);
        status.setVisibility(View.VISIBLE);
    }
}
