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

package com.android.car.developeroptions.homepage.contextualcards;

import android.annotation.Nullable;

import com.android.car.developeroptions.intelligence.ContextualCardProto.ContextualCard;
import com.android.car.developeroptions.intelligence.ContextualCardProto.ContextualCardList;
import com.android.car.developeroptions.slices.CustomSliceRegistry;

/** Provides dynamic card for SettingsIntelligence. */
public class SettingsContextualCardProvider {

    public static final String CARD_AUTHORITY = "com.android.car.developeroptions.homepage.contextualcards";

    @Nullable
    public ContextualCardList getContextualCards() {
        final ContextualCard wifiCard =
                ContextualCard.newBuilder()
                        .setSliceUri(CustomSliceRegistry.CONTEXTUAL_WIFI_SLICE_URI.toString())
                        .setCardName(CustomSliceRegistry.CONTEXTUAL_WIFI_SLICE_URI.toString())
                        .setCardCategory(ContextualCard.Category.IMPORTANT)
                        .build();
        final ContextualCard connectedDeviceCard =
                ContextualCard.newBuilder()
                        .setSliceUri(CustomSliceRegistry.BLUETOOTH_DEVICES_SLICE_URI.toString())
                        .setCardName(CustomSliceRegistry.BLUETOOTH_DEVICES_SLICE_URI.toString())
                        .setCardCategory(ContextualCard.Category.IMPORTANT)
                        .build();
        final ContextualCard lowStorageCard =
                ContextualCard.newBuilder()
                        .setSliceUri(CustomSliceRegistry.LOW_STORAGE_SLICE_URI.toString())
                        .setCardName(CustomSliceRegistry.LOW_STORAGE_SLICE_URI.toString())
                        .setCardCategory(ContextualCard.Category.IMPORTANT)
                        .build();
        final ContextualCard batteryFixCard =
                ContextualCard.newBuilder()
                        .setSliceUri(CustomSliceRegistry.BATTERY_FIX_SLICE_URI.toString())
                        .setCardName(CustomSliceRegistry.BATTERY_FIX_SLICE_URI.toString())
                        .setCardCategory(ContextualCard.Category.IMPORTANT)
                        .build();
        final String contextualNotificationChannelSliceUri =
                CustomSliceRegistry.CONTEXTUAL_NOTIFICATION_CHANNEL_SLICE_URI.toString();
        final ContextualCard notificationChannelCard =
                ContextualCard.newBuilder()
                        .setSliceUri(contextualNotificationChannelSliceUri)
                        .setCardName(contextualNotificationChannelSliceUri)
                        .setCardCategory(ContextualCard.Category.POSSIBLE)
                        .build();
        final ContextualCardList cards = ContextualCardList.newBuilder()
                .addCard(wifiCard)
                .addCard(connectedDeviceCard)
                .addCard(lowStorageCard)
                .addCard(batteryFixCard)
                .addCard(notificationChannelCard)
                .build();

        return cards;
    }
}
