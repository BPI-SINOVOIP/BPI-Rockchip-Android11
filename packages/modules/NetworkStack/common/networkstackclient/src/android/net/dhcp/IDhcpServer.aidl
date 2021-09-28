/**
 * Copyright (c) 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing perNmissions and
 * limitations under the License.
 */

package android.net.dhcp;

import android.net.INetworkStackStatusCallback;
import android.net.dhcp.DhcpServingParamsParcel;
import android.net.dhcp.IDhcpEventCallbacks;

/** @hide */
oneway interface IDhcpServer {
    const int STATUS_UNKNOWN = 0;
    const int STATUS_SUCCESS = 1;
    const int STATUS_INVALID_ARGUMENT = 2;
    const int STATUS_UNKNOWN_ERROR = 3;

    void start(in INetworkStackStatusCallback cb) = 0;
    void startWithCallbacks(in INetworkStackStatusCallback statusCb,
        in IDhcpEventCallbacks eventCb) = 3;
    void updateParams(in DhcpServingParamsParcel params, in INetworkStackStatusCallback cb) = 1;
    void stop(in INetworkStackStatusCallback cb) = 2;

    // Next available ID: 4
}
