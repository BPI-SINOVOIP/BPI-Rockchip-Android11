/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.googlecode.android_scripting.facade.telephony;

import android.app.Service;
import android.content.Context;
import android.net.TetheringManager;
import android.net.TetheringManager.OnTetheringEntitlementResultListener;
import android.telephony.CarrierConfigManager;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.AndroidFacade;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcParameter;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

public class CarrierConfigFacade extends RpcReceiver {
    private final Service mService;
    private final AndroidFacade mAndroidFacade;
    private final CarrierConfigManager mCarrierConfigManager;
    private final TetheringManager mTetheringManager;

    public CarrierConfigFacade(FacadeManager manager) {
        super(manager);
        mService = manager.getService();
        mAndroidFacade = manager.getReceiver(AndroidFacade.class);
        mCarrierConfigManager =
            (CarrierConfigManager)mService.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        mTetheringManager = (TetheringManager) mService.getSystemService(Context.TETHERING_SERVICE);

    }

    private class EntitlementResultListener implements OnTetheringEntitlementResultListener {
        private final CompletableFuture<Integer> mFuture = new CompletableFuture<>();

        @Override
        public void onTetheringEntitlementResult(int result) {
            mFuture.complete(result);
        }

        public int get(int timeout, TimeUnit unit) throws Exception {
            return mFuture.get(timeout, unit);
        }

    }

    @Rpc(description = "Tethering Entitlement Check")
    public boolean carrierConfigIsTetheringModeAllowed(
        @RpcParameter(name="mode") String mode,
        @RpcParameter(name="timeout") Integer timeout) {
        final int tetheringType;
        if (mode.equals("wifi")){
            tetheringType = TetheringManager.TETHERING_WIFI;
        } else if (mode.equals("usb")) {
            tetheringType = TetheringManager.TETHERING_USB;
        } else if (mode.equals("bluetooth")) {
            tetheringType = TetheringManager.TETHERING_BLUETOOTH;
        } else {
            tetheringType = TetheringManager.TETHERING_INVALID;
        }

        final EntitlementResultListener listener = new EntitlementResultListener();
        mTetheringManager.requestLatestTetheringEntitlementResult(tetheringType, true,
                c -> c.run(), listener);
        int result;
        try{
            result = listener.get(timeout, TimeUnit.MILLISECONDS);
        } catch (Exception e) {
            Log.d("phoneTetherCheck exception" + e.toString());
            return false;
        }

        return result == TetheringManager.TETHER_ERROR_NO_ERROR;
    }

    @Override
    public void shutdown() {

    }
}
