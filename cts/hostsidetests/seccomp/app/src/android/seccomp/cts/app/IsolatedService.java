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
package android.seccomp.cts.app;

import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

public class IsolatedService extends Service {
    static final String TAG = "IsolatedService";

    static final int MSG_GET_SECCOMP_RESULT = 1;

    static final int MSG_SECCOMP_RESULT = 2;
    final Messenger mMessenger = new Messenger(new ServiceHandler());

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }

    static class ServiceHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case (MSG_GET_SECCOMP_RESULT):
                    int result = ZygotePreload.getSeccomptestResult() ? 1 : 0;
                    try {
                        msg.replyTo.send(Message.obtain(null, MSG_SECCOMP_RESULT, result, 0));
                    } catch (RemoteException e) {
                        Log.e(TAG, "Failed to send seccomp test result", e);
                    }
                    break;
                default:
                    super.handleMessage(msg);
            }
        }
    }
}
