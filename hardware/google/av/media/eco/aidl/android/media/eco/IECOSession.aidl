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

package android.media.eco;

import android.media.eco.ECOData;
import android.media.eco.IECOServiceStatsProvider;
import android.media.eco.IECOServiceInfoListener;

/**
 * Binder interface for ECO Session.
 * @hide
 */
interface IECOSession {
    /**
     * All ECO session Binder calls may return a ServiceSpecificException with the following error
     * codes.
     */
    const int ERROR_PERMISSION_DENIED = 1;
    const int ERROR_ALREADY_EXISTS = 2;
    const int ERROR_ILLEGAL_ARGUMENT = 3;
    const int ERROR_DISCONNECTED = 4;
    const int ERROR_INVALID_OPERATION = 5;
    const int ERROR_UNSUPPORTED = 6;

    /**
     * Adds an ECOServiceStasProvider.
     *
     * <p>This is called by ECOServiceStasProvider to add itself to the ECOSession.</p>
     *
     * @param provider Provider that implements IECOServiceStatsProvider interface.
     * @param config  Config that specifies the types of the stats that the provider is going to
     *                provide. The ECOData must be of type DATA_TYPE_STATS_PROVIDER_CONFIG. Note:
     *                The provider must specify the following keys when adding itself:
     *                KEY_PROVIDER_NAME in string and KEY_PROVIDER_TYPE as specified in
     *                IECOServiceStatsProvider.
     *
     * @return true if the provider was successfully added, false otherwise.
     */
    boolean addStatsProvider(IECOServiceStatsProvider provider, in ECOData config);

    /**
     * Removes an ECOServiceStasProvider from ECOSession.
     *
     * @param provider Provider that implements IECOServiceStatsProvider interface.
     *
     * @return true if the provider was successfully removed, false otherwise.
     */
    boolean removeStatsProvider(IECOServiceStatsProvider provider);

    /**
     * Registers an ECOServiceInfoListener.
     *
     * <p>This is called by IECOServiceInfoListener to add itself to the ECOSession.</p>
     *
     * @param listener Listener that implements IECOServiceInfoListener interface.
     * @param config  Config that specifies the condition for the data that the listener is
     *                interested in.
     *
     * @return true if the listener was successfully added, false otherwise.
     */
    boolean addInfoListener(IECOServiceInfoListener listener, in ECOData config);

    /**
     * Removes an ECOServiceInfoListener from ECOSession.
     *
     * @param listener Listener that implements IECOServiceInfoListener interface.
     *
     * @return true if the listener was successfully removed, false otherwise.
     */
    boolean removeInfoListener(IECOServiceInfoListener listener);

    /**
     * Push new stats to the ECOSession. This should only be called by IECOServiceStatsProvider.
     */
    boolean pushNewStats(in ECOData newStats);

    /**
     * Return the width in pixels.
     */
    int getWidth();

    /**
     * Return the height in pixels.
     */
    int getHeight();

    /**
     * Return whether the session is for camera recording.
     */
    boolean getIsCameraRecording();

    /**
     * Query the number of listeners that a session has.
     */
    int getNumOfListeners();

    /**
     * Query the number of providers that a session has.
     */
    int getNumOfProviders();
}
