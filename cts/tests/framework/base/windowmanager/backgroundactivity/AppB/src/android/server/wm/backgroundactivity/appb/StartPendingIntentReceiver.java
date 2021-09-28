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
 * limitations under the License
 */

package android.server.wm.backgroundactivity.appb;

import static android.server.wm.backgroundactivity.appb.Components.StartPendingIntentReceiver.PENDING_INTENT_EXTRA;
import static android.server.wm.backgroundactivity.common.CommonComponents.EVENT_NOTIFIER_EXTRA;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.ResultReceiver;
import android.server.wm.backgroundactivity.common.CommonComponents.Event;

/**
 * Receive pending intent from AppA and launch it
 */
public class StartPendingIntentReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        PendingIntent pendingIntent = intent.getParcelableExtra(PENDING_INTENT_EXTRA);
        ResultReceiver eventNotifier = intent.getParcelableExtra(EVENT_NOTIFIER_EXTRA);
        if (eventNotifier != null) {
            eventNotifier.send(Event.APP_B_START_PENDING_INTENT_BROADCAST_RECEIVED, null);
        }

        try {
            pendingIntent.send();
        } catch (PendingIntent.CanceledException e) {
            e.printStackTrace();
        }
    }
}
