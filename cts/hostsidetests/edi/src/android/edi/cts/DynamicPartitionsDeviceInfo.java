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
package android.edi.cts;

import com.android.compatibility.common.util.DeviceInfo;
import com.android.compatibility.common.util.HostInfoStore;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import java.io.File;
import java.io.FileWriter;
import org.json.JSONException;
import org.json.JSONObject;

public class DynamicPartitionsDeviceInfo extends DeviceInfo {

    @Override
    protected void collectDeviceInfo(HostInfoStore store) throws Exception {
        throw new UnsupportedOperationException();
    }

    @Override
    protected void collectDeviceInfo(File jsonFile) throws Exception {
        ITestDevice device = getDevice();

        CommandResult commandResult = device.executeShellV2Command(
                "lpdump --json");
        if (commandResult.getExitCode() == null) {
            CLog.e("lpdump exit code is null");
            return;
        }
        if (commandResult.getExitCode() != 0) {
            CLog.e("lpdump returns %d: %s", commandResult.getExitCode(),
                   commandResult.getStderr());
            return;
        }

        if (commandResult.getExitCode() == 0 && !commandResult.getStderr().isEmpty()) {
            CLog.w("Warnings occur when running lpdump --json:\n%s",
                   commandResult.getStderr());
        }

        String output = commandResult.getStdout();
        if (output == null) output = "";
        output = output.trim();
        if (output.isEmpty()) {
            CLog.e("lpdump --json does not generate anything");
            return;
        }

        if (!isJsonString(output)) {
            CLog.e("lpdump --json does not generate a valid JSON string: %s", output);
            return;
        }

        try (FileWriter writer = new FileWriter(jsonFile)) {
            writer.write(output);
        }
    }

    private static boolean isJsonString(String s) {
        try {
            new JSONObject(s);
            return true;
        } catch (JSONException e) {
            return false;
        }
    }
}
