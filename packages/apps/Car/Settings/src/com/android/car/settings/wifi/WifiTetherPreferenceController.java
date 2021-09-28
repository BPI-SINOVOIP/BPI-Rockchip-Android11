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

package com.android.car.settings.wifi;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.net.TetheringManager;
import android.os.HandlerExecutor;

import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.internal.os.BackgroundThread;

/**
 * Controls the availability of wifi tethering preference based on whether tethering is supported
 */
public class WifiTetherPreferenceController extends PreferenceController<Preference> {

    private final TetheringManager mTetheringManager =
            getContext().getSystemService(TetheringManager.class);
    private volatile boolean mIsTetheringSupported;
    private volatile boolean mReceivedTetheringEventCallback = false;

    private TetheringManager.TetheringEventCallback mTetheringCallback =
            new TetheringManager.TetheringEventCallback() {
                @Override
                public void onTetheringSupported(boolean supported) {
                    mReceivedTetheringEventCallback = true;
                    if (mIsTetheringSupported != supported) {
                        mIsTetheringSupported = supported;
                    }
                    refreshUi();
                }
            };

    public WifiTetherPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    @Override
    protected void onStartInternal() {
        mTetheringManager.registerTetheringEventCallback(
                new HandlerExecutor(BackgroundThread.getHandler()), mTetheringCallback);
    }

    @Override
    protected void onStopInternal() {
        mTetheringManager.unregisterTetheringEventCallback(mTetheringCallback);
    }

    @Override
    protected int getAvailabilityStatus() {
        if (!mReceivedTetheringEventCallback) {
            return AVAILABLE_FOR_VIEWING;
        }
        return  mIsTetheringSupported ? AVAILABLE : UNSUPPORTED_ON_DEVICE;
    }
}
