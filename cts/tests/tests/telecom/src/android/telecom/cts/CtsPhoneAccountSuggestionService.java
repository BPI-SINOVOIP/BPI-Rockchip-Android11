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
package android.telecom.cts;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.telecom.PhoneAccountSuggestion;
import android.telecom.PhoneAccountSuggestionService;

import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class CtsPhoneAccountSuggestionService extends PhoneAccountSuggestionService {
    static long sSuggestionWaitTime = 0;
    static List<PhoneAccountSuggestion> sSuggestionsToProvide;
    final static BlockingQueue<String> sSuggestionRequestQueue = new LinkedBlockingQueue<>(1);

    @Override
    public void onAccountSuggestionRequest(String number) {
        sSuggestionRequestQueue.offer(number);
        new Handler(Looper.getMainLooper()).postDelayed(
                () -> suggestPhoneAccounts(number, sSuggestionsToProvide),
                sSuggestionWaitTime);
    }

    public static void enableService(Context context) {
        context.getPackageManager().setComponentEnabledSetting(getComponentName(context),
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP);
    }

    public static void disableService(Context context) {
        context.getPackageManager().setComponentEnabledSetting(getComponentName(context),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP);
    }

    private static ComponentName getComponentName(Context context) {
        return new ComponentName(context, CtsPhoneAccountSuggestionService.class);
    }
}
