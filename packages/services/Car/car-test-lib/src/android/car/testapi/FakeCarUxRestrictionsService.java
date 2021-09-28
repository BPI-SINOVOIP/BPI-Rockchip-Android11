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

package android.car.testapi;

import static android.car.drivingstate.CarUxRestrictions.UX_RESTRICTIONS_BASELINE;

import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.CarUxRestrictionsConfiguration;
import android.car.drivingstate.ICarUxRestrictionsChangeListener;
import android.car.drivingstate.ICarUxRestrictionsManager;
import android.os.IRemoteCallback;
import android.os.RemoteException;
import android.os.SystemClock;

import com.android.internal.annotations.GuardedBy;

import java.util.Collections;
import java.util.List;

/**
 * A fake implementation of {@link ICarUxRestrictionsManager.Stub} to facilitate the use of {@link
 * android.car.drivingstate.CarUxRestrictionsManager} in external unit tests.
 *
 * @hide
 */
public class FakeCarUxRestrictionsService extends ICarUxRestrictionsManager.Stub implements
        CarUxRestrictionsController {
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private CarUxRestrictions mCarUxRestrictions;
    @GuardedBy("mLock")
    private ICarUxRestrictionsChangeListener mListener;

    @GuardedBy("mLock")
    private String mMode = "baseline";

    private static CarUxRestrictions createCarUxRestrictions(int activeRestrictions) {
        return new CarUxRestrictions.Builder(
                false, /* requires driving distraction optimization */
                activeRestrictions,
                SystemClock.elapsedRealtimeNanos())
                .build();
    }

    FakeCarUxRestrictionsService() {
        synchronized (mLock) {
            mCarUxRestrictions = createCarUxRestrictions(UX_RESTRICTIONS_BASELINE);
        }
    }

    @Override
    public void registerUxRestrictionsChangeListener(
            ICarUxRestrictionsChangeListener listener, int displayId) {
        synchronized (mLock) {
            this.mListener = listener;
        }
    }

    @Override
    public void unregisterUxRestrictionsChangeListener(ICarUxRestrictionsChangeListener listener) {
        synchronized (mLock) {
            this.mListener = null;
        }
    }

    @Override
    public CarUxRestrictions getCurrentUxRestrictions(int displayId) {
        synchronized (mLock) {
            return mCarUxRestrictions;
        }
    }

    @Override
    public List<CarUxRestrictionsConfiguration> getStagedConfigs() {
        return Collections.emptyList();
    }

    @Override
    public List<CarUxRestrictionsConfiguration> getConfigs() {
        return Collections.emptyList();
    }

    @Override
    public boolean setRestrictionMode(String mode) throws RemoteException {
        synchronized (mLock) {
            mMode = mode;
        }
        return true;
    }

    @Override
    public String getRestrictionMode() throws RemoteException {
        synchronized (mLock) {
            return mMode;
        }
    }

    @Override
    public void reportVirtualDisplayToPhysicalDisplay(IRemoteCallback binder, int virtualDisplayId,
            int physicalDisplayId) throws RemoteException {

    }

    @Override
    public int getMappedPhysicalDisplayOfVirtualDisplay(int displayId) throws RemoteException {
        return 0;
    }

    @Override
    public boolean saveUxRestrictionsConfigurationForNextBoot(
            List<CarUxRestrictionsConfiguration> config) {
        return true;
    }

    /**************************** CarUxRestrictionsController impl ********************************/

    @Override
    public void setUxRestrictions(int restrictions) throws RemoteException {
        synchronized (mLock) {
            mCarUxRestrictions = createCarUxRestrictions(restrictions);
            if (isListenerRegistered()) {
                mListener.onUxRestrictionsChanged(mCarUxRestrictions);
            }
        }
    }

    @Override
    public void clearUxRestrictions() throws RemoteException {
        setUxRestrictions(0);
    }

    @Override
    public boolean isListenerRegistered() {
        synchronized (mLock) {
            return mListener != null;
        }
    }

}
