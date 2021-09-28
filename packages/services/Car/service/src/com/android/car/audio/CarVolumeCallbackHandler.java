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
package com.android.car.audio;

import android.car.media.ICarVolumeCallback;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;

import com.android.car.BinderInterfaceContainer;
import com.android.car.CarLog;

/**
 * Manages callbacks for changes in car volume
 */
class CarVolumeCallbackHandler {
    private final BinderInterfaceContainer<ICarVolumeCallback> mVolumeCallbackContainer =
            new BinderInterfaceContainer<>();

    void release() {
        mVolumeCallbackContainer.clear();
    }

    void onVolumeGroupChange(int zoneId, int groupId, int flags) {
        for (BinderInterfaceContainer.BinderInterface<ICarVolumeCallback> callback :
                mVolumeCallbackContainer.getInterfaces()) {
            try {
                callback.binderInterface.onGroupVolumeChanged(zoneId, groupId, flags);
            } catch (RemoteException e) {
                Log.e(CarLog.TAG_AUDIO, "Failed to callback onGroupVolumeChanged", e);
            }
        }
    }

    void onMasterMuteChanged(int zoneId, int flags) {
        for (BinderInterfaceContainer.BinderInterface<ICarVolumeCallback> callback :
                mVolumeCallbackContainer.getInterfaces()) {
            try {
                callback.binderInterface.onMasterMuteChanged(zoneId, flags);
            } catch (RemoteException e) {
                Log.e(CarLog.TAG_AUDIO, "Failed to callback onMasterMuteChanged", e);
            }
        }
    }

    public void registerCallback(@NonNull IBinder binder) {
        mVolumeCallbackContainer.addBinder(ICarVolumeCallback.Stub.asInterface(binder));
    }

    public void unregisterCallback(@NonNull IBinder binder) {
        mVolumeCallbackContainer.removeBinder(ICarVolumeCallback.Stub.asInterface(binder));
    }
}
