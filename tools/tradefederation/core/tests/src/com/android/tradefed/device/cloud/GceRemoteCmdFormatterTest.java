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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.*;

import com.android.tradefed.device.cloud.GceRemoteCmdFormatter.ScpMode;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.List;

/** Unit tests for {@link GceRemoteCmdFormatter} */
@RunWith(JUnit4.class)
public class GceRemoteCmdFormatterTest {

    @Test
    public void testFormatSsh() {
        List<String> res =
                GceRemoteCmdFormatter.getSshCommand(
                        new File("/tmp/key"), null, "root", "127.0.0.1", "stop", "adbd");
        assertEquals("ssh", res.get(0));
        assertEquals("-o", res.get(1));
        assertEquals("UserKnownHostsFile=/dev/null", res.get(2));
        assertEquals("-o", res.get(3));
        assertEquals("StrictHostKeyChecking=no", res.get(4));
        assertEquals("-o", res.get(5));
        assertEquals("ServerAliveInterval=10", res.get(6));
        assertEquals("-i", res.get(7));
        assertEquals("/tmp/key", res.get(8));
        assertEquals("root@127.0.0.1", res.get(9));
        assertEquals("stop", res.get(10));
        assertEquals("adbd", res.get(11));
    }

    @Test
    public void testFormatScp() {
        List<String> res =
                GceRemoteCmdFormatter.getScpCommand(
                        new File("/tmp/key"),
                        null,
                        "root",
                        "127.0.0.1",
                        "/sdcard/test",
                        "/tmp/here",
                        ScpMode.PULL);
        assertEquals("scp", res.get(0));
        assertEquals("-o", res.get(1));
        assertEquals("UserKnownHostsFile=/dev/null", res.get(2));
        assertEquals("-o", res.get(3));
        assertEquals("StrictHostKeyChecking=no", res.get(4));
        assertEquals("-o", res.get(5));
        assertEquals("ServerAliveInterval=10", res.get(6));
        assertEquals("-i", res.get(7));
        assertEquals("/tmp/key", res.get(8));
        assertEquals("root@127.0.0.1:/sdcard/test", res.get(9));
        assertEquals("/tmp/here", res.get(10));
    }
}
