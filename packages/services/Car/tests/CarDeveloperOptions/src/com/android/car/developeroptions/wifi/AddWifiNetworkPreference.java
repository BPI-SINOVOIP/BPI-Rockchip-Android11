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
 * limitations under the License
 */

package com.android.car.developeroptions.wifi;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.widget.ImageButton;

import androidx.annotation.DrawableRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.wifi.dpp.WifiDppUtils;

/**
 * The Preference for users to add Wi-Fi networks in WifiSettings
 */
public class AddWifiNetworkPreference extends Preference {

    private static final String TAG = "AddWifiNetworkPreference";

    private boolean mInitialized;

    public AddWifiNetworkPreference(Context context) {
        super(context);

        setLayoutResource(com.android.settingslib.R.layout.preference_access_point);
        setWidgetLayoutResource(R.layout.wifi_button_preference_widget);
        setIcon(R.drawable.ic_menu_add);
        setTitle(R.string.wifi_add_network);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (!mInitialized) {
            mInitialized = true;

            final ImageButton imageButton = (ImageButton) holder.findViewById(R.id.button_icon);
            imageButton.setImageDrawable(getDrawable(R.drawable.ic_scan_24dp));
            imageButton.setContentDescription(
                    getContext().getString(R.string.wifi_dpp_scan_qr_code));
            imageButton.setOnClickListener(view -> {
                getContext().startActivity(
                    WifiDppUtils.getEnrolleeQrCodeScannerIntent(/* ssid */ null));
            });
        }
    }

    private Drawable getDrawable(@DrawableRes int iconResId) {
        Drawable buttonIcon = null;

        try {
            buttonIcon = getContext().getDrawable(iconResId);
        } catch (Resources.NotFoundException exception) {
            Log.e(TAG, "Resource does not exist: " + iconResId);
        }
        return buttonIcon;
    }
}
