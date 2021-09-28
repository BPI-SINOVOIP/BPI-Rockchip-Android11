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
package android.appwidget.cts;

import static org.junit.Assert.assertTrue;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.test.InstrumentationRegistry;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class BlockingBroadcastReceiver extends BroadcastReceiver {

    public static final String KEY_PARAM = "BlockingBroadcastReceiver.param";

    private final CountDownLatch latch = new CountDownLatch(1);

    public Intent result;
    private String mParams;

    @Override
    public void onReceive(Context context, Intent intent) {
        result = intent;
        mParams = intent.getStringExtra(KEY_PARAM);
        InstrumentationRegistry.getTargetContext().unregisterReceiver(this);
        latch.countDown();
    }

    public String getParam(long timeout, TimeUnit unit) throws InterruptedException {
        latch.await(timeout, unit);
        return mParams;
    }

    public void await() throws Exception {
        assertTrue(latch.await(20, TimeUnit.SECONDS));
    }

    public BlockingBroadcastReceiver register(String action) {
        InstrumentationRegistry.getTargetContext().registerReceiver(
                this, new IntentFilter(action));
        return this;
    }
}