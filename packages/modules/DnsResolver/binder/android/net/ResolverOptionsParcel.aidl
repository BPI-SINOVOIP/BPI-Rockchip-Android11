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

import android.net.ResolverHostsParcel;

/**
 * Knobs for OEM to control alternative behavior.
 *
 * {@hide}
 */
parcelable ResolverOptionsParcel {
    /**
     * An IP/hostname mapping table for DNS local lookup customization.
     * WARNING: this is intended for local testing and other special situations.
     * Future versions of the DnsResolver module may break your assumptions.
     * Injecting static mappings for public hostnames is generally A VERY BAD IDEA,
     * since it makes it impossible for the domain owners to migrate the domain.
     * It is also not an effective domain blocking mechanism, because apps can
     * easily hardcode IPs or bypass the system DNS resolver.
     */
    ResolverHostsParcel[] hosts = {};

    /**
     * Truncated UDP DNS response handling mode. Handling of UDP responses with the TC (truncated)
     * bit set. The values are defined in {@code IDnsResolver.aidl}
     * 0: TC_MODE_DEFAULT
     * 1: TC_MODE_UDP_TCP
     * Other values are invalid.
     */
    int tcMode = 0;

    /**
     * The default behavior is that plaintext DNS queries are sent by the application's UID using
     * fchown(). DoT are sent with an UID of AID_DNS. This option control the plaintext uid of DNS
     * query.
     * Setting this option to true decreases battery life because it results in the device sending
     * UDP DNS queries even if the app that made the DNS lookup does not have network access.
     * Anecdotal data from the field suggests that about 15% of DNS lookups are in this category.
     * This option also results in data usage for UDP DNS queries being attributed to the OS instead
     * of to the requesting app.
     * false: set application uid on DNS sockets (default)
     * true: set AID_DNS on DNS sockets
     */
    boolean enforceDnsUid = false;
}
