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
import android.os.IBinder;

/**
 * Binder interface for ECO service information listener.
*
* {@hide}
*/
interface IECOServiceInfoListener {
    /**
     * All listener Binder calls may return a ServiceSpecificException with the following error
     * codes.
     */
    const int ERROR_PERMISSION_DENIED = 1;
    const int ERROR_ILLEGAL_ARGUMENT = 2;
    const int ERROR_INVALID_OPERATION = 3;
    const int ERROR_UNSUPPORTED = 4;

    /**
     * Constants for the type of the listener.
     */
    const int INFO_LISTENER_TYPE_UNKNOWN = 1;
    const int INFO_LISTENER_TYPE_VIDEO_ENCODER = 2;
    const int INFO_LISTENER_TYPE_CAMERA = 3;

    /**
     * Return the type of the listener.
     */
    int getType();

    /**
     * Return the name of the listener.
     */
    String getName();

    /**
     * Return the IBinder instance of the ECOSession associated the provider.
     */
    IBinder getECOSession();

    /**
     * Handle the new info from ECOSession. This should only be called by ECOSession.
     */
    oneway void onNewInfo(in ECOData newInfo);
}