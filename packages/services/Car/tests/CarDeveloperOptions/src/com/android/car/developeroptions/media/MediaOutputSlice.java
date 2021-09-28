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

import static com.android.car.developeroptions.slices.CustomSliceRegistry.MEDIA_OUTPUT_SLICE_URI;

import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.drawable.IconCompat;
import androidx.slice.Slice;
import androidx.slice.builders.ListBuilder;
import androidx.slice.builders.SliceAction;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.slices.CustomSliceable;
import com.android.car.developeroptions.slices.SliceBackgroundWorker;
import com.android.car.developeroptions.slices.SliceBroadcastReceiver;
import com.android.settingslib.media.MediaDevice;

import java.util.List;

/**
 * Show the Media device that can be transfer the media.
 */
public class MediaOutputSlice implements CustomSliceable {

    private static final String TAG = "MediaOutputSlice";
    private static final String MEDIA_DEVICE_ID = "media_device_id";

    public static final String MEDIA_PACKAGE_NAME = "media_package_name";

    private final Context mContext;

    private MediaDeviceUpdateWorker mWorker;
    private String mPackageName;

    public MediaOutputSlice(Context context) {
        mContext = context;
        mPackageName = getUri().getQueryParameter(MEDIA_PACKAGE_NAME);
    }

    @VisibleForTesting
    void init(String packageName, MediaDeviceUpdateWorker worker) {
        mPackageName = packageName;
        mWorker = worker;
    }

    @Override
    public Slice getSlice() {
        final BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (!adapter.isEnabled()) {
            Log.d(TAG, "getSlice() Bluetooth is off");
            return null;
        }

        if (getWorker() == null) {
            Log.d(TAG, "getSlice() Can not get worker through uri!");
            return null;
        }

        final List<MediaDevice> devices = getMediaDevices();

        final MediaDevice connectedDevice = getWorker().getCurrentConnectedMediaDevice();
        final ListBuilder listBuilder = buildActiveDeviceHeader(connectedDevice);

        for (MediaDevice device : devices) {
            if (!TextUtils.equals(connectedDevice.getId(), device.getId())) {
                listBuilder.addRow(getMediaDeviceRow(device));
            }
        }

        return listBuilder.build();
    }

    private ListBuilder buildActiveDeviceHeader(MediaDevice device) {
        final String title = device.getName();
        final IconCompat icon = getDeviceIconCompat(device);

        final PendingIntent broadcastAction =
                getBroadcastIntent(mContext, device.getId(), device.hashCode());
        final SliceAction primarySliceAction = SliceAction.createDeeplink(broadcastAction, icon,
                ListBuilder.ICON_IMAGE, title);

        final ListBuilder listBuilder = new ListBuilder(mContext, MEDIA_OUTPUT_SLICE_URI,
                ListBuilder.INFINITY)
                .setAccentColor(COLOR_NOT_TINTED)
                .addRow(new ListBuilder.RowBuilder()
                        .setTitleItem(icon, ListBuilder.ICON_IMAGE)
                        .setTitle(title)
                        .setSubtitle(device.getSummary())
                        .setPrimaryAction(primarySliceAction));

        return listBuilder;
    }

    private IconCompat getDeviceIconCompat(MediaDevice device) {
        Drawable drawable = device.getIcon();
        if (drawable == null) {
            Log.d(TAG, "getDeviceIconCompat() device : " + device.getName() + ", drawable is null");
            // Use default Bluetooth device icon to handle getIcon() is null case.
            drawable = mContext.getDrawable(com.android.internal.R.drawable.ic_bt_headphones_a2dp);
        }

        return Utils.createIconWithDrawable(drawable);
    }

    private MediaDeviceUpdateWorker getWorker() {
        if (mWorker == null) {
            mWorker = (MediaDeviceUpdateWorker) SliceBackgroundWorker.getInstance(getUri());
            if (mWorker != null) {
                mWorker.setPackageName(mPackageName);
            }
        }
        return mWorker;
    }

    private List<MediaDevice> getMediaDevices() {
        final List<MediaDevice> devices = getWorker().getMediaDevices();
        return devices;
    }

    private ListBuilder.RowBuilder getMediaDeviceRow(MediaDevice device) {
        final String title = device.getName();
        final PendingIntent broadcastAction =
                getBroadcastIntent(mContext, device.getId(), device.hashCode());
        final IconCompat deviceIcon = getDeviceIconCompat(device);

        final ListBuilder.RowBuilder rowBuilder = new ListBuilder.RowBuilder()
                .setTitleItem(deviceIcon, ListBuilder.ICON_IMAGE)
                .setPrimaryAction(SliceAction.create(broadcastAction, deviceIcon,
                        ListBuilder.ICON_IMAGE, title))
                .setTitle(title)
                .setSubtitle(device.getSummary());

        return rowBuilder;
    }

    private PendingIntent getBroadcastIntent(Context context, String id, int requestCode) {
        final Intent intent = new Intent(getUri().toString());
        intent.setClass(context, SliceBroadcastReceiver.class);
        intent.putExtra(MEDIA_DEVICE_ID, id);
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        return PendingIntent.getBroadcast(context, requestCode /* requestCode */, intent,
                PendingIntent.FLAG_CANCEL_CURRENT);
    }

    @Override
    public Uri getUri() {
        return MEDIA_OUTPUT_SLICE_URI;
    }

    @Override
    public void onNotifyChange(Intent intent) {
        final MediaDeviceUpdateWorker worker = getWorker();
        final String id = intent != null ? intent.getStringExtra(MEDIA_DEVICE_ID) : "";
        final MediaDevice device = worker.getMediaDeviceById(id);
        if (device != null) {
            Log.d(TAG, "onNotifyChange() device name : " + device.getName());
            worker.connectDevice(device);
        }
    }

    @Override
    public Intent getIntent() {
        return null;
    }

    @Override
    public Class getBackgroundWorkerClass() {
        return MediaDeviceUpdateWorker.class;
    }
}
