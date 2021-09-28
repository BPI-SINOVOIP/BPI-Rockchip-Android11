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

package android.app.usage.cts.test1;

import android.app.Service;
import android.app.usage.UsageStatsManager;
import android.app.usage.cts.ITestReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

public class TestService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return new TestReceiver();
    }

    private class TestReceiver extends ITestReceiver.Stub {
        @Override
        public boolean isAppInactive(String pkg) {
            UsageStatsManager usm = (UsageStatsManager) getSystemService(
                    Context.USAGE_STATS_SERVICE);
            return usm.isAppInactive(pkg);
        }
    }
}
