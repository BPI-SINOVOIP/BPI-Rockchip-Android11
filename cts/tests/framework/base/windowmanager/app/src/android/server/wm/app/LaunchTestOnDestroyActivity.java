/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.app.Components.TEST_ACTIVITY;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class LaunchTestOnDestroyActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Intent intent = new Intent();
        intent.setComponent(TEST_ACTIVITY);
        intent.addFlags(FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
    }
}
