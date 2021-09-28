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

package android.os.cts

import android.app.Instrumentation
import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_AUTO_REVOKE_PERMISSIONS
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.pm.PackageManager
import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.content.res.Resources
import android.net.Uri
import android.platform.test.annotations.AppModeFull
import android.provider.DeviceConfig
import android.support.test.uiautomator.By
import android.support.test.uiautomator.BySelector
import android.support.test.uiautomator.UiObject2
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Switch
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.android.compatibility.common.util.MatcherUtils.hasTextThat
import com.android.compatibility.common.util.SystemUtil
import com.android.compatibility.common.util.SystemUtil.runShellCommand
import com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity
import com.android.compatibility.common.util.ThrowingSupplier
import com.android.compatibility.common.util.UiAutomatorUtils
import com.android.compatibility.common.util.click
import com.android.compatibility.common.util.depthFirstSearch
import com.android.compatibility.common.util.lowestCommonAncestor
import com.android.compatibility.common.util.textAsString
import com.android.compatibility.common.util.uiDump
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.CoreMatchers.containsStringIgnoringCase
import org.hamcrest.CoreMatchers.equalTo
import org.hamcrest.Matcher
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThat
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.reflect.Modifier
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import java.util.regex.Pattern

private const val APK_PATH = "/data/local/tmp/cts/os/CtsAutoRevokeDummyApp.apk"
private const val APK_PACKAGE_NAME = "android.os.cts.autorevokedummyapp"
private const val APK_PATH_2 = "/data/local/tmp/cts/os/CtsAutoRevokePreRApp.apk"
private const val APK_PACKAGE_NAME_2 = "android.os.cts.autorevokeprerapp"
private const val READ_CALENDAR = "android.permission.READ_CALENDAR"

/**
 * Test for auto revoke
 */
@RunWith(AndroidJUnit4::class)
class AutoRevokeTest {

    private val context: Context = InstrumentationRegistry.getTargetContext()
    private val instrumentation: Instrumentation = InstrumentationRegistry.getInstrumentation()

    private val mPermissionControllerResources: Resources = context.createPackageContext(
            context.packageManager.permissionControllerPackageName, 0).resources

    companion object {
        const val LOG_TAG = "AutoRevokeTest"
    }

    @Before
    fun setup() {
        // Kill Permission Controller
        assertThat(
                runShellCommand("killall " +
                        context.packageManager.permissionControllerPackageName),
                equalTo(""))

        // Collapse notifications
        assertThat(
                runShellCommand("cmd statusbar collapse"),
                equalTo(""))

        // Wake up the device
        runShellCommand("input keyevent KEYCODE_WAKEUP")
        runShellCommand("input keyevent 82")
    }

    @AppModeFull(reason = "Uses separate apps for testing")
    @Test
    fun testUnusedApp_getsPermissionRevoked() {
        withUnusedThresholdMs(3L) {
            withDummyApp {
                // Setup
                startApp()
                clickPermissionAllow()
                eventually {
                    assertPermission(PERMISSION_GRANTED)
                }
                goBack()
                goHome()
                goBack()
                Thread.sleep(5)

                // Run
                runAutoRevoke()

                // Verify
                eventually {
                    assertPermission(PERMISSION_DENIED)
                }
                runShellCommand("cmd statusbar expand-notifications")
                waitFindObject(By.textContains("unused app"))
                        .click()
                waitFindObject(By.text(APK_PACKAGE_NAME))
                waitFindObject(By.text("Calendar permission removed"))
            }
        }
    }

    @AppModeFull(reason = "Uses separate apps for testing")
    @Test
    fun testUsedApp_doesntGetPermissionRevoked() {
        withUnusedThresholdMs(100_000L) {
            withDummyApp {
                // Setup
                startApp()
                clickPermissionAllow()
                eventually {
                    assertPermission(PERMISSION_GRANTED)
                }
                goHome()
                Thread.sleep(5)

                // Run
                runAutoRevoke()
                Thread.sleep(1000)

                // Verify
                assertPermission(PERMISSION_GRANTED)
            }
        }
    }

    @AppModeFull(reason = "Uses separate apps for testing")
    @Test
    fun testPreRUnusedApp_doesntGetPermissionRevoked() {
        withUnusedThresholdMs(3L) {
            withDummyApp(APK_PATH_2, APK_PACKAGE_NAME_2) {
                withDummyApp {
                    startApp(APK_PACKAGE_NAME_2)
                    clickPermissionAllow()
                    eventually {
                        assertPermission(PERMISSION_GRANTED, APK_PACKAGE_NAME_2)
                    }

                    goBack()
                    goHome()
                    goBack()

                    startApp()
                    clickPermissionAllow()
                    eventually {
                        assertPermission(PERMISSION_GRANTED)
                    }

                    goBack()
                    goHome()
                    goBack()
                    Thread.sleep(20)

                    // Run
                    runAutoRevoke()
                    Thread.sleep(500)

                    // Verify
                    eventually {
                        assertPermission(PERMISSION_DENIED)
                        assertPermission(PERMISSION_GRANTED, APK_PACKAGE_NAME_2)
                    }
                }
            }
        }
    }

