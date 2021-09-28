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

package android.app.cts;

import android.app.Instrumentation;
import android.app.KeyguardManager;
import android.content.Context;
import android.os.PowerManager;
import android.util.Log;

import com.android.compatibility.common.util.CommonTestUtils;
import com.android.compatibility.common.util.SystemUtil;

class CtsAppTestUtils {
    private static final String TAG = CtsAppTestUtils.class.getName();

    public static String executeShellCmd(Instrumentation instrumentation, String cmd)
            throws Exception {
        final String result = SystemUtil.runShellCommand(instrumentation, cmd);
        Log.d(TAG, String.format("Output for '%s': %s", cmd, result));
        return result;
    }

    public static boolean isScreenInteractive(Context context) {
        final PowerManager powerManager =
                (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return powerManager.isInteractive();
    }

    public static boolean isKeyguardLocked(Context context) {
        final KeyguardManager keyguardManager =
                (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        return keyguardManager.isKeyguardLocked();
    }

    public static void turnScreenOn(Instrumentation instrumentation, Context context)
            throws Exception {
        executeShellCmd(instrumentation, "input keyevent KEYCODE_WAKEUP");
        executeShellCmd(instrumentation, "wm dismiss-keyguard");
        CommonTestUtils.waitUntil("Device does not wake up after 5 seconds", 5,
                () ->  {
                    return isScreenInteractive(context)
                            && !isKeyguardLocked(context);
                });
    }

    public static String makeUidIdle(Instrumentation instrumentation, String packageName)
            throws Exception {
        // Force app to go idle now
        String cmd = "am make-uid-idle " + packageName;
        return executeShellCmd(instrumentation, cmd);
    }
}
