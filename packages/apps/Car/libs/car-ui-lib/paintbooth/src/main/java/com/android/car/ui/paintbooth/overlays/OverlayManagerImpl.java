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

package com.android.car.ui.paintbooth.overlays;

import android.app.ActivityManager;
import android.content.Context;
import android.content.om.IOverlayManager;
import android.os.RemoteException;
import android.os.ServiceManager;

import androidx.annotation.NonNull;

import static java.util.stream.Collectors.toList;
import static java.util.stream.Collectors.toMap;

import java.util.List;
import java.util.Map;

/**
 * OverlayManager implementation. Exclude this file when building Paintbooth outside of the system
 * image.
 */
public class OverlayManagerImpl implements OverlayManager {
    private final IOverlayManager mOverlayManager;

    public OverlayManagerImpl(Context context) {
        mOverlayManager = IOverlayManager.Stub.asInterface(
                ServiceManager.getService(Context.OVERLAY_SERVICE));
    }

    private OverlayInfo convertOverlayInfo(android.content.om.OverlayInfo info) {
        return new OverlayInfo() {
            @NonNull
            @Override
            public String getPackageName() {
                return info.packageName;
            }

            @Override
            public boolean isEnabled() {
                return info.state == android.content.om.OverlayInfo.STATE_ENABLED;
            }
        };
    }

    @Override
    @NonNull
    public Map<String, List<OverlayManager.OverlayInfo>> getOverlays() throws RemoteException {
        Map<String, List<android.content.om.OverlayInfo>> overlays =
                mOverlayManager.getAllOverlays(ActivityManager.getCurrentUser());
        return overlays.entrySet()
                .stream()
                .collect(toMap(Map.Entry::getKey, e -> e.getValue()
                        .stream()
                        .map(this::convertOverlayInfo)
                        .collect(toList())));
    }

    @Override
    public void applyOverlay(@NonNull String packageName, boolean enable) throws RemoteException {
        mOverlayManager.setEnabled(packageName, enable, ActivityManager.getCurrentUser());
    }
}
