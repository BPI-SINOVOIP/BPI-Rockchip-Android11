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

package android.incrementalinstall.inrementaltestappvalidation;

import static android.incrementalinstall.common.Consts.COMPONENT_STATUS_KEY;
import static android.incrementalinstall.common.Consts.TARGET_COMPONENT_KEY;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.incrementalinstall.common.Consts;

import java.util.HashMap;
import java.util.Set;
import java.util.concurrent.CountDownLatch;

final class StatusReceiver extends BroadcastReceiver {

    private HashMap<String, StatusLatch> mStatusLatchMap;

    public StatusReceiver() {
        mStatusLatchMap = new HashMap<>();
        for (String component : Consts.SupportedComponents.getAllComponents()) {
            mStatusLatchMap.put(component, new StatusLatch());
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String targetComponent = intent.getStringExtra(TARGET_COMPONENT_KEY);
        boolean status = intent.getBooleanExtra(COMPONENT_STATUS_KEY, false);
        mStatusLatchMap.get(targetComponent).releaseWithStatus(status);
    }

    public boolean verifyComponentInvoked(String component) throws InterruptedException {
        return mStatusLatchMap.get(component).awaitForStatus();
    }

    public Set<String> supportedComponentsList() {
        return mStatusLatchMap.keySet();
    }

    private static class StatusLatch {

        private final CountDownLatch mCountDownLatch = new CountDownLatch(1);
        private boolean mStatus = false;

        void releaseWithStatus(boolean status) {
            mStatus = status;
            mCountDownLatch.countDown();
        }

        boolean awaitForStatus() throws InterruptedException {
            mCountDownLatch.await();
            return mStatus;
        }
    }
}

