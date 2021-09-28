/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.permission3.cts

import android.app.Instrumentation
import android.app.UiAutomation
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Resources
import android.provider.Settings
import android.support.test.uiautomator.By
import android.support.test.uiautomator.BySelector
import android.support.test.uiautomator.UiDevice
import android.support.test.uiautomator.UiObject2
import androidx.test.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import com.android.compatibility.common.util.SystemUtil.runShellCommand
import com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity
import com.android.compatibility.common.util.UiAutomatorUtils
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Rule
import java.util.concurrent.CompletableFuture
import java.util.regex.Pattern

abstract class BasePermissionTest {
    companion object {
        const val APK_DIRECTORY = "/data/local/tmp/cts/permission3"

        const val IDLE_TIMEOUT_MILLIS: Long = 1000
        const val UNEXPECTED_TIMEOUT_MILLIS = 1000
        const val TIMEOUT_MILLIS: Long = 20000
    }

    protected val instrumentation: Instrumentation = InstrumentationRegistry.getInstrumentation()
    protected val context: Context = instrumentation.context
    protected val uiAutomation: UiAutomation = instrumentation.uiAutomation
    protected val uiDevice: UiDevice = UiDevice.getInstance(instrumentation)
    protected val packageManager: PackageManager = context.packageManager
    private val mPermissionControllerResources: Resources = context.createPackageContext(
            context.packageManager.permissionControllerPackageName, 0).resources

    @get:Rule
    val activityRule = ActivityTestRule(StartForFutureActivity::class.java, false, false)

    private var screenTimeoutBeforeTest: Long = 0L

    @Before
    fun setUp() {
        runWithShellPermissionIdentity {
            screenTimeoutBeforeTest = Settings.System.getLong(
                context.contentResolver, Settings.System.SCREEN_OFF_TIMEOUT
            )
            Settings.System.putLong(
                context.contentResolver, Settings.System.SCREEN_OFF_TIMEOUT, 1800000L
            )
        }

        uiDevice.wakeUp()
        runShellCommand(instrumentation, "wm dismiss-keyguard")

        uiDevice.findObject(By.text("Close"))?.click()
    }

    @After
    fun tearDown() {
        runWithShellPermissionIdentity {
            Settings.System.putLong(
                context.contentResolver, Settings.System.SCREEN_OFF_TIMEOUT,
                screenTimeoutBeforeTest
            )
        }

        pressHome()
    }

    protected fun getPermissionControllerString(res: String): Pattern =
            Pattern.compile(Pattern.quote(mPermissionControllerResources.getString(
                    mPermissionControllerResources.getIdentifier(
                            res, "string", "com.android.permissioncontroller"))),
                    Pattern.CASE_INSENSITIVE or Pattern.UNICODE_CASE)

    protected fun installPackage(
        apkPath: String,
        reinstall: Boolean = false,
        expectSuccess: Boolean = true
    ) {
        val output = runShellCommand("pm install${if (reinstall) " -r" else ""} $apkPath").trim()
        if (expectSuccess) {
            assertEquals("Success", output)
        } else {
            assertNotEquals("Success", output)
        }
    }

    protected fun uninstallPackage(packageName: String, requireSuccess: Boolean = true) {
        val output = runShellCommand("pm uninstall $packageName").trim()
        if (requireSuccess) {
            assertEquals("Success", output)
        }
    }

    protected fun waitFindObject(selector: BySelector): UiObject2 {
        waitForIdle()
        return UiAutomatorUtils.waitFindObject(selector)
    }

    protected fun waitFindObject(selector: BySelector, timeoutMillis: Long): UiObject2 {
        waitForIdle()
        return UiAutomatorUtils.waitFindObject(selector, timeoutMillis)
    }

    protected fun click(selector: BySelector, timeoutMillis: Long = 20_000) {
        waitFindObject(selector, timeoutMillis).click()
        waitForIdle()
    }

    protected fun pressBack() {
        uiDevice.pressBack()
        waitForIdle()
    }

    protected fun pressHome() {
        uiDevice.pressHome()
        waitForIdle()
    }

    protected fun waitForIdle() = uiAutomation.waitForIdle(IDLE_TIMEOUT_MILLIS, TIMEOUT_MILLIS)

    protected fun startActivityForFuture(
        intent: Intent
    ): CompletableFuture<Instrumentation.ActivityResult> =
        activityRule.launchActivity(null).startActivityForFuture(intent)
}