    @AppModeFull(reason = "Uses separate apps for testing")
    @Test
    fun testAutoRevoke_userWhitelisting() {
        withUnusedThresholdMs(4L) {
            withDummyApp {
                // Setup
                startApp()
                clickPermissionAllow()
                assertWhitelistState(false)

                // Verify
                waitFindObject(byTextIgnoreCase("Request whitelist")).click()
                waitFindObject(byTextIgnoreCase("Permissions")).click()
                val autoRevokeEnabledToggle = getWhitelistToggle()
                assertTrue(autoRevokeEnabledToggle.isChecked)

                // Grant whitelist
                autoRevokeEnabledToggle.click()
                eventually {
                    assertFalse(getWhitelistToggle().isChecked)
                }

                // Run
                goBack()
                goBack()
                goBack()
                runAutoRevoke()
                Thread.sleep(500L)

                // Verify
                startApp()
                assertWhitelistState(true)
                assertPermission(PERMISSION_GRANTED)
            }
        }
    }

    @AppModeFull(reason = "Uses separate apps for testing")
    @Test
    fun testInstallGrants_notRevokedImmediately() {
        withUnusedThresholdMs(TimeUnit.DAYS.toMillis(30)) {
            withDummyApp {
                // Setup
                goToPermissions()
                click("Calendar")
                click("Allow")
                Thread.sleep(500)
                goBack()
                goBack()
                goBack()
                eventually {
                    assertPermission(PERMISSION_GRANTED)
                }

                // Run
                runAutoRevoke()
                Thread.sleep(500)

                // Verify
                assertPermission(PERMISSION_GRANTED)
            }
        }
    }

    @AppModeFull(reason = "Uses separate apps for testing")
    @Test
    fun testAutoRevoke_whitelistingApis() {
        withDummyApp {
            val pm = context.packageManager
            runWithShellPermissionIdentity {
                assertFalse(pm.isAutoRevokeWhitelisted(APK_PACKAGE_NAME))
            }

            runWithShellPermissionIdentity {
                assertTrue(pm.setAutoRevokeWhitelisted(APK_PACKAGE_NAME, true))
            }
            eventually {
                runWithShellPermissionIdentity {
                    assertTrue(pm.isAutoRevokeWhitelisted(APK_PACKAGE_NAME))
                }
            }

            runWithShellPermissionIdentity {
                assertTrue(pm.setAutoRevokeWhitelisted(APK_PACKAGE_NAME, false))
            }
            eventually {
                runWithShellPermissionIdentity {
                    assertFalse(pm.isAutoRevokeWhitelisted(APK_PACKAGE_NAME))
                }
            }
        }
    }

    private fun runAutoRevoke() {
        runShellCommand("cmd jobscheduler run -u 0 " +
                "-f ${context.packageManager.permissionControllerPackageName} 2")
    }

    private inline fun <T> withDeviceConfig(
        namespace: String,
        name: String,
        value: String,
        action: () -> T
    ): T {
        val oldValue = runWithShellPermissionIdentity(ThrowingSupplier {
            DeviceConfig.getProperty(namespace, name)
        })
        try {
            runWithShellPermissionIdentity {
                DeviceConfig.setProperty(namespace, name, value, false /* makeDefault */)
            }
            return action()
        } finally {
            runWithShellPermissionIdentity {
                DeviceConfig.setProperty(namespace, name, oldValue, false /* makeDefault */)
            }
        }
    }

    private inline fun <T> withUnusedThresholdMs(threshold: Long, action: () -> T): T {
        return withDeviceConfig(
                "permissions", "auto_revoke_unused_threshold_millis2", threshold.toString(), action)
    }

    private fun installApp(apk: String = APK_PATH) {
        assertThat(runShellCommand("pm install -r $apk"), containsString("Success"))
    }

    private fun uninstallApp(packageName: String = APK_PACKAGE_NAME) {
        assertThat(runShellCommand("pm uninstall $packageName"), containsString("Success"))
    }

    private fun startApp(packageName: String = APK_PACKAGE_NAME) {
        runShellCommand("am start -n $packageName/$packageName.MainActivity")
    }

    private fun goHome() {
        runShellCommand("input keyevent KEYCODE_HOME")
    }

    private fun goBack() {
        runShellCommand("input keyevent KEYCODE_BACK")
    }

