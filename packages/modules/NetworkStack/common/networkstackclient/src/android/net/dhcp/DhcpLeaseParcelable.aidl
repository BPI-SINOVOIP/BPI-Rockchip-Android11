/**
 * Copyright (c) 2020, The Android Open Source Project
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

parcelable DhcpLeaseParcelable {
    // Client ID of the lease; may be null.
    byte[] clientId;
    // MAC address provided by the client.
    byte[] hwAddr;
    // IPv4 address of the lease, in network byte order.
    int netAddr;
    // Prefix length of the lease (0-32)
    int prefixLength;
    // Expiration time of the lease, to compare with SystemClock.elapsedRealtime().
    long expTime;
    // Hostname provided by the client, if any, or null.
    String hostname;
}