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
package com.android.tradefed.cluster;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link ClusterCommandEvent}. */
@RunWith(JUnit4.class)
public class ClusterCommandEventTest {

    @Test
    public void testToJSON() throws Exception {
        Set<String> device_serials = new HashSet<>();
        device_serials.add("s1");
        ClusterCommandEvent event =
                new ClusterCommandEvent.Builder()
                        .setType(ClusterCommandEvent.Type.InvocationStarted)
                        .setAttemptId("attempt_id")
                        .setCommandTaskId("task_id")
                        .setDeviceSerials(device_serials)
                        .setHostName("host")
                        .setTimestamp(100000)
                        .build();
        JSONObject obj = event.toJSON();
        Assert.assertEquals("InvocationStarted", obj.getString("type"));
        Assert.assertEquals("task_id", obj.getString("task_id"));
        Assert.assertEquals("attempt_id", obj.getString("attempt_id"));
        Assert.assertEquals("host", obj.getString("hostname"));
        Assert.assertEquals(100, obj.getLong("time"));
        Assert.assertEquals("s1", obj.getString("device_serial"));
        Assert.assertEquals(1, obj.getJSONArray("device_serials").length());
        Assert.assertEquals("s1", obj.getJSONArray("device_serials").getString(0));
    }

    @Test
    public void testToJSON_noDeviceSerial() throws Exception {
        ClusterCommandEvent event =
                new ClusterCommandEvent.Builder()
                        .setType(ClusterCommandEvent.Type.InvocationStarted)
                        .setAttemptId("attempt_id")
                        .setCommandTaskId("task_id")
                        .setHostName("host")
                        .setTimestamp(100000)
                        .build();
        JSONObject obj = event.toJSON();
        Assert.assertEquals("InvocationStarted", obj.getString("type"));
        Assert.assertEquals("task_id", obj.getString("task_id"));
        Assert.assertEquals("attempt_id", obj.getString("attempt_id"));
        Assert.assertEquals("host", obj.getString("hostname"));
        Assert.assertEquals(100, obj.getLong("time"));
        Assert.assertFalse(obj.has("device_serial"));
        Assert.assertEquals(0, obj.getJSONArray("device_serials").length());
    }
}
