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

package android.assist.common;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteCallback;

public class BaseRemoteCallbackActivity extends Activity {

    private RemoteCallback mRemoteCallback;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        registerReceivingCallback();
    }

    protected void notify(String action) {
        RemoteCallback callback = getIntent().getParcelableExtra(Utils.EXTRA_REMOTE_CALLBACK);

        if (callback == null) {
            throw new IllegalStateException("Test ran expecting " + action
                    + " but did not register callback");
        } else {
            Bundle bundle = new Bundle();
            bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, action);
            callback.sendResult(bundle);
        }
    }

    protected void registerReceivingCallback() {
        RemoteCallback remoteCallback = getIntent().getParcelableExtra(Utils.EXTRA_REMOTE_CALLBACK_RECEIVING);
        if (remoteCallback == null) {
            return;

        }

        mRemoteCallback = new RemoteCallback((results) -> {
            String action = results.getString(Utils.EXTRA_REMOTE_CALLBACK_ACTION);
            if (action.equals(Utils.ACTION_END_OF_TEST)) {
                if (!isFinishing()) {
                    finish();
                }
            } else {
                onReceivedEventFromCaller(results, action);
            }
        }, new Handler(Looper.getMainLooper()));

        Bundle bundle = new Bundle();
        bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, Utils.EXTRA_REMOTE_CALLBACK_RECEIVING_ACTION);
        bundle.putParcelable(Utils.EXTRA_REMOTE_CALLBACK_RECEIVING, mRemoteCallback);
        remoteCallback.sendResult(bundle);
    }

    /**
     * React to events from the calling test. Called from the main thread.
     */
    protected void onReceivedEventFromCaller(Bundle results, String action) {
    }
}
