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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.List;

/** Unit tests for {@link ClusterCommand}. */
@RunWith(JUnit4.class)
public class ClusterCommandTest {
    private static final String REQUEST_ID = "request_id";
    private static final String COMMAND_ID = "command_id";
    private static final String TASK_ID = "task_id";
    private static final String COMMAND_LINE = "command_line";

    @Test
    public void testFromJson_withAssignedAttemptId() throws JSONException {
        JSONObject json = createCommandJson().put("attempt_id", "attempt_id");

        ClusterCommand command = ClusterCommand.fromJson(json);

        assertEquals(REQUEST_ID, command.getRequestId());
        assertEquals(COMMAND_ID, command.getCommandId());
        assertEquals(TASK_ID, command.getTaskId());
        assertEquals(COMMAND_LINE, command.getCommandLine());
        assertEquals("attempt_id", command.getAttemptId());
    }

    @Test
    public void testFromJson_withoutAssignedAttemptId() throws JSONException {
        JSONObject json = createCommandJson();

        ClusterCommand command = ClusterCommand.fromJson(json);

        assertEquals(REQUEST_ID, command.getRequestId());
        assertEquals(COMMAND_ID, command.getCommandId());
        assertEquals(TASK_ID, command.getTaskId());
        assertEquals(COMMAND_LINE, command.getCommandLine());
        String UUIDPattern =
                "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$";
        assertTrue(command.getAttemptId().matches(UUIDPattern));
    }

    @Test
    public void testFromJson_extraOptions() throws JSONException {
        JSONArray values = new JSONArray().put("hello").put("world");
        JSONObject option = new JSONObject().put("key", "key").put("values", values);
        JSONObject json = createCommandJson().put("extra_options", new JSONArray().put(option));

        ClusterCommand command = ClusterCommand.fromJson(json);

        assertEquals(REQUEST_ID, command.getRequestId());
        assertEquals(COMMAND_ID, command.getCommandId());
        assertEquals(TASK_ID, command.getTaskId());
        assertEquals(COMMAND_LINE, command.getCommandLine());

        assertEquals(1, command.getExtraOptions().size());
        assertEquals(List.of("hello", "world"), command.getExtraOptions().get("key"));
    }

    /** Helper method to create a JSON command object with required fields. */
    private static JSONObject createCommandJson() throws JSONException {
        return new JSONObject()
                .put("request_id", REQUEST_ID)
                .put("command_id", COMMAND_ID)
                .put("task_id", TASK_ID)
                .put("command_line", COMMAND_LINE);
    }
}
