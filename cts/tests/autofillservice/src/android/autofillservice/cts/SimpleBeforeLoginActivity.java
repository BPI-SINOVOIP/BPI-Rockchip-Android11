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

package android.autofillservice.cts;

import android.os.Bundle;
import android.util.Log;

/**
 * Activity that displays a "Launch login activity!" message before login.
 */
public class SimpleBeforeLoginActivity extends AbstractAutoFillActivity {

    private static final String TAG = "SimpleBeforeLoginActivity";

    static final String ID_BEFORE_LOGIN = "before_login";

    private static SimpleBeforeLoginActivity sCurrentActivity;

    public static SimpleBeforeLoginActivity getCurrentActivity() {
        return sCurrentActivity;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.simple_before_login_activity);

        Log.v(TAG, "Set sCurrentActivity to this onCreate()");
        sCurrentActivity = this;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        Log.v(TAG, "Set sCurrentActivity to null onDestroy()");
        sCurrentActivity = null;
    }
}
