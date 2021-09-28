/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.net;

import android.net.CaptivePortalData;
import android.net.DataStallReportParcelable;
import android.net.INetworkMonitor;
import android.net.NetworkTestResultParcelable;
import android.net.PrivateDnsConfigParcel;

/** @hide */
oneway interface INetworkMonitorCallbacks {
    void onNetworkMonitorCreated(in INetworkMonitor networkMonitor) = 0;

    // Deprecated. Use notifyNetworkTestedWithExtras() instead.
    void notifyNetworkTested(int testResult, @nullable String redirectUrl) = 1;
    void notifyPrivateDnsConfigResolved(in PrivateDnsConfigParcel config) = 2;
    void showProvisioningNotification(String action, String packageName) = 3;
    void hideProvisioningNotification() = 4;
    void notifyProbeStatusChanged(int probesCompleted, int probesSucceeded) = 5;
    void notifyNetworkTestedWithExtras(in NetworkTestResultParcelable result) = 6;
    void notifyDataStallSuspected(in DataStallReportParcelable report) = 7;
    void notifyCaptivePortalDataChanged(in CaptivePortalData data) = 8;
}