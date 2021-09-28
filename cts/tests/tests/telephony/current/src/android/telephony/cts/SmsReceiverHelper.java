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

package android.telephony.cts;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

public class SmsReceiverHelper {

    public static final String MESSAGE_SENT_ACTION =
            "android.telephony.cts.SmsReceiverHelper.MESSAGE_SENT_ACTION";

    public static final String MESSAGE_DELIVERED_ACTION =
            "android.telephony.cts.SmsReceiverHelper.MESSAGE_DELIVERED_ACTION";

    public static final String EXTRA_RESULT_CODE = "resultCode";

    public static PendingIntent getMessageSentPendingIntent(Context context) {
        Intent intent = new Intent(context, SmsReceiver.class);
        intent.setAction(MESSAGE_SENT_ACTION);
        return PendingIntent.getBroadcast(context, 0, intent,
                PendingIntent.FLAG_CANCEL_CURRENT);
    }

    public static PendingIntent getMessageDeliveredPendingIntent(Context context) {
        Intent intent = new Intent(context, SmsReceiver.class);
        intent.setAction(MESSAGE_DELIVERED_ACTION);
        return PendingIntent.getBroadcast(context, 0, intent,
                PendingIntent.FLAG_CANCEL_CURRENT);
    }
}
