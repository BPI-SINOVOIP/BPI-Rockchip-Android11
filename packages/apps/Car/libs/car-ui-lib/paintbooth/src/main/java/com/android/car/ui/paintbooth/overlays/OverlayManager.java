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

import android.content.Context;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;

import java.lang.reflect.InvocationTargetException;
import java.util.List;
import java.util.Map;

/**
 * Abstraction around {@link IOverlayManager}, used to deal with the fact that this is a hidden
 * API. {@link OverlayManagerImpl} should be excluded when compiling Paintbooth outside of the
 * system image.
 */
public interface OverlayManager {
    String TAG = OverlayManager.class.getSimpleName();

    /** Information about a single overlay affecting a target APK */
    interface OverlayInfo {
        /** Name of the overlay */
        @NonNull
        String getPackageName();
        /** Whether this overlay is enabled or not */
        boolean isEnabled();
    }

    /**
     * Returns a map of available overlays, indexed by the package name each overlay applies to
     */
    @NonNull
    Map<String, List<OverlayInfo>> getOverlays() throws RemoteException;

    /**
     * Enables/disables a given overlay
     * @param packageName an overlay package name (obtained from
     * {@link OverlayInfo#getPackageName()})
     */
    void applyOverlay(@NonNull String packageName, boolean enable) throws RemoteException;

    /** A null {@link OverlayManager} */
    final class OverlayManagerStub implements OverlayManager {
        @Override
        @NonNull
        public Map<String, List<OverlayInfo>> getOverlays() throws RemoteException {
            throw new RemoteException("Overlay manager is not available");
        }

        @Override
        public void applyOverlay(@NonNull String packageName, boolean enable)
                throws RemoteException {
            throw new RemoteException("Overlay manager is not available");
        }
    }

    /** Returns a valid {@link OverlayManager} for this environment */
    @NonNull
    static OverlayManager getInstance(Context context) {
        try {
            return (OverlayManager) Class
                    .forName("com.android.car.ui.paintbooth.overlays.OverlayManagerImpl")
                    .getConstructor(Context.class)
                    .newInstance(context);
        } catch (ClassNotFoundException | IllegalAccessException | InstantiationException
                | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "Overlay Manager is not available");
            return new OverlayManagerStub();
        }
    }
}
