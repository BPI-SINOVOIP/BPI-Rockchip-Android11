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
package com.android.car.notification.testutils;

import android.service.notification.StatusBarNotification;

import com.android.car.assist.client.CarAssistUtils;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.ArrayList;
import java.util.List;

@Implements(CarAssistUtils.class)
public class ShadowCarAssistUtils {

    private static List<String> sMessageNotificationSbnKeys = new ArrayList<>();
    private static int sRequestAssistantVoiceActionCount = 0;

    private static boolean mHasActiveAssistant = true;
    private static boolean mIsFallbackAssistantEnabled = false;

    @Implementation
    protected static boolean isCarCompatibleMessagingNotification(StatusBarNotification sbn) {
        return sMessageNotificationSbnKeys.contains(sbn.getKey());
    }

    @Implementation
    protected void requestAssistantVoiceAction(StatusBarNotification sbn, String voiceAction,
            CarAssistUtils.ActionRequestCallback callback) {
        sRequestAssistantVoiceActionCount++;
    }

    @Implementation
    public boolean hasActiveAssistant() {
        return mHasActiveAssistant;
    }

    @Implementation
    public boolean isFallbackAssistantEnabled() {
        return mIsFallbackAssistantEnabled;
    }

    public static void addMessageNotification(String messageSbnKey) {
        sMessageNotificationSbnKeys.add(messageSbnKey);
    }

    public static int getRequestAssistantVoiceActionCount() {
        return sRequestAssistantVoiceActionCount;
    }

    public static void setHasActiveAssistant(boolean hasActiveAssistant) {
        mHasActiveAssistant = hasActiveAssistant;
    }

    public static void setIsFallbackAssistantEnabled(boolean isFallbackAssistantEnabled) {
        mIsFallbackAssistantEnabled = isFallbackAssistantEnabled;
    }

    /**
     * Resets the shadow state.
     */
    @Resetter
    public static void reset() {
        sMessageNotificationSbnKeys.clear();
        sRequestAssistantVoiceActionCount = 0;

        mHasActiveAssistant = true;
        mIsFallbackAssistantEnabled = false;
    }
}
