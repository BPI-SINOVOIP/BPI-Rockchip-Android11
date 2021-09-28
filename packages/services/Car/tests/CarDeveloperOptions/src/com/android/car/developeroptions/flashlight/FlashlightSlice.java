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

package com.android.car.developeroptions.flashlight;

import static android.app.slice.Slice.EXTRA_TOGGLE_STATE;

import static androidx.slice.builders.ListBuilder.ICON_IMAGE;

import android.annotation.ColorInt;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.net.Uri;
import android.provider.Settings;
import android.provider.Settings.Secure;
import android.util.Log;

import androidx.core.graphics.drawable.IconCompat;
import androidx.slice.Slice;
import androidx.slice.builders.ListBuilder;
import androidx.slice.builders.ListBuilder.RowBuilder;
import androidx.slice.builders.SliceAction;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.slices.CustomSliceRegistry;
import com.android.car.developeroptions.slices.CustomSliceable;


/**
 * Utility class to build a Flashlight Slice, and handle all associated actions.
 */
public class FlashlightSlice implements CustomSliceable {

    private static final String TAG = "FlashlightSlice";

    /**
     * Action broadcasting a change on whether flashlight is on or off.
     */
    private static final String ACTION_FLASHLIGHT_CHANGED =
            "com.android.car.developeroptions.flashlight.action.FLASHLIGHT_CHANGED";

    private final Context mContext;

    public FlashlightSlice(Context context) {
        mContext = context;
    }

    @Override
    public Slice getSlice() {
        if (!isFlashlightAvailable(mContext)) {
            return null;
        }
        final PendingIntent toggleAction = getBroadcastIntent(mContext);
        @ColorInt final int color = Utils.getColorAccentDefaultColor(mContext);
        final IconCompat icon =
                IconCompat.createWithResource(mContext, R.drawable.ic_signal_flashlight);
        return new ListBuilder(mContext, CustomSliceRegistry.FLASHLIGHT_SLICE_URI,
                ListBuilder.INFINITY)
                .setAccentColor(color)
                .addRow(new RowBuilder()
                        .setTitle(mContext.getText(R.string.power_flashlight))
                        .setTitleItem(icon, ICON_IMAGE)
                        .setPrimaryAction(
                                SliceAction.createToggle(toggleAction, null,
                                        isFlashlightEnabled(mContext))))
                .build();
    }

    @Override
    public Uri getUri() {
        return CustomSliceRegistry.FLASHLIGHT_SLICE_URI;
    }

    @Override
    public IntentFilter getIntentFilter() {
        return new IntentFilter(ACTION_FLASHLIGHT_CHANGED);
    }

    @Override
    public void onNotifyChange(Intent intent) {
        try {
            final String cameraId = getCameraId(mContext);
            if (cameraId != null) {
                final boolean state = intent.getBooleanExtra(
                        EXTRA_TOGGLE_STATE, isFlashlightEnabled(mContext));
                final CameraManager cameraManager = mContext.getSystemService(CameraManager.class);
                cameraManager.setTorchMode(cameraId, state);
            }
        } catch (CameraAccessException e) {
            Log.e(TAG, "Camera couldn't set torch mode.", e);
        }
        mContext.getContentResolver().notifyChange(CustomSliceRegistry.FLASHLIGHT_SLICE_URI, null);
    }

    @Override
    public Intent getIntent() {
        return null;
    }

    private static String getCameraId(Context context) throws CameraAccessException {
        final CameraManager cameraManager = context.getSystemService(CameraManager.class);
        final String[] ids = cameraManager.getCameraIdList();
        for (String id : ids) {
            CameraCharacteristics c = cameraManager.getCameraCharacteristics(id);
            Boolean flashAvailable = c.get(CameraCharacteristics.FLASH_INFO_AVAILABLE);
            Integer lensFacing = c.get(CameraCharacteristics.LENS_FACING);
            if (flashAvailable != null && flashAvailable
                    && lensFacing != null && lensFacing == CameraCharacteristics.LENS_FACING_BACK) {
                return id;
            }
        }
        return null;
    }


    private static boolean isFlashlightAvailable(Context context) {
        return Settings.Secure.getInt(
                context.getContentResolver(), Secure.FLASHLIGHT_AVAILABLE, 0) == 1;
    }

    private static boolean isFlashlightEnabled(Context context) {
        return Settings.Secure.getInt(
                context.getContentResolver(), Secure.FLASHLIGHT_ENABLED, 0) == 1;
    }
}
