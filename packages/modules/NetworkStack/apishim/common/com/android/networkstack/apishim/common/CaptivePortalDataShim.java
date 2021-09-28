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

package com.android.networkstack.apishim.common;

import android.net.INetworkMonitorCallbacks;
import android.net.Uri;
import android.os.RemoteException;

/**
 * Compatibility interface for {@link android.net.CaptivePortalData}.
 */
public interface CaptivePortalDataShim {
    /**
     * @see android.net.CaptivePortalData#isCaptive()
     */
    boolean isCaptive();

    /**
     * @see android.net.CaptivePortalData#getByteLimit()
     */
    long getByteLimit();

    /**
     * @see android.net.CaptivePortalData#getExpiryTimeMillis()
     */
    long getExpiryTimeMillis();

    /**
     * @see android.net.CaptivePortalData#getUserPortalUrl()
     */
    Uri getUserPortalUrl();

    /**
     * @see android.net.CaptivePortalData#getVenueInfoUrl()
     */
    Uri getVenueInfoUrl();

    /**
     * @see INetworkMonitorCallbacks#notifyCaptivePortalDataChanged(android.net.CaptivePortalData)
     */
    void notifyChanged(INetworkMonitorCallbacks cb) throws RemoteException;
}
