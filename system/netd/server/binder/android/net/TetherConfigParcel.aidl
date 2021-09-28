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

package android.net;

/**
 * The configuration to start tethering.
 *
 * {@hide}
 */
parcelable TetherConfigParcel {
    // Whether to enable or disable legacy DNS proxy server.
    boolean usingLegacyDnsProxy;
    // DHCP ranges to set.
    // dhcpRanges might contain many addresss {addr1, addr2, addr3, addr4...}
    // Netd splits them into ranges: addr1-addr2, addr3-addr4, etc.
    // An odd number of addrs will fail.
    @utf8InCpp String[] dhcpRanges;
}
