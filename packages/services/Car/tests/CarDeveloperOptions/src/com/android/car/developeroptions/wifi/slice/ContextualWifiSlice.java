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

package com.android.car.developeroptions.wifi.slice;

import android.content.Context;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.slice.Slice;

import com.android.car.developeroptions.overlay.FeatureFactory;
import com.android.car.developeroptions.slices.CustomSliceRegistry;
import com.android.car.developeroptions.slices.CustomSliceable;

/**
 * {@link CustomSliceable} for Wi-Fi, used by contextual homepage.
 */
public class ContextualWifiSlice extends WifiSlice {

    private static final String TAG = "ContextualWifiSlice";
    @VisibleForTesting
    static long sActiveUiSession = -1000;
    @VisibleForTesting
    static boolean sPreviouslyDisplayed;

    public ContextualWifiSlice(Context context) {
        super(context);
    }

    @Override
    public Uri getUri() {
        return CustomSliceRegistry.CONTEXTUAL_WIFI_SLICE_URI;
    }

    @Override
    public Slice getSlice() {
        final long currentUiSession = FeatureFactory.getFactory(mContext)
                .getSlicesFeatureProvider().getUiSessionToken();
        if (currentUiSession != sActiveUiSession) {
            sActiveUiSession = currentUiSession;
            sPreviouslyDisplayed = false;
        }
        if (!sPreviouslyDisplayed && hasWorkingNetwork()) {
            Log.d(TAG, "Wifi is connected, no point showing any suggestion.");
            return null;
        }
        // Set sPreviouslyDisplayed to true - we will show *something* on the screen. So we should
        // keep showing this card to keep UI stable, even if wifi connects to a network later.
        sPreviouslyDisplayed = true;

        return super.getSlice();
    }

    private boolean hasWorkingNetwork() {
        return !TextUtils.equals(getActiveSSID(), WifiManager.UNKNOWN_SSID) && !isCaptivePortal();
    }
}
