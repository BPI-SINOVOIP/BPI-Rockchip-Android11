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

import android.os.IBinder;

/**
* An interface for providers that provides various statistics to ECO service.
* {@hide}
*/
interface IECOServiceStatsProvider {
    /**
     * All provider Binder calls may return a ServiceSpecificException with the following error
     * codes.
     */
    const int ERROR_PERMISSION_DENIED = 1;
    const int ERROR_ILLEGAL_ARGUMENT = 2;
    const int ERROR_INVALID_OPERATION = 3;
    const int ERROR_UNSUPPORTED = 4;

    /**
     * Constants for the type of the provider.
     */
    const int STATS_PROVIDER_TYPE_UNKNOWN = 1;
    const int STATS_PROVIDER_TYPE_VIDEO_ENCODER = 2;
    const int STATS_PROVIDER_TYPE_CAMERA = 3;

    /**
     * Return the type of the provider.
     */
    int getType();

    /**
     * Return the name of the provider.
     */
    String getName();

    /**
     * Return the IBinder instance of the ECOSession associated the listener.
     */
    IBinder getECOSession();
}
