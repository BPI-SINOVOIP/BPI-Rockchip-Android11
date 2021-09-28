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

package com.android.car.developeroptions.media;

import static com.android.car.developeroptions.slices.CustomSliceRegistry.MEDIA_OUTPUT_INDICATOR_SLICE_URI;

import android.annotation.ColorInt;
import android.app.PendingIntent;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import androidx.core.graphics.drawable.IconCompat;
import androidx.slice.Slice;
import androidx.slice.builders.ListBuilder;
import androidx.slice.builders.SliceAction;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.slices.CustomSliceable;
import com.android.internal.util.CollectionUtils;
import com.android.settingslib.bluetooth.A2dpProfile;
import com.android.settingslib.bluetooth.HearingAidProfile;
import com.android.settingslib.bluetooth.LocalBluetoothManager;
import com.android.settingslib.bluetooth.LocalBluetoothProfileManager;
import com.android.settingslib.media.MediaOutputSliceConstants;

import java.util.ArrayList;
import java.util.List;

public class MediaOutputIndicatorSlice implements CustomSliceable {

    private static final String TAG = "MediaOutputIndicatorSlice";

    private Context mContext;
    private LocalBluetoothManager mLocalBluetoothManager;
    private LocalBluetoothProfileManager mProfileManager;

    public MediaOutputIndicatorSlice(Context context) {
        mContext = context;
        mLocalBluetoothManager = com.android.car.developeroptions.bluetooth.Utils.getLocalBtManager(context);
        if (mLocalBluetoothManager == null) {
            Log.e(TAG, "Bluetooth is not supported on this device");
            return;
        }
        mProfileManager = mLocalBluetoothManager.getProfileManager();
    }

    @Override
    public Slice getSlice() {
        if (!isVisible()) {
            return null;
        }
        final IconCompat icon = IconCompat.createWithResource(mContext,
                com.android.internal.R.drawable.ic_settings_bluetooth);
        final CharSequence title = mContext.getText(R.string.media_output_title);
        final PendingIntent primaryActionIntent = PendingIntent.getActivity(mContext,
                0 /* requestCode */, getMediaOutputSliceIntent(), 0 /* flags */);
        final SliceAction primarySliceAction = SliceAction.createDeeplink(
                primaryActionIntent, icon, ListBuilder.ICON_IMAGE, title);
        @ColorInt final int color = Utils.getColorAccentDefaultColor(mContext);

        final ListBuilder listBuilder = new ListBuilder(mContext,
                MEDIA_OUTPUT_INDICATOR_SLICE_URI,
                ListBuilder.INFINITY)
                .setAccentColor(color)
                .addRow(new ListBuilder.RowBuilder()
                        .setTitle(title)
                        .setSubtitle(findActiveDeviceName())
                        .setPrimaryAction(primarySliceAction));
        return listBuilder.build();
    }

    private Intent getMediaOutputSliceIntent() {
        final Intent intent = new Intent()
                .setAction(MediaOutputSliceConstants.ACTION_MEDIA_OUTPUT)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        return intent;
    }

    @Override
    public Uri getUri() {
        return MEDIA_OUTPUT_INDICATOR_SLICE_URI;
    }

    @Override
    public Intent getIntent() {
        // This Slice reflects active media device information and launch MediaOutputSlice. It does
        // not contain its owned Slice data
        return null;
    }

    @Override
    public Class getBackgroundWorkerClass() {
        return MediaOutputIndicatorWorker.class;
    }

    private boolean isVisible() {
        // To decide Slice's visibility.
        // return true if device is connected or previously connected, false for other cases.
        return !CollectionUtils.isEmpty(getConnectableA2dpDevices())
                || !CollectionUtils.isEmpty(getConnectableHearingAidDevices());
    }

    private List<BluetoothDevice> getConnectableA2dpDevices() {
        // Get A2dp devices on all states
        // (STATE_DISCONNECTED, STATE_CONNECTING, STATE_CONNECTED,  STATE_DISCONNECTING)
        final A2dpProfile a2dpProfile = mProfileManager.getA2dpProfile();
        if (a2dpProfile == null) {
            return new ArrayList<>();
        }
        return a2dpProfile.getConnectableDevices();
    }

    private List<BluetoothDevice> getConnectableHearingAidDevices() {
        // Get hearing aid profile devices on all states
        // (STATE_DISCONNECTED, STATE_CONNECTING, STATE_CONNECTED,  STATE_DISCONNECTING)
        final HearingAidProfile hapProfile = mProfileManager.getHearingAidProfile();
        if (hapProfile == null) {
            return new ArrayList<>();
        }

        return hapProfile.getConnectableDevices();
    }

    private CharSequence findActiveDeviceName() {
        // Return Hearing Aid device name if it is active
        BluetoothDevice activeDevice = findActiveHearingAidDevice();
        if (activeDevice != null) {
            return activeDevice.getAlias();
        }
        // Return A2DP device name if it is active
        final A2dpProfile a2dpProfile = mProfileManager.getA2dpProfile();
        if (a2dpProfile != null) {
            activeDevice = a2dpProfile.getActiveDevice();
            if (activeDevice != null) {
                return activeDevice.getAlias();
            }
        }
        // No active device, return default summary
        return mContext.getText(R.string.media_output_default_summary);
    }

    private BluetoothDevice findActiveHearingAidDevice() {
        final HearingAidProfile hearingAidProfile = mProfileManager.getHearingAidProfile();
        if (hearingAidProfile == null) {
            return null;
        }

        final List<BluetoothDevice> activeDevices = hearingAidProfile.getActiveDevices();
        for (BluetoothDevice btDevice : activeDevices) {
            if (btDevice != null) {
                return btDevice;
            }
        }
        return null;
    }
}
