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

package com.android.compatibility.common.util;

import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.ITestDevice;

import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;

/** Utility class for proto dumps. */
public class ProtoUtils {
    /**
     * Command to get a JobScheduler proto dump. Can be used as input to {@link
     * #getProto(ITestDevice, Parser, String)}.
     */
    public static final String DUMPSYS_JOB_SCHEDULER = "dumpsys jobscheduler --proto";

    /**
     * Call onto the device with an adb shell command and get the results of that as a proto of the
     * given type.
     *
     * @param device The test device to run a command on.
     * @param parser A protobuf parser object. e.g. MyProto.parser()
     * @param command The adb shell command to run. e.g. "dumpsys fingerprint --proto"
     */
    public static <T extends MessageLite> T getProto(
            ITestDevice device, Parser<T> parser, String command) throws Exception {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        device.executeShellCommand(command, receiver);
        return parser.parseFrom(receiver.getOutput());
    }
}
