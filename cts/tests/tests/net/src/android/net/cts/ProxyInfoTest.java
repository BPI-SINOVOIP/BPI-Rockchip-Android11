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

package android.net.cts;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.net.ProxyInfo;
import android.net.Uri;
import android.os.Build;

import androidx.test.runner.AndroidJUnit4;

import com.android.testutils.DevSdkIgnoreRule;
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

@RunWith(AndroidJUnit4.class)
public final class ProxyInfoTest {
    private static final String TEST_HOST = "test.example.com";
    private static final int TEST_PORT = 5566;
    private static final Uri TEST_URI = Uri.parse("https://test.example.com");
    // This matches android.net.ProxyInfo#LOCAL_EXCL_LIST
    private static final String LOCAL_EXCL_LIST = "";
    // This matches android.net.ProxyInfo#LOCAL_HOST
    private static final String LOCAL_HOST = "localhost";
    // This matches android.net.ProxyInfo#LOCAL_PORT
    private static final int LOCAL_PORT = -1;

    @Rule
    public final DevSdkIgnoreRule ignoreRule = new DevSdkIgnoreRule();

    @Test
    public void testConstructor() {
        final ProxyInfo proxy = new ProxyInfo((ProxyInfo) null);
        checkEmpty(proxy);

        assertEquals(proxy, new ProxyInfo(proxy));
    }

    @Test
    public void testBuildDirectProxy() {
        final ProxyInfo proxy1 = ProxyInfo.buildDirectProxy(TEST_HOST, TEST_PORT);

        assertEquals(TEST_HOST, proxy1.getHost());
        assertEquals(TEST_PORT, proxy1.getPort());
        assertArrayEquals(new String[0], proxy1.getExclusionList());
        assertEquals(Uri.EMPTY, proxy1.getPacFileUrl());

        final List<String> exclList = new ArrayList<>();
        exclList.add("localhost");
        exclList.add("*.exclusion.com");
        final ProxyInfo proxy2 = ProxyInfo.buildDirectProxy(TEST_HOST, TEST_PORT, exclList);

        assertEquals(TEST_HOST, proxy2.getHost());
        assertEquals(TEST_PORT, proxy2.getPort());
        assertArrayEquals(exclList.toArray(new String[0]), proxy2.getExclusionList());
        assertEquals(Uri.EMPTY, proxy2.getPacFileUrl());
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testBuildPacProxy() {
        final ProxyInfo proxy1 = ProxyInfo.buildPacProxy(TEST_URI);

        assertEquals(LOCAL_HOST, proxy1.getHost());
        assertEquals(LOCAL_PORT, proxy1.getPort());
        assertArrayEquals(LOCAL_EXCL_LIST.toLowerCase(Locale.ROOT).split(","),
                proxy1.getExclusionList());
        assertEquals(TEST_URI, proxy1.getPacFileUrl());

        final ProxyInfo proxy2 = ProxyInfo.buildPacProxy(TEST_URI, TEST_PORT);

        assertEquals(LOCAL_HOST, proxy2.getHost());
        assertEquals(TEST_PORT, proxy2.getPort());
        assertArrayEquals(LOCAL_EXCL_LIST.toLowerCase(Locale.ROOT).split(","),
                proxy2.getExclusionList());
        assertEquals(TEST_URI, proxy2.getPacFileUrl());
    }

    @Test
    public void testIsValid() {
        final ProxyInfo proxy1 = ProxyInfo.buildDirectProxy(TEST_HOST, TEST_PORT);
        assertTrue(proxy1.isValid());

        // Given empty host
        final ProxyInfo proxy2 = ProxyInfo.buildDirectProxy("", TEST_PORT);
        assertFalse(proxy2.isValid());
        // Given invalid host
        final ProxyInfo proxy3 = ProxyInfo.buildDirectProxy(".invalid.com", TEST_PORT);
        assertFalse(proxy3.isValid());
        // Given invalid port.
        final ProxyInfo proxy4 = ProxyInfo.buildDirectProxy(TEST_HOST, 0);
        assertFalse(proxy4.isValid());
        // Given another invalid port
        final ProxyInfo proxy5 = ProxyInfo.buildDirectProxy(TEST_HOST, 65536);
        assertFalse(proxy5.isValid());
        // Given invalid exclusion list
        final List<String> exclList = new ArrayList<>();
        exclList.add(".invalid.com");
        exclList.add("%.test.net");
        final ProxyInfo proxy6 = ProxyInfo.buildDirectProxy(TEST_HOST, TEST_PORT, exclList);
        assertFalse(proxy6.isValid());
    }

    private void checkEmpty(ProxyInfo proxy) {
        assertNull(proxy.getHost());
        assertEquals(0, proxy.getPort());
        assertNull(proxy.getExclusionList());
        assertEquals(Uri.EMPTY, proxy.getPacFileUrl());
    }
}
