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

import android.os.RemoteException;

/**
 * Used to set the current set of UX restrictions being enforced in the test.
 */
public interface CarUxRestrictionsController {
    /**
     * Sets the current set of UX restrictions.
     *
     * @param restrictions is a bitmask of UX restrictions as defined in {@link
     * android.car.drivingstate.CarUxRestrictions}
     * @throws RemoteException when the underlying service fails to set the given restrictions.
     */
    void setUxRestrictions(int restrictions) throws RemoteException;

    /**
     * Clears all UX restrictions.
     *
     * @throws RemoteException when the underlying service fails to clear the restrictions.
     */
    void clearUxRestrictions() throws RemoteException;

    /**
     * Returns {@code true} if a {@link
     * android.car.drivingstate.CarUxRestrictionsManager.OnUxRestrictionsChangedListener} is
     * registered with the manager, otherwise returns {@code false}.
     */
    boolean isListenerRegistered();
}
