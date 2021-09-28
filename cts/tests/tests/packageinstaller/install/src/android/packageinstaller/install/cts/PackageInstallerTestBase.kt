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

package android.packageinstaller.install.cts

import android.app.PendingIntent
import android.app.PendingIntent.FLAG_UPDATE_CURRENT
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.Intent.EXTRA_INTENT
import android.content.Intent.FLAG_ACTIVITY_CLEAR_TASK
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.IntentFilter
import android.content.pm.PackageInstaller
import android.content.pm.PackageInstaller.EXTRA_STATUS
import android.content.pm.PackageInstaller.STATUS_FAILURE_INVALID
import android.content.pm.PackageInstaller.STATUS_PENDING_USER_ACTION
import android.content.pm.PackageInstaller.SessionParams.MODE_FULL_INSTALL
import android.content.pm.PackageManager
import android.support.test.uiautomator.By
import androidx.test.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import android.support.test.uiautomator.UiDevice
import android.support.test.uiautomator.Until
import androidx.core.content.FileProvider
import com.android.compatibility.common.util.FutureResultActivity
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import java.io.File
import java.lang.IllegalArgumentException
import java.util.concurrent.CompletableFuture
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

const val TEST_APK_NAME = "CtsEmptyTestApp.apk"
const val TEST_APK_PACKAGE_NAME = "android.packageinstaller.emptytestapp.cts"
const val TEST_APK_EXTERNAL_LOCATION = "/data/local/tmp/cts/packageinstaller"
const val INSTALL_ACTION_CB = "PackageInstallerTestBase.install_cb"

const val CONTENT_AUTHORITY = "android.packageinstaller.install.cts.fileprovider"

const val PACKAGE_INSTALLER_PACKAGE_NAME = "com.android.packageinstaller"
const val SYSTEM_PACKAGE_NAME = "android"

const val TIMEOUT = 60000L
const val APP_OP_STR = "REQUEST_INSTALL_PACKAGES"

open class PackageInstallerTestBase {
    @get:Rule
    val installDialogStarter = ActivityTestRule(FutureResultActivity::class.java)

    private val context = InstrumentationRegistry.getTargetContext()
    private val pm = context.packageManager
    private val uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private val apkFile = File(context.filesDir, TEST_APK_NAME)

    /** If a status was received the value of the status, otherwise null */
    private var installSessionResult = LinkedBlockingQueue<Int>()

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val status = intent.getIntExtra(EXTRA_STATUS, STATUS_FAILURE_INVALID)

            if (status == STATUS_PENDING_USER_ACTION) {
                val activityIntent = intent.getParcelableExtra<Intent>(EXTRA_INTENT)
                activityIntent!!.addFlags(FLAG_ACTIVITY_CLEAR_TASK or FLAG_ACTIVITY_NEW_TASK)
                installDialogStarter.activity.startActivityForResult(activityIntent)
            }

            installSessionResult.offer(status)
        }
    }

    @Before
    fun copyTestApk() {
        File(TEST_APK_EXTERNAL_LOCATION, TEST_APK_NAME).copyTo(target = apkFile, overwrite = true)
    }

    @Before
    fun wakeUpScreen() {
        if (!uiDevice.isScreenOn) {
            uiDevice.wakeUp()
        }
        uiDevice.executeShellCommand("wm dismiss-keyguard")
    }

    @Before
    fun assertTestPackageNotInstalled() {
        try {
            context.packageManager.getPackageInfo(TEST_APK_PACKAGE_NAME, 0)
            Assert.fail("Package should not be installed")
        } catch (expected: PackageManager.NameNotFoundException) {
        }
    }

    @Before
    fun registerInstallResultReceiver() {
        context.registerReceiver(receiver, IntentFilter(INSTALL_ACTION_CB))
    }

    @Before
    fun waitForUIIdle() {
        uiDevice.waitForIdle()
    }

    /**
     * Wait for session's install result and return it
     */
    protected fun getInstallSessionResult(timeout: Long = TIMEOUT): Int? {
        return installSessionResult.poll(timeout, TimeUnit.MILLISECONDS)
    }

    /**
     * Start an installation via a session
     */
    protected fun startInstallationViaSession(): CompletableFuture<Int> {
        val pi = pm.packageInstaller

        // Create session
        val sessionId = pi.createSession(PackageInstaller.SessionParams(MODE_FULL_INSTALL))
        val session = pi.openSession(sessionId)!!

        // Write data to session
        apkFile.inputStream().use { fileOnDisk ->
            session.openWrite(TEST_APK_NAME, 0, -1).use { sessionFile ->
                fileOnDisk.copyTo(sessionFile)
            }
        }

        // Commit session
        val dialog = FutureResultActivity.doAndAwaitStart {
            val pendingIntent = PendingIntent.getBroadcast(context, 0, Intent(INSTALL_ACTION_CB),
                    FLAG_UPDATE_CURRENT)
            session.commit(pendingIntent.intentSender)
        }

        // The system should have asked us to launch the installer
        Assert.assertEquals(STATUS_PENDING_USER_ACTION, getInstallSessionResult())

        return dialog
    }

    /**
     * Start an installation via a session
     */
    protected fun startInstallationViaIntent(): CompletableFuture<Int> {
        val intent = Intent(Intent.ACTION_INSTALL_PACKAGE)
        intent.data = FileProvider.getUriForFile(context, CONTENT_AUTHORITY, apkFile)
        intent.putExtra(Intent.EXTRA_RETURN_RESULT, true)
        intent.flags = Intent.FLAG_GRANT_READ_URI_PERMISSION

        return installDialogStarter.activity.startActivityForResult(intent)
    }

    fun assertInstalled() {
        // Throws exception if package is not installed.
        pm.getPackageInfo(TEST_APK_PACKAGE_NAME, 0)
    }

    fun assertNotInstalled() {
        try {
            pm.getPackageInfo(TEST_APK_PACKAGE_NAME, 0)
            Assert.fail("Package should not be installed")
        } catch (expected: PackageManager.NameNotFoundException) {
        }
    }

    /**
     * Click a button in the UI of the installer app
     *
     * @param resId The resource ID of the button to click
     */
    fun clickInstallerUIButton(resId: String) {
        uiDevice.wait(Until.findObject(By.res(SYSTEM_PACKAGE_NAME, resId)), TIMEOUT)
                .click()
    }

    /**
     * Sets the given secure setting to the provided value.
     */
    fun setSecureSetting(secureSetting: String, value: Int) {
        uiDevice.executeShellCommand("settings put secure $secureSetting $value")
    }

    fun setSecureFrp(secureFrp: Boolean) {
        uiDevice.executeShellCommand("settings --user 0 " +
                "put secure secure_frp_mode ${if (secureFrp) 1 else 0}")
    }

    @After
    fun unregisterInstallResultReceiver() {
        try {
            context.unregisterReceiver(receiver)
        } catch (ignored: IllegalArgumentException) {
        }
    }

    @After
    fun uninstallTestPackage() {
        uiDevice.executeShellCommand("pm uninstall $TEST_APK_PACKAGE_NAME")
    }
}
