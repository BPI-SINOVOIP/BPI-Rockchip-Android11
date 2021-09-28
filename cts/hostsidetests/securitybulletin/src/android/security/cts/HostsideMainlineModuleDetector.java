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

package android.security.cts;

import com.android.ddmlib.Log;

import com.google.common.collect.ImmutableSet;

import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class HostsideMainlineModuleDetector {
    private static final String LOG_TAG = "MainlineModuleDetector";

    private SecurityTestCase context;

    private static ImmutableSet<String> playManagedModules;

    HostsideMainlineModuleDetector(SecurityTestCase context) {
        this.context = context;
    }

    synchronized Set<String> getPlayManagedModules() throws Exception {
        if (playManagedModules == null) {
            AdbUtils.runCommandLine("logcat -c", context.getDevice());
            String output = AdbUtils.runCommandLine(
                    "am start com.android.cts.mainlinemoduledetector/.MainlineModuleDetector",
                    context.getDevice());
            Log.logAndDisplay(Log.LogLevel.INFO, LOG_TAG,
                    "am output: " + output);
            Thread.sleep(5 * 1000L);
            String logcat = AdbUtils.runCommandLine("logcat -d -s MainlineModuleDetector:I",
                    context.getDevice());
            Log.logAndDisplay(Log.LogLevel.INFO, LOG_TAG,
                    "Found logcat output: " + logcat);
            Matcher matcher = Pattern.compile("Play managed modules are: <(.*?)>").matcher(logcat);
            if (matcher.find()) {
                playManagedModules = ImmutableSet.copyOf(matcher.group(1).split(","));
            } else {
                playManagedModules = ImmutableSet.of();
            }
        }
        return playManagedModules;
    }
}
