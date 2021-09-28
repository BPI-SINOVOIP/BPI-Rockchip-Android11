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

package android.telecom.cts.screeningtestapp;

import static android.telecom.TelecomManager.EXTRA_DISCONNECT_CAUSE;
import static android.telecom.TelecomManager.EXTRA_HANDLE;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class CtsPostCallActivity extends Activity {
    private static final String ACTION_POST_CALL = "android.telecom.action.POST_CALL";
    private static final int DEFAULT_DISCONNECT_CAUSE = -1;
    private static final long TEST_TIMEOUT = 5000;

    private static Uri cachedHandle;
    private static int cachedDisconnectCause;
    private static CountDownLatch sLatch = new CountDownLatch(1);

    @Override
    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        final Intent intent = getIntent();
        final String action = intent != null ? intent.getAction() : null;
        if (ACTION_POST_CALL.equals(action)) {
            cachedHandle = intent.getParcelableExtra(EXTRA_HANDLE);
            cachedDisconnectCause = intent
                    .getIntExtra(EXTRA_DISCONNECT_CAUSE, DEFAULT_DISCONNECT_CAUSE);
            sLatch.countDown();
        }
    }

    public static Uri getCachedHandle() {
        return cachedHandle;
    }

    public static int getCachedDisconnectCause() {
        return cachedDisconnectCause;
    }

    public static void resetPostCallActivity() {
        sLatch = new CountDownLatch(1);
        cachedHandle = null;
        cachedDisconnectCause = DEFAULT_DISCONNECT_CAUSE;
    }

    public static boolean waitForActivity() {
        try {
            return sLatch.await(TEST_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return false;
        }
    }
}
