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
import android.media.eco.IECOSession;

/**
 * Binder interface for ECO (Encoder Camera Optimization) service.
 * @hide
 */
interface IECOService {
    /**
     * All ECO service Binder calls may return a ServiceSpecificException with the following error
     * codes.
     */
    const int ERROR_PERMISSION_DENIED = 1;
    const int ERROR_ALREADY_EXISTS = 2;
    const int ERROR_ILLEGAL_ARGUMENT = 3;
    const int ERROR_DISCONNECTED = 4;
    const int ERROR_INVALID_OPERATION = 5;
    const int ERROR_UNSUPPORTED = 6;

    /**
     * Obtains an IECOSession from the ECO service.
     *
     * <p>ECOService will first check if the requested session already existed. If not, it will
     * create and return the new session. This should be called by IECOServiceStatsProvider or
     * IECOServiceInfoListener to obtain an ECOSession interface upon start.</p>
     *
     * @param width Width of the requested video session (in pixel).
     * @param height Height of the requested video session (in pixel).
     * @param isCameraRecording A boolean indicates whether the session is for camera recording.
     *
     * @return IECOSession The session instance created by the ECOService.
     */
    IECOSession obtainSession(int width, int height, boolean isCameraRecording);

    /**
     * Return the total number of sessions inside ECO service.
     */
    int getNumOfSessions();

    /**
     * Return a list containing all ECOSessions.
     */
    List<IBinder> getSessions();
}
