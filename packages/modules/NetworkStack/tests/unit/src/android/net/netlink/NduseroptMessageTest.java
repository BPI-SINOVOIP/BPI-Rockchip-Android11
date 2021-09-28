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

package android.net.netlink;

import static android.net.InetAddresses.parseNumericAddress;
import static android.system.OsConstants.AF_INET6;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.net.IpPrefix;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import libcore.util.HexEncoding;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class NduseroptMessageTest {

    private static final byte ICMP_TYPE_RA = (byte) 134;

    private static final int IFINDEX1 = 15715755;
    private static final int IFINDEX2 = 1431655765;

    // IPv6, 0 bytes of options, interface index 15715755, type 134 (RA), code 0, padding.
    private static final String HDR_EMPTY = "0a00" + "0000" + "abcdef00" + "8600000000000000";

    // IPv6, 16 bytes of options, interface index 1431655765, type 134 (RA), code 0, padding.
    private static final String HDR_16BYTE = "0a00" + "1000" + "55555555" + "8600000000000000";

    // IPv6, 32 bytes of options, interface index 1431655765, type 134 (RA), code 0, padding.
    private static final String HDR_32BYTE = "0a00" + "2000" + "55555555" + "8600000000000000";

    // PREF64 option, 2001:db8:3:4:5:6::/96, lifetime=10064
    private static final String OPT_PREF64 = "2602" + "2750" + "20010db80003000400050006";

    // Length 20, NDUSEROPT_SRCADDR, fe80:2:3:4:5:6:7:8
    private static final String NLA_SRCADDR = "1400" + "0100" + "fe800002000300040005000600070008";

    private static final InetAddress SADDR1 = parseNumericAddress("fe80:2:3:4:5:6:7:8%" + IFINDEX1);
    private static final InetAddress SADDR2 = parseNumericAddress("fe80:2:3:4:5:6:7:8%" + IFINDEX2);

    private static final String MSG_EMPTY = HDR_EMPTY + NLA_SRCADDR;
    private static final String MSG_PREF64 = HDR_16BYTE + OPT_PREF64 + NLA_SRCADDR;

    @Test
    public void testParsing() {
        NduseroptMessage msg = parseNduseroptMessage(toBuffer(MSG_EMPTY));
        assertMatches(AF_INET6, 0, IFINDEX1, ICMP_TYPE_RA, (byte) 0, SADDR1, msg);
        assertNull(msg.option);

        msg = parseNduseroptMessage(toBuffer(MSG_PREF64));
        assertMatches(AF_INET6, 16, IFINDEX2, ICMP_TYPE_RA, (byte) 0, SADDR2, msg);
        assertPref64Option("2001:db8:3:4:5:6::/96", msg.option);
    }

    @Test
    public void testParseWithinNetlinkMessage() throws Exception {
        // A NduseroptMessage inside a netlink message. Ensure that it parses the same way both by
        // parsing the netlink message via NetlinkMessage.parse() and by parsing the option itself
        // with NduseroptMessage.parse().
        final String hexBytes =
                "44000000440000000000000000000000"             // len=68, RTM_NEWNDUSEROPT
                + "0A0010001E0000008600000000000000"           // IPv6, opt_bytes=16, ifindex=30, RA
                + "260202580064FF9B0000000000000000"           // pref64, prefix=64:ff9b::/96, 600
                + "14000100FE800000000000000250B6FFFEB7C499";  // srcaddr=fe80::250:b6ff:feb7:c499

        ByteBuffer buf = toBuffer(hexBytes);
        assertEquals(68, buf.limit());
        buf.order(ByteOrder.nativeOrder());

        NetlinkMessage nlMsg = NetlinkMessage.parse(buf);
        assertNotNull(nlMsg);
        assertTrue(nlMsg instanceof NduseroptMessage);

        NduseroptMessage msg = (NduseroptMessage) nlMsg;
        InetAddress srcaddr = InetAddress.getByName("fe80::250:b6ff:feb7:c499%30");
        assertMatches(AF_INET6, 16, 30, ICMP_TYPE_RA, (byte) 0, srcaddr, msg);
        assertPref64Option("64:ff9b::/96", msg.option);

        final String hexBytesWithoutHeader = hexBytes.substring(StructNlMsgHdr.STRUCT_SIZE * 2);
        ByteBuffer bufWithoutHeader = toBuffer(hexBytesWithoutHeader);
        assertEquals(52, bufWithoutHeader.limit());
        msg = parseNduseroptMessage(bufWithoutHeader);
        assertMatches(AF_INET6, 16, 30, ICMP_TYPE_RA, (byte) 0, srcaddr, msg);
        assertPref64Option("64:ff9b::/96", msg.option);
    }

    @Test
    public void testParseUnknownOptionWithinNetlinkMessage() throws Exception {
        final String hexBytes =
                "4C0000004400000000000000000000000"
                + "A0018001E0000008600000000000000"
                + "1903000000001770FD123456789000000000000000000001"  // RDNSS option
                + "14000100FE800000000000000250B6FFFEB7C499";

        ByteBuffer buf = toBuffer(hexBytes);
        assertEquals(76, buf.limit());
        buf.order(ByteOrder.nativeOrder());

        NetlinkMessage nlMsg = NetlinkMessage.parse(buf);
        assertNotNull(nlMsg);
        assertTrue(nlMsg instanceof NduseroptMessage);

        NduseroptMessage msg = (NduseroptMessage) nlMsg;
        InetAddress srcaddr = InetAddress.getByName("fe80::250:b6ff:feb7:c499%30");
        assertMatches(AF_INET6, 24, 30, ICMP_TYPE_RA, (byte) 0, srcaddr, msg);
        assertEquals(NdOption.UNKNOWN, msg.option);
    }

    @Test
    public void testUnknownOption() {
        ByteBuffer buf = toBuffer(MSG_PREF64);
        // Replace the PREF64 option type (38) with an unknown option number.
        final int optionStart = NduseroptMessage.STRUCT_SIZE;
        assertEquals(38, buf.get(optionStart));
        buf.put(optionStart, (byte) 42);

        NduseroptMessage msg = parseNduseroptMessage(buf);
        assertMatches(AF_INET6, 16, IFINDEX2, ICMP_TYPE_RA, (byte) 0, SADDR2, msg);
        assertEquals(NdOption.UNKNOWN, msg.option);

        buf.flip();
        assertEquals(42, buf.get(optionStart));
        buf.put(optionStart, (byte) 38);

        msg = parseNduseroptMessage(buf);
        assertMatches(AF_INET6, 16, IFINDEX2, ICMP_TYPE_RA, (byte) 0, SADDR2, msg);
        assertPref64Option("2001:db8:3:4:5:6::/96", msg.option);
    }

    @Test
    public void testZeroLengthOption() {
        // Make sure an unknown option with a 0-byte length is ignored and parsing continues with
        // the address, which comes after it.
        final String hexString = HDR_16BYTE + "00000000000000000000000000000000" + NLA_SRCADDR;
        ByteBuffer buf = toBuffer(hexString);
        assertEquals(52, buf.limit());
        NduseroptMessage msg = parseNduseroptMessage(buf);
        assertMatches(AF_INET6, 16, IFINDEX2, ICMP_TYPE_RA, (byte) 0, SADDR2, msg);
        assertNull(msg.option);
    }

    @Test
    public void testTooLongOption() {
        // Make sure that if an option's length is too long, it's ignored and parsing continues with
        // the address, which comes after it.
        final String hexString = HDR_16BYTE + "26030000000000000000000000000000" + NLA_SRCADDR;
        ByteBuffer buf = toBuffer(hexString);
        assertEquals(52, buf.limit());
        NduseroptMessage msg = parseNduseroptMessage(buf);
        assertMatches(AF_INET6, 16, IFINDEX2, ICMP_TYPE_RA, (byte) 0, SADDR2, msg);
        assertNull(msg.option);
    }

    @Test
    public void testOptionsTooLong() {
        // Header claims 32 bytes of options. Buffer ends before options end.
        String hexString = HDR_32BYTE + OPT_PREF64;
        ByteBuffer buf = toBuffer(hexString);
        assertEquals(32, buf.limit());
        assertNull(NduseroptMessage.parse(toBuffer(hexString)));

        // Header claims 32 bytes of options. Buffer ends at end of options with no source address.
        hexString = HDR_32BYTE + OPT_PREF64 + OPT_PREF64;
        buf = toBuffer(hexString);
        assertEquals(48, buf.limit());
        assertNull(NduseroptMessage.parse(toBuffer(hexString)));
    }

    @Test
    public void testTruncation() {
        final int optLen = MSG_PREF64.length() / 2;  // 1 byte = 2 hex chars
        for (int len = 0; len < optLen; len++) {
            ByteBuffer buf = toBuffer(MSG_PREF64.substring(0, len * 2));
            NduseroptMessage msg = parseNduseroptMessage(buf);
            if (len < optLen) {
                assertNull(msg);
            } else {
                assertNotNull(msg);
                assertPref64Option("2001:db8:3:4:5:6::/96", msg.option);
            }
        }
    }

    @Test
    public void testToString() {
        NduseroptMessage msg = parseNduseroptMessage(toBuffer(MSG_PREF64));
        assertNotNull(msg);
        assertEquals("Nduseroptmsg(10, 16, 1431655765, 134, 0, fe80:2:3:4:5:6:7:8%1431655765)",
                msg.toString());
    }

    // Convenience method to parse a NduseroptMessage that's not part of a netlink message.
    private NduseroptMessage parseNduseroptMessage(ByteBuffer buf) {
        return NduseroptMessage.parse(null, buf);
    }

    private ByteBuffer toBuffer(String hexString) {
        return ByteBuffer.wrap(HexEncoding.decode(hexString));
    }

    private void assertMatches(int family, int optsLen, int ifindex, byte icmpType,
            byte icmpCode, InetAddress srcaddr, NduseroptMessage msg) {
        assertNotNull(msg);
        assertEquals(family, msg.family);
        assertEquals(ifindex, msg.ifindex);
        assertEquals(optsLen, msg.opts_len);
        assertEquals(icmpType, msg.icmp_type);
        assertEquals(icmpCode, msg.icmp_code);
        assertEquals(srcaddr, msg.srcaddr);
    }

    private void assertPref64Option(String prefix, NdOption opt) {
        assertNotNull(opt);
        assertTrue(opt instanceof StructNdOptPref64);
        StructNdOptPref64 pref64Opt = (StructNdOptPref64) opt;
        assertEquals(new IpPrefix(prefix), pref64Opt.prefix);
    }
}
