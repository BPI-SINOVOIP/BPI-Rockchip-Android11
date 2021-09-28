/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.test;

import com.android.game.qualification.GameCoreConfiguration;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.INativeDevice;
import com.android.tradefed.util.CommandResult;

import static org.junit.Assume.assumeTrue;

public class TestUtils {

    public static boolean isGameCoreCertified(INativeDevice device)
            throws DeviceNotAvailableException {
        CommandResult result = device.executeShellV2Command(
                "pm has-feature " + GameCoreConfiguration.FEATURE_STRING);
        return result.getExitCode() == 0;
    }

    public static void assumeGameCoreCertified(INativeDevice device)
            throws DeviceNotAvailableException {
        assumeTrue(isGameCoreCertified(device));
    }
}