    private fun clickPermissionAllow() {
        if (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
            waitFindObject(By.text(Pattern.compile(
                    Pattern.quote(mPermissionControllerResources.getString(
                            mPermissionControllerResources.getIdentifier(
                                    "grant_dialog_button_allow", "string",
                                    "com.android.permissioncontroller"))),
                    Pattern.CASE_INSENSITIVE or Pattern.UNICODE_CASE))).click()
        } else {
            waitFindObject(By.res("com.android.permissioncontroller:id/permission_allow_button"))
                    .click()
        }
    }

    private inline fun withDummyApp(
        apk: String = APK_PATH,
        packageName: String = APK_PACKAGE_NAME,
        action: () -> Unit
    ) {
        installApp(apk)
        try {
            // Try to reduce flakiness caused by new package update not propagating in time
            Thread.sleep(1000)
            action()
        } finally {
            uninstallApp(packageName)
        }
    }

    private fun assertPermission(state: Int, packageName: String = APK_PACKAGE_NAME) {
        runWithShellPermissionIdentity {
            assertEquals(
                permissionStateToString(state),
                permissionStateToString(
                        context.packageManager.checkPermission(READ_CALENDAR, packageName)))
        }
    }

    private fun goToPermissions(packageName: String = APK_PACKAGE_NAME) {
        context.startActivity(Intent(ACTION_AUTO_REVOKE_PERMISSIONS)
                .setData(Uri.fromParts("package", packageName, null))
                .addFlags(FLAG_ACTIVITY_NEW_TASK))

        waitForIdle()
        click("Permissions")
    }

    private fun click(label: String) {
        waitFindNode(hasTextThat(containsStringIgnoringCase(label))).click()
    }

    private fun assertWhitelistState(state: Boolean) {
        assertThat(
            waitFindObject(By.textStartsWith("Auto-revoke whitelisted: ")).text,
            containsString(state.toString()))
    }

    private fun getWhitelistToggle(): AccessibilityNodeInfo {
        waitForIdle()
        return eventually {
            val ui = instrumentation.uiAutomation.rootInActiveWindow
            return@eventually ui.lowestCommonAncestor(
                { node -> node.textAsString == "Remove permissions if app isn’t used" },
                { node -> node.className == Switch::class.java.name }
            ).assertNotNull {
                "No auto-revoke whitelist toggle found in\n${uiDump(ui)}"
            }.depthFirstSearch { node -> node.className == Switch::class.java.name }!!
        }
    }

    private fun waitForIdle() {
        instrumentation.uiAutomation.waitForIdle(1000, 10000)
        Thread.sleep(500)
        instrumentation.uiAutomation.waitForIdle(1000, 10000)
    }

    private inline fun <T> eventually(crossinline action: () -> T): T {
        val res = AtomicReference<T>()
        SystemUtil.eventually {
            res.set(action())
        }
        return res.get()
    }

    private fun waitFindObject(selector: BySelector): UiObject2 {
        try {
            return UiAutomatorUtils.waitFindObject(selector)
        } catch (e: RuntimeException) {
            val ui = instrumentation.uiAutomation.rootInActiveWindow

            val title = ui.depthFirstSearch { node ->
                node.viewIdResourceName?.contains("alertTitle") == true
            }
            val okButton = ui.depthFirstSearch { node ->
                node.textAsString?.equals("OK", ignoreCase = true) ?: false
            }

            if (title?.text?.toString() == "Android System" && okButton != null) {
                // Auto dismiss occasional system dialogs to prevent interfering with the test
                android.util.Log.w(LOG_TAG, "Ignoring exception", e)
                okButton.click()
                return UiAutomatorUtils.waitFindObject(selector)
            } else {
                throw e
            }
        }
    }

    /**
     * For some reason waitFindObject sometimes fails to find UI that is present in the view hierarchy
     */
    private fun waitFindNode(matcher: Matcher<AccessibilityNodeInfo>): AccessibilityNodeInfo {
        return eventually {
            val ui = instrumentation.uiAutomation.rootInActiveWindow
            ui.depthFirstSearch { node ->
                matcher.matches(node)
            }.assertNotNull {
                "No view found matching $matcher:\n\n${uiDump(ui)}"
            }
        }
    }

    private fun byTextIgnoreCase(txt: String): BySelector {
        return By.text(Pattern.compile(txt, Pattern.CASE_INSENSITIVE))
    }

    private fun permissionStateToString(state: Int): String {
        return constToString<PackageManager>("PERMISSION_", state)
    }
}

inline fun <reified T> constToString(prefix: String, value: Int): String {
    return T::class.java.declaredFields.filter {
        Modifier.isStatic(it.modifiers) && it.name.startsWith(prefix)
    }.map {
        it.isAccessible = true
        it.name to it.get(null)
    }.find { (k, v) ->
        v == value
    }.assertNotNull {
        "None of ${T::class.java.simpleName}.$prefix* == $value"
    }.first
}

inline fun <T> T?.assertNotNull(errorMsg: () -> String): T {
    return if (this == null) throw AssertionError(errorMsg()) else this
}
