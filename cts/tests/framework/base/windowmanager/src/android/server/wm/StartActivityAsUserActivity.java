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

package android.server.wm;


import static android.server.wm.StartActivityAsUserTests.EXTRA_CALLBACK;
import static android.server.wm.StartActivityAsUserTests.KEY_USER_ID;

import android.app.Activity;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.os.UserHandle;
import android.util.Log;

public class StartActivityAsUserActivity extends Activity {
    private static final String LOG_TAG = "startActivityAsUserTest";

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        Bundle extra = getIntent().getExtras();
        RemoteCallback cb = (RemoteCallback) extra.get(EXTRA_CALLBACK);
        if (cb != null) {
            Bundle result = new Bundle();
            result.putInt(KEY_USER_ID, UserHandle.myUserId());
            cb.sendResult(result);
        }
        Log.i(LOG_TAG, "Second activity started with user " + UserHandle.myUserId());
        finish();
    }
}


