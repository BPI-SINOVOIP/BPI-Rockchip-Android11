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

package android.permission.cts;

import android.service.notification.NotificationListenerService;
import android.util.Log;

public class NotificationListener extends NotificationListenerService {
    private static final String LOG_TAG = NotificationListener.class.getSimpleName();

    private static final Object sLock = new Object();

    private static NotificationListener sService;

    @Override
    public void onListenerConnected() {
        Log.i(LOG_TAG, "connected");
        synchronized (sLock) {
            sService = this;
            sLock.notifyAll();
        }
    }

    public static NotificationListenerService getInstance() throws Exception {
        synchronized (sLock) {
            if (sService == null) {
                sLock.wait(5000);
            }

            return sService;
        }
    }

    @Override
    public void onListenerDisconnected() {
        Log.i(LOG_TAG, "disconnected");

        synchronized (sLock) {
            sService = null;
        }
    }
}
