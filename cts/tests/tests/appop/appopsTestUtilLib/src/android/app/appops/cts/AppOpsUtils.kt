/*
 * Copyright (C) 2018 Google Inc.
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

package android.app.appops.cts

import android.app.AppOpsManager
import android.app.AppOpsManager.MODE_ALLOWED
import android.app.AppOpsManager.MODE_DEFAULT
import android.app.AppOpsManager.MODE_ERRORED
import android.app.AppOpsManager.MODE_IGNORED
import android.app.AppOpsManager.OpEntry
import android.util.Log
import androidx.test.InstrumentationRegistry
import com.android.compatibility.common.util.SystemUtil

private const val LOG_TAG = "AppOpsUtils"
private const val TIMEOUT_MILLIS = 10000L

const val TEST_ATTRIBUTION_TAG = "testAttribution"

/**
 * Resets a package's app ops configuration to the device default. See AppOpsManager for the
 * default op settings.
 *
 * <p>
 * It's recommended to call this in setUp() and tearDown() of your test so the test starts and
 * ends with a reproducible default state, and so doesn't affect other tests.
 *
 * <p>
 * Some app ops are configured to be non-resettable, which means that the state of these will
 * not be reset even when calling this method.
 */
fun reset(packageName: String): String {
    return runCommand("appops reset $packageName")
}

/**
 * Sets the app op mode (e.g. allowed, denied) for a single package and operation.
 */
fun setOpMode(packageName: String, opStr: String, mode: Int): String {
    val modeStr = when (mode) {
        MODE_ALLOWED -> "allow"
        MODE_ERRORED -> "deny"
        MODE_IGNORED -> "ignore"
        MODE_DEFAULT -> "default"
        else -> throw IllegalArgumentException("Unexpected app op type")
    }
    val command = "appops set $packageName $opStr $modeStr"
    return runCommand(command)
}

/**
 * Get the app op mode (e.g. MODE_ALLOWED, MODE_DEFAULT) for a single package and operation.
 */
fun getOpMode(packageName: String, opStr: String): Int {
    val opState = getOpState(packageName, opStr)
    return when {
        opState.contains(" allow") -> MODE_ALLOWED
        opState.contains(" deny") -> MODE_ERRORED
        opState.contains(" ignore") -> MODE_IGNORED
        opState.contains(" default") -> MODE_DEFAULT
        else -> throw IllegalStateException("Unexpected app op mode returned $opState")
    }
}

/**
 * Returns whether an allowed operation has been logged by the AppOpsManager for a
 * package. Operations are noted when the app attempts to perform them and calls e.g.
 * {@link AppOpsManager#noteOperation}.
 *
 * @param opStr The public string constant of the operation (e.g. OPSTR_READ_SMS).
 */
fun allowedOperationLogged(packageName: String, opStr: String): Boolean {
    return getOpState(packageName, opStr).contains(" time=")
}

/**
 * Returns whether a rejected operation has been logged by the AppOpsManager for a
 * package. Operations are noted when the app attempts to perform them and calls e.g.
 * {@link AppOpsManager#noteOperation}.
 *
 * @param opStr The public string constant of the operation (e.g. OPSTR_READ_SMS).
 */
fun rejectedOperationLogged(packageName: String, opStr: String): Boolean {
    return getOpState(packageName, opStr).contains(" rejectTime=")
}

/**
 * Runs a lambda adopting Shell's permissions.
 */
inline fun <T> runWithShellPermissionIdentity(runnable: () -> T): T {
    val uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation()
    uiAutomation.adoptShellPermissionIdentity()
    try {
        return runnable.invoke()
    } finally {
        uiAutomation.dropShellPermissionIdentity()
    }
}

/**
 * Returns the app op state for a package. Includes information on when the operation
 * was last attempted to be performed by the package.
 *
 * Format: "SEND_SMS: allow; time=+23h12m54s980ms ago; rejectTime=+1h10m23s180ms"
 */
private fun getOpState(packageName: String, opStr: String): String {
    return runCommand("appops get $packageName $opStr")
}

/**
 * Run a shell command.
 *
 * @param command Command to run
 *
 * @return The output of the command
 */
fun runCommand(command: String): String {
    return SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(), command)
}
/**
 * Make sure that a lambda eventually finishes without throwing an exception.
 *
 * @param r The lambda to run.
 * @param timeout the maximum time to wait
 *
 * @return the return value from the lambda
 *
 * @throws NullPointerException If the return value never becomes non-null
 */
fun <T> eventually(timeout: Long = TIMEOUT_MILLIS, r: () -> T): T {
    val start = System.currentTimeMillis()

    while (true) {
        try {
            return r()
        } catch (e: Throwable) {
            val elapsed = System.currentTimeMillis() - start

            if (elapsed < timeout) {
                Log.d(LOG_TAG, "Ignoring exception", e)

                Thread.sleep(minOf(100, timeout - elapsed))
            } else {
                throw e
            }
        }
    }
}

/**
 * The the {@link AppOpsManager$OpEntry} for the package name and op
 *
 * @param uid UID of the package
 * @param packageName name of the package
 * @param op name of the op
 *
 * @return The entry for the op
 */
fun getOpEntry(uid: Int, packageName: String, op: String): OpEntry? {
    return SystemUtil.callWithShellPermissionIdentity {
        InstrumentationRegistry.getInstrumentation().targetContext
                .getSystemService(AppOpsManager::class.java).getOpsForPackage(uid, packageName, op)
    }[0].ops[0]
}