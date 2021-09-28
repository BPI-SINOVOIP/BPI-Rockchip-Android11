/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.net.netlink

import android.net.netlink.NetlinkConstants.RTM_DELNEIGH
import android.net.netlink.NetlinkConstants.RTM_NEWNEIGH
import libcore.util.HexEncoding
import libcore.util.HexEncoding.encodeToString
import java.net.Inet6Address
import java.net.InetAddress

/**
 * Make a RTM_NEWNEIGH netlink message.
 */
fun makeNewNeighMessage(
    neighAddr: InetAddress,
    nudState: Short
) = makeNeighborMessage(
        neighAddr = neighAddr,
        type = RTM_NEWNEIGH,
        nudState = nudState
)

/**
 * Make a RTM_DELNEIGH netlink message.
 */
fun makeDelNeighMessage(
    neighAddr: InetAddress,
    nudState: Short
) = makeNeighborMessage(
        neighAddr = neighAddr,
        type = RTM_DELNEIGH,
        nudState = nudState
)

private fun makeNeighborMessage(
    neighAddr: InetAddress,
    type: Short,
    nudState: Short
) = HexEncoding.decode(
    /* ktlint-disable indent */
    // -- struct nlmsghdr --
                         // length = 88 or 76:
    (if (neighAddr is Inet6Address) "58000000" else "4c000000") +
    type.toLEHex() +     // type
    "0000" +             // flags
    "00000000" +         // seqno
    "00000000" +         // pid (0 == kernel)
    // struct ndmsg
                         // family (AF_INET6 or AF_INET)
    (if (neighAddr is Inet6Address) "0a" else "02") +
    "00" +               // pad1
    "0000" +             // pad2
    "15000000" +         // interface index (21 == wlan0, on test device)
    nudState.toLEHex() + // NUD state
    "00" +               // flags
    "01" +               // type
    // -- struct nlattr: NDA_DST --
                         // length = 20 or 8:
    (if (neighAddr is Inet6Address) "1400" else "0800") +
    "0100" +             // type (1 == NDA_DST, for neighbor messages)
                         // IP address:
    encodeToString(neighAddr.address) +
    // -- struct nlattr: NDA_LLADDR --
    "0a00" +             // length = 10
    "0200" +             // type (2 == NDA_LLADDR, for neighbor messages)
    "00005e000164" +     // MAC Address (== 00:00:5e:00:01:64)
    "0000" +             // padding, for 4 byte alignment
    // -- struct nlattr: NDA_PROBES --
    "0800" +             // length = 8
    "0400" +             // type (4 == NDA_PROBES, for neighbor messages)
    "01000000" +         // number of probes
    // -- struct nlattr: NDA_CACHEINFO --
    "1400" +             // length = 20
    "0300" +             // type (3 == NDA_CACHEINFO, for neighbor messages)
    "05190000" +         // ndm_used, as "clock ticks ago"
    "05190000" +         // ndm_confirmed, as "clock ticks ago"
    "190d0000" +         // ndm_updated, as "clock ticks ago"
    "00000000",          // ndm_refcnt
    false /* allowSingleChar */)
    /* ktlint-enable indent */

/**
 * Convert a [Short] to a little-endian hex string.
 */
private fun Short.toLEHex() = String.format("%04x", java.lang.Short.reverseBytes(this))
