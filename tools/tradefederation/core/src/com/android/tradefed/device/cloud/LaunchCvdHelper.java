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
package com.android.tradefed.device.cloud;

import java.util.ArrayList;
import java.util.List;

/** Utility helper to control Launch_cvd in the Cuttlefish VM. */
public class LaunchCvdHelper {

    /**
     * Create the command line to start an additional device for a user.
     *
     * @param username The user that will run the device.
     * @param daemon Whether or not to start the device as a daemon.
     * @return The created command line;
     */
    public static List<String> createSimpleDeviceCommand(String username, boolean daemon) {
        return createSimpleDeviceCommand(username, daemon, true, true);
    }

    /**
     * Create the command line to start an additional device for a user.
     *
     * @param username The user that will run the device.
     * @param daemon Whether or not to start the device as a daemon.
     * @param alwaysCreateUserData Whether or not to create the userdata partition
     * @param blankDataImage whether or not to create the data image.
     * @return The created command line;
     */
    public static List<String> createSimpleDeviceCommand(
            String username, boolean daemon, boolean alwaysCreateUserData, boolean blankDataImage) {
        List<String> command = new ArrayList<>();
        command.add("sudo -u " + username);
        command.add("/home/" + username + "/bin/launch_cvd");
        command.add("-data_policy");
        if (alwaysCreateUserData) {
            command.add("always_create");
        } else {
            command.add("create_if_missing");
        }
        if (blankDataImage) {
            command.add("-blank_data_image_mb");
        }
        command.add("8000");
        if (daemon) {
            command.add("-daemon");
        }
        return command;
    }
}
