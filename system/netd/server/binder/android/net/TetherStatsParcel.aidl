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

/**
 * The statistics of tethering interface
 *
 * {@hide}
 */
parcelable TetherStatsParcel {
    /**
     * Parcel representing tethering interface statistics.
     *
     * This parcel is used by tetherGetStats, tetherOffloadGetStats and
     * tetherOffloadGetAndClearStats in INetd.aidl. tetherGetStats uses this parcel to return the
     * tethering statistics since netd startup and presents the interface via its interface name.
     * Both tetherOffloadGetStats and tetherOffloadGetAndClearStats use this parcel to return
     * the tethering statistics since tethering was first started. They present the interface via
     * its interface index. Note that the interface must be presented by either interface name
     * |iface| or interface index |ifIndex| in this parcel. The unused interface name is set to
     * an empty string "" by default and the unused interface index is set to 0 by default.
     */

    /** The interface name. */
    @utf8InCpp String iface;

    /** Total number of received bytes. */
    long rxBytes;

    /** Total number of received packets. */
    long rxPackets;

    /** Total number of transmitted bytes. */
    long txBytes;

    /** Total number of transmitted packets. */
    long txPackets;

    /** The interface index. */
    int ifIndex = 0;
}
