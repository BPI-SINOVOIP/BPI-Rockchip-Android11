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
package android.app.notification.legacy.cts;

import android.net.Uri;
import android.service.notification.Condition;
import android.service.notification.ConditionProviderService;
import android.util.Log;

public abstract class PollableConditionProviderService extends ConditionProviderService {
    final static String TAG = "CtsCps";

    boolean isConnected;
    boolean subscribed;
    Uri conditionId;

    @Override
    public void onConnected() {
        Log.d(TAG, getClass().getName() + " Connected");
        isConnected = true;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, getClass().getName() + " Destroyed");
        isConnected = false;
        super.onDestroy();
    }

    @Override
    public void onSubscribe(Uri conditionId) {
        Log.d(TAG, getClass().getName() + " got subscribe");
        subscribed = true;
        this.conditionId = conditionId;
        notifyCondition(new Condition(conditionId, "", Condition.STATE_TRUE));
    }

    @Override
    public void onUnsubscribe(Uri conditionId) {
        Log.d(TAG, getClass().getName() + " got unsubscribe");
        subscribed = false;
        this.conditionId = null;
    }
}
