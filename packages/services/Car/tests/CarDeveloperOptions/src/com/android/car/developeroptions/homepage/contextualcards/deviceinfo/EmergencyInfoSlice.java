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

package com.android.car.developeroptions.homepage.contextualcards.deviceinfo;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.core.graphics.drawable.IconCompat;
import androidx.slice.Slice;
import androidx.slice.builders.ListBuilder;
import androidx.slice.builders.SliceAction;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.accounts.EmergencyInfoPreferenceController;
import com.android.car.developeroptions.slices.CustomSliceRegistry;
import com.android.car.developeroptions.slices.CustomSliceable;

// This is a slice helper class for EmergencyInfo
public class EmergencyInfoSlice implements CustomSliceable {

    private final Context mContext;

    public EmergencyInfoSlice(Context context) {
        mContext = context;
    }

    @Override
    public Slice getSlice() {
        final ListBuilder listBuilder = new ListBuilder(mContext,
                CustomSliceRegistry.EMERGENCY_INFO_SLICE_URI,
                ListBuilder.INFINITY);
        listBuilder.addRow(
                new ListBuilder.RowBuilder()
                        .setTitle(mContext.getText(R.string.emergency_info_title))
                        .setSubtitle(
                                mContext.getText(R.string.emergency_info_contextual_card_summary))
                        .setPrimaryAction(createPrimaryAction()));
        return listBuilder.build();
    }

    @Override
    public Uri getUri() {
        return CustomSliceRegistry.EMERGENCY_INFO_SLICE_URI;
    }

    @Override
    public Intent getIntent() {
        return new Intent(EmergencyInfoPreferenceController.getIntentAction(mContext))
                .setPackage(Utils.SETTINGS_PACKAGE_NAME);
    }

    @Override
    public void onNotifyChange(Intent intent) {
    }

    private SliceAction createPrimaryAction() {
        final PendingIntent pendingIntent =
                PendingIntent.getActivity(
                        mContext,
                        0 /* requestCode */,
                        getIntent(),
                        PendingIntent.FLAG_UPDATE_CURRENT);

        return SliceAction.createDeeplink(
                pendingIntent,
                IconCompat.createWithResource(mContext, R.drawable.empty_icon),
                ListBuilder.ICON_IMAGE,
                mContext.getText(R.string.emergency_info_title));
    }
}
