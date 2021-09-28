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

package android.net.ipsec.ike;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;

import android.net.InetAddresses;
import android.net.Network;

import org.junit.Test;

import java.net.Inet4Address;

public final class IkeSessionConnectionInfoTest {
    private static final Inet4Address LOCAL_IPV4_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.200"));
    private static final Inet4Address REMOTE_IPV4_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.100"));

    private final Network mMockNetwork;

    public IkeSessionConnectionInfoTest() {
        mMockNetwork = mock(Network.class);
    }

    @Test
    public void testBuild() {
        IkeSessionConnectionInfo ikeConnInfo =
                new IkeSessionConnectionInfo(LOCAL_IPV4_ADDRESS, REMOTE_IPV4_ADDRESS, mMockNetwork);

        assertEquals(LOCAL_IPV4_ADDRESS, ikeConnInfo.getLocalAddress());
        assertEquals(REMOTE_IPV4_ADDRESS, ikeConnInfo.getRemoteAddress());
        assertEquals(mMockNetwork, ikeConnInfo.getNetwork());
    }

    @Test
    public void testBuildWithNullValue() {
        try {
            new IkeSessionConnectionInfo(null /*localAddress*/, REMOTE_IPV4_ADDRESS, mMockNetwork);
            fail("Expected to fail due to null value localAddress");
        } catch (NullPointerException expected) {

        }
    }
}
