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

package com.android;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.PersistableBundle;
import android.telecom.TelecomManager;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.test.mock.MockContext;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.concurrent.Executor;

public class TestContext extends MockContext {

    @Mock CarrierConfigManager mMockCarrierConfigManager;
    @Mock TelecomManager mMockTelecomManager;
    @Mock TelephonyManager mMockTelephonyManager;
    @Mock SubscriptionManager mMockSubscriptionManager;

    private PersistableBundle mCarrierConfig = new PersistableBundle();

    public TestContext() {
        MockitoAnnotations.initMocks(this);
        doReturn(mCarrierConfig).when(mMockCarrierConfigManager).getConfigForSubId(anyInt());
    }

    @Override
    public Executor getMainExecutor() {
        // Just run on current thread
        return Runnable::run;
    }

    @Override
    public Context getApplicationContext() {
        return this;
    }

    @Override
    public String getPackageName() {
        return "com.android.phone.tests";
    }

    @Override
    public String getAttributionTag() {
        return "";
    }

    @Override
    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
        return null;
    }

    @Override
    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter, int flags) {
        return null;
    }

    @Override
    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter,
            String broadcastPermission, Handler scheduler) {
        return null;
    }

    @Override
    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter,
            String broadcastPermission, Handler scheduler, int flags) {
        return null;
    }

    @Override
    public Object getSystemService(String name) {
        switch (name) {
            case (Context.CARRIER_CONFIG_SERVICE) : {
                return mMockCarrierConfigManager;
            }
            case (Context.TELECOM_SERVICE) : {
                return mMockTelecomManager;
            }
            case (Context.TELEPHONY_SERVICE) : {
                return mMockTelephonyManager;
            }
            case (Context.TELEPHONY_SUBSCRIPTION_SERVICE) : {
                return mMockSubscriptionManager;
            }
        }
        return null;
    }

    @Override
    public String getSystemServiceName(Class<?> serviceClass) {
        if (serviceClass == CarrierConfigManager.class) {
            return Context.CARRIER_CONFIG_SERVICE;
        }
        if (serviceClass == TelecomManager.class) {
            return Context.TELECOM_SERVICE;
        }
        if (serviceClass == TelephonyManager.class) {
            return Context.TELEPHONY_SERVICE;
        }
        if (serviceClass == SubscriptionManager.class) {
            return Context.TELEPHONY_SUBSCRIPTION_SERVICE;
        }
        return null;
    }

    public PersistableBundle getCarrierConfig() {
        return mCarrierConfig;
    }
}
