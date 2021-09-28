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

package android.telephony.cts.smsretriever;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.telephony.SmsManager;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class SmsRetrieverBroadcastReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
          if (intent.getAction().equals("android.telephony.cts.action.SMS_RETRIEVED")) {
              context.startActivity(new Intent("any.action")
                      .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                      .setComponent(new ComponentName("android.telephony.cts.smsretriever",
                              "android.telephony.cts.smsretriever.MainActivity"))
                      .putExtra("message", intent.getStringExtra(SmsManager.EXTRA_SMS_MESSAGE)));
          }
        }
}
