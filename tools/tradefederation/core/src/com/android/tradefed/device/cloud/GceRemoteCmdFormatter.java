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

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Utility class to format commands to reach a remote gce device. */
public class GceRemoteCmdFormatter {

    /**
     * SCP can be used to push or pull file depending of the structure of the args. Ensure we can
     * support both.
     */
    public enum ScpMode {
        PUSH,
        PULL;
    }

    /**
     * Utility to create a ssh command for a gce device based on some parameters.
     *
     * @param sshKey the ssh key {@link File}.
     * @param extraOptions a List of {@link String} that can be added for extra ssh options. can be
     *     null.
     * @param hostName the hostname where to connect to the gce device.
     * @param command the actual command to run on the gce device.
     * @return a list representing the ssh command for a gce device.
     */
    public static List<String> getSshCommand(
            File sshKey,
            List<String> extraOptions,
            String user,
            String hostName,
            String... command) {
        List<String> cmd = new ArrayList<>();
        cmd.add("ssh");
        cmd.add("-o");
        cmd.add("UserKnownHostsFile=/dev/null");
        cmd.add("-o");
        cmd.add("StrictHostKeyChecking=no");
        cmd.add("-o");
        cmd.add("ServerAliveInterval=10");
        cmd.add("-i");
        cmd.add(sshKey.getAbsolutePath());
        if (extraOptions != null) {
            for (String op : extraOptions) {
                cmd.add(op);
            }
        }
        cmd.add(user + "@" + hostName);
        for (String cmdOption : command) {
            cmd.add(cmdOption);
        }
        return cmd;
    }

    /**
     * Utility to create a scp command to fetch a file from a remote gce device.
     *
     * @param sshKey the ssh key {@link File}.
     * @param extraOptions a List of {@link String} that can be added for extra ssh options. can be
     *     null.
     * @param hostName the hostname where to connect to the gce device.
     * @param remoteFile the file to be fetched on the remote gce device.
     * @param localFile the local file where to put the remote file.
     * @param mode whether we are pushing the local file to the remote or pulling the remote
     * @return a list representing the scp command for a gce device.
     */
    public static List<String> getScpCommand(
            File sshKey,
            List<String> extraOptions,
            String user,
            String hostName,
            String remoteFile,
            String localFile,
            ScpMode mode) {
        List<String> cmd = new ArrayList<>();
        cmd.add("scp");
        cmd.add("-o");
        cmd.add("UserKnownHostsFile=/dev/null");
        cmd.add("-o");
        cmd.add("StrictHostKeyChecking=no");
        cmd.add("-o");
        cmd.add("ServerAliveInterval=10");
        cmd.add("-i");
        cmd.add(sshKey.getAbsolutePath());
        if (extraOptions != null) {
            for (String op : extraOptions) {
                cmd.add(op);
            }
        }
        if (ScpMode.PULL.equals(mode)) {
            cmd.add(String.format("%s@%s:%s", user, hostName, remoteFile));
            cmd.add(localFile);
        } else {
            cmd.add(localFile);
            cmd.add(String.format("%s@%s:%s", user, hostName, remoteFile));
        }
        return cmd;
    }
}
