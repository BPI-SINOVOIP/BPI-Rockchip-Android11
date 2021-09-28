/*
 * Copyright (C) 2020 The Android Open Source Project
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

import android.app.Activity
import android.app.AppOpsManager
import android.app.AppOpsManager.KEY_BG_STATE_SETTLE_TIME
import android.app.AppOpsManager.KEY_FG_SERVICE_STATE_SETTLE_TIME
import android.app.AppOpsManager.KEY_TOP_STATE_SETTLE_TIME
import android.app.AppOpsManager.MODE_ALLOWED
import android.app.AppOpsManager.MODE_IGNORED
import android.app.AppOpsManager.OPSTR_FINE_LOCATION
import android.app.AppOpsManager.WATCH_FOREGROUND_CHANGES
import android.content.ComponentName
import android.content.Context
import android.content.Context.BIND_AUTO_CREATE
import android.content.Context.BIND_NOT_FOREGROUND
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.ServiceConnection
import android.os.IBinder
import android.platform.test.annotations.AppModeFull
import android.provider.Settings
import android.provider.Settings.Global.APP_OPS_CONSTANTS
import android.support.test.uiautomator.UiDevice
import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import com.android.compatibility.common.util.SystemUtil.callWithShellPermissionIdentity
import com.google.common.truth.Truth.assertThat
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit.MILLISECONDS

private const val TEST_SERVICE_PKG = "android.app.appops.cts.appthatcanbeforcedintoforegroundstates"
private const val TIMEOUT_MILLIS = 45000L

@AppModeFull(reason = "This test connects to other test app")
class ForegroundModeTest {
    private var previousAppOpsConstants: String? = null

    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val appopsManager = context.getSystemService(AppOpsManager::class.java)!!
    private val testPkgUid = context.packageManager.getPackageUid(TEST_SERVICE_PKG, 0)

    private lateinit var foregroundControlService: IAppOpsForegroundControlService
    private lateinit var serviceConnection: ServiceConnection

    private val testPkgAppOpMode: Int
        get() {
            return callWithShellPermissionIdentity {
                appopsManager.noteOp(OPSTR_FINE_LOCATION, testPkgUid, TEST_SERVICE_PKG,
                        null, null)
            }
        }

    private fun wakeUpScreen() {
        val uiDevice = UiDevice.getInstance(instrumentation)
        uiDevice.wakeUp()
        uiDevice.executeShellCommand("wm dismiss-keyguard")
        uiDevice.executeShellCommand("input keyevent KEYCODE_HOME")
    }

    @Before
    fun setup() {
        Log.i("Test", "uid=$testPkgUid")

        runWithShellPermissionIdentity {
            previousAppOpsConstants = Settings.Global.getString(context.contentResolver,
                    APP_OPS_CONSTANTS)

            // Speed up app-ops service proc state transitions
            Settings.Global.putString(context.contentResolver, APP_OPS_CONSTANTS,
                    "$KEY_TOP_STATE_SETTLE_TIME=300,$KEY_FG_SERVICE_STATE_SETTLE_TIME=100," +
                            "$KEY_BG_STATE_SETTLE_TIME=10")
        }

        // Wait until app counts as background
        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
        }

        val serviceIntent = Intent().setComponent(ComponentName(TEST_SERVICE_PKG,
                "$TEST_SERVICE_PKG.AppOpsForegroundControlService"))

        val newService = CompletableFuture<IAppOpsForegroundControlService>()
        serviceConnection = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                newService.complete(IAppOpsForegroundControlService.Stub.asInterface(service))
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                Assert.fail("foreground control service disconnected")
            }
        }

        context.bindService(serviceIntent, serviceConnection,
                BIND_AUTO_CREATE or BIND_NOT_FOREGROUND)
        foregroundControlService = newService.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    private fun makeTop() {
        wakeUpScreen()

        context.startActivity(Intent().setComponent(
                ComponentName(TEST_SERVICE_PKG,
                        "$TEST_SERVICE_PKG.AppOpsForegroundControlActivity"))
                .setFlags(FLAG_ACTIVITY_NEW_TASK))
        foregroundControlService.waitUntilForeground()

        // Sometimes it can take some time for the lock screen to disappear. Use eval'ed appop mode
        // as a proxy
        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)
        }
    }

    private fun withTopActivity(code: (Activity) -> Unit) {
        wakeUpScreen()

        context.startActivity(Intent(context, UidStateForceActivity::class.java)
                .setFlags(FLAG_ACTIVITY_NEW_TASK))

        UidStateForceActivity.waitForResumed()
        try {
            code(UidStateForceActivity.instance!!)
        } finally {
            UidStateForceActivity.instance?.finish()
        }
    }

    private fun startForegroundService(startingContext: Context) {
        startingContext.startForegroundService(Intent().setComponent(
                ComponentName(TEST_SERVICE_PKG,
                        "$TEST_SERVICE_PKG.AppOpsForegroundControlForegroundService")))
        foregroundControlService.waitUntilForegroundServiceStarted()
    }

    private fun startLocationForegroundService(startingContext: Context) {
        startingContext.startForegroundService(Intent().setComponent(
                ComponentName(TEST_SERVICE_PKG,
                        "$TEST_SERVICE_PKG.AppOpsForegroundControlLocationForegroundService")))
        foregroundControlService.waitUntilLocationForegroundServiceStarted()
    }

    private fun makeBackground() {
        foregroundControlService.finishActivity()
        foregroundControlService.stopForegroundService()
        foregroundControlService.stopLocationForegroundService()
    }

    @Test
    fun modeIsIgnoredWhenAppIsBackground() {
        assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
    }

    @Test
    fun modeIsAllowedWhenForeground() {
        makeTop()
        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)
        }
    }

    @Test
    fun modeBecomesIgnoredAfterEnteringBackground() {
        makeTop()
        assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)

        makeBackground()
        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
        }
    }

    @Test
    fun modeChangeCallbackWhenEnteringForeground() {
        val gotCallback = CompletableFuture<Unit>()
        appopsManager.startWatchingMode(OPSTR_FINE_LOCATION, TEST_SERVICE_PKG,
                WATCH_FOREGROUND_CHANGES) { op, packageName ->
            if (op == OPSTR_FINE_LOCATION && packageName == TEST_SERVICE_PKG) {
                gotCallback.complete(Unit)
            }
        }

        makeTop()
        gotCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    @Test
    fun modeChangeCallbackWhenEnteringBackground() {
        makeTop()

        val gotCallback = CompletableFuture<Unit>()
        appopsManager.startWatchingMode(OPSTR_FINE_LOCATION, TEST_SERVICE_PKG,
                WATCH_FOREGROUND_CHANGES) { op, packageName ->
            if (op == OPSTR_FINE_LOCATION && packageName == TEST_SERVICE_PKG) {
                gotCallback.complete(Unit)
            }
        }

        makeBackground()
        gotCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    @Test
    fun modeIsIgnoredWhenAppHasNonLocationForegroundService() {
        withTopActivity { fgActivity ->
            startForegroundService(fgActivity)
        }
        assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
    }

    @Test
    fun modeIsAllowedWhenAppHasLocationForegroundService() {
        withTopActivity { fgActivity ->
            startLocationForegroundService(fgActivity)
        }

        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)
        }
    }

    @Test
    fun modeBecomesIgnoredAfterLocationForegroundIsStopped() {
        withTopActivity { fgActivity ->
            startLocationForegroundService(fgActivity)
        }

        foregroundControlService.stopLocationForegroundService()
        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
        }
    }

    @Test
    fun modeBecomesAllowedAfterLocationForegroundIsAdded() {
        withTopActivity { fgActivity ->
            startForegroundService(fgActivity)
            startLocationForegroundService(fgActivity)
        }

        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)
        }
    }

    @Test
    fun modeBecomesIgnoredAfterLocationForegroundIsRemoved() {
        withTopActivity { fgActivity ->
            startForegroundService(fgActivity)
            startLocationForegroundService(fgActivity)
        }

        foregroundControlService.stopLocationForegroundService()

        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
        }
    }

    @Test
    fun modeChangeCallbackWhenStartingLocationForegroundService() {
        val gotCallback = CompletableFuture<Unit>()
        appopsManager.startWatchingMode(OPSTR_FINE_LOCATION, TEST_SERVICE_PKG,
                WATCH_FOREGROUND_CHANGES) { op, packageName ->
            if (op == OPSTR_FINE_LOCATION && packageName == TEST_SERVICE_PKG) {
                gotCallback.complete(Unit)
            }
        }

        withTopActivity { fgActivity ->
            startLocationForegroundService(fgActivity)
        }

        gotCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    @Test
    fun modeChangeCallbackAfterLocationForegroundIsStopped() {
        withTopActivity { fgActivity ->
            startLocationForegroundService(fgActivity)
        }

        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)
        }

        val gotCallback = CompletableFuture<Unit>()
        appopsManager.startWatchingMode(OPSTR_FINE_LOCATION, TEST_SERVICE_PKG,
                WATCH_FOREGROUND_CHANGES) { op, packageName ->
            if (op == OPSTR_FINE_LOCATION && packageName == TEST_SERVICE_PKG) {
                gotCallback.complete(Unit)
            }
        }

        foregroundControlService.stopLocationForegroundService()
        gotCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    @Test
    fun modeChangeCallbackAfterLocationForegroundIsAdded() {
        withTopActivity { fgActivity ->
            startForegroundService(fgActivity)
        }

        val gotCallback = CompletableFuture<Unit>()
        appopsManager.startWatchingMode(OPSTR_FINE_LOCATION, TEST_SERVICE_PKG,
                WATCH_FOREGROUND_CHANGES) { op, packageName ->
            if (op == OPSTR_FINE_LOCATION && packageName == TEST_SERVICE_PKG) {
                gotCallback.complete(Unit)
            }
        }

        withTopActivity { fgActivity ->
            startLocationForegroundService(fgActivity)
        }

        gotCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    @Test
    fun modeChangeCallbackAfterLocationForegroundIsRemoved() {
        withTopActivity { fgActivity ->
            startForegroundService(fgActivity)
            startLocationForegroundService(fgActivity)
        }

        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_ALLOWED)
        }

        val gotCallback = CompletableFuture<Unit>()
        appopsManager.startWatchingMode(OPSTR_FINE_LOCATION, TEST_SERVICE_PKG,
                WATCH_FOREGROUND_CHANGES) { op, packageName ->
            if (op == OPSTR_FINE_LOCATION && packageName == TEST_SERVICE_PKG) {
                gotCallback.complete(Unit)
            }
        }

        foregroundControlService.stopLocationForegroundService()

        gotCallback.get(TIMEOUT_MILLIS, MILLISECONDS)
    }

    @After
    fun cleanup() {
        foregroundControlService.cleanup()
        context.unbindService(serviceConnection)

        // Wait until app counts as background
        eventually {
            assertThat(testPkgAppOpMode).isEqualTo(MODE_IGNORED)
        }

        runWithShellPermissionIdentity {
            if (previousAppOpsConstants != null) {
                Settings.Global.putString(context.contentResolver,
                        APP_OPS_CONSTANTS, previousAppOpsConstants)
            }
        }
    }
}
