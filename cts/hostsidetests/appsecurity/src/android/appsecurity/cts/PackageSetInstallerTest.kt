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

package android.appsecurity.cts

import android.appsecurity.cts.PackageSetInstallerConstants.CHANGE_ID
import android.appsecurity.cts.PackageSetInstallerConstants.PERMISSION_HARD_RESTRICTED
import android.appsecurity.cts.PackageSetInstallerConstants.PERMISSION_IMMUTABLY_SOFT_RESTRICTED
import android.appsecurity.cts.PackageSetInstallerConstants.PERMISSION_KEY
import android.appsecurity.cts.PackageSetInstallerConstants.PERMISSION_NOT_RESTRICTED
import android.appsecurity.cts.PackageSetInstallerConstants.SHOULD_SUCCEED_KEY
import android.appsecurity.cts.PackageSetInstallerConstants.SHOULD_THROW_EXCEPTION_KEY
import android.appsecurity.cts.PackageSetInstallerConstants.TARGET_APK
import android.appsecurity.cts.PackageSetInstallerConstants.TARGET_PKG
import android.appsecurity.cts.PackageSetInstallerConstants.WHITELIST_APK
import android.appsecurity.cts.PackageSetInstallerConstants.WHITELIST_PKG
import android.cts.host.utils.DeviceJUnit4ClassRunnerWithParameters
import android.cts.host.utils.DeviceJUnit4Parameterized
import com.google.common.truth.Truth.assertThat
import com.google.common.truth.Truth.assertWithMessage
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized

/**
 * This test verifies protection for an exploit where any app could set the installer package
 * name for another app if the installer was uninstalled or never set.
 *
 * It mimics both the set installer logic and checks for a permission bypass caused by this exploit,
 * where an app could take installer for itself and whitelist itself to receive protected
 * permissions.
 */
@RunWith(DeviceJUnit4Parameterized::class)
@Parameterized.UseParametersRunnerFactory(
        DeviceJUnit4ClassRunnerWithParameters.RunnerFactory::class)
class PackageSetInstallerTest : BaseAppSecurityTest() {

    companion object {

        @JvmStatic
        @Parameterized.Parameters(name = "{1}")
        fun parameters() = arrayOf(
                arrayOf(false, "throwException"),
                arrayOf(true, "failSilently")
        )
    }

    @JvmField
    @Parameterized.Parameter(0)
    var failSilently = false

    @Parameterized.Parameter(1)
    lateinit var testName: String

    @Before
    @After
    fun uninstallTestPackages() {
        device.uninstallPackage(TARGET_PKG)
        device.uninstallPackage(WHITELIST_PKG)
    }

    @After
    fun resetChanges() {
        device.executeShellCommand("am compat reset $CHANGE_ID $TARGET_PKG")
        device.executeShellCommand("am compat reset $CHANGE_ID $WHITELIST_PKG")
    }

    @Test
    fun notRestricted() {
        runTest(removeWhitelistShouldSucceed = false,
                permission = PERMISSION_NOT_RESTRICTED,
                finalState = GrantState.TRUE)
    }

    @Test
    fun hardRestricted() {
        runTest(removeWhitelistShouldSucceed = true,
                permission = PERMISSION_HARD_RESTRICTED,
                finalState = GrantState.FALSE)
    }

    @Test
    fun immutablySoftRestrictedGranted() {
        runTest(removeWhitelistShouldSucceed = null,
                permission = PERMISSION_IMMUTABLY_SOFT_RESTRICTED,
                finalState = GrantState.TRUE_EXEMPT)
    }

    @Test
    fun immutablySoftRestrictedRevoked() {
        runTest(removeWhitelistShouldSucceed = null,
                permission = PERMISSION_IMMUTABLY_SOFT_RESTRICTED,
                restrictPermissions = true,
                finalState = GrantState.TRUE_RESTRICTED)
    }

    private fun runTest(
        removeWhitelistShouldSucceed: Boolean?,
        permission: String,
        restrictPermissions: Boolean = false,
        finalState: GrantState
    ) {
        // Verifies throwing a SecurityException or failing silently for backwards compatibility
        val testArgs: Map<String, String?> = mapOf(
                PERMISSION_KEY to permission
        )

        // First, install both packages and ensure no installer is set
        InstallMultiple(false, false)
                .addFile(TARGET_APK)
                .allowTest()
                .forUser(mPrimaryUserId)
                .apply {
                    if (restrictPermissions) {
                        restrictPermissions()
                    }
                }
                .run()

        InstallMultiple(false, false)
                .addFile(WHITELIST_APK)
                .allowTest()
                .forUser(mPrimaryUserId)
                .run()

        setChangeState()

        assertPermission(false, permission)
        assertTargetInstaller(null)

        // Install the installer whitelist app and take over the installer package. This methods
        // adopts the INSTALL_PACKAGES permission and verifies that the new behavior of checking
        // this permission is applied.
        Utils.runDeviceTests(device, WHITELIST_PKG, ".PermissionWhitelistTest",
                "setTargetInstallerPackage", mPrimaryUserId,
                testArgs.plus(SHOULD_THROW_EXCEPTION_KEY to (!failSilently).toString()))
        assertTargetInstaller(WHITELIST_PKG)

        // Verify that without whitelist restriction, the target app can be granted the permission
        grantPermission(permission)
        assertPermission(true, permission)
        revokePermission(permission)
        assertPermission(false, permission)

        val whitelistArgs = testArgs
                .plus(SHOULD_SUCCEED_KEY to removeWhitelistShouldSucceed?.toString())
                .filterValues { it != null }

        // Now restrict the permission from the target app using the whitelist app
        Utils.runDeviceTests(device, WHITELIST_PKG, ".PermissionWhitelistTest",
                "removeWhitelistRestrictedPermission", mPrimaryUserId, whitelistArgs)

        // Now remove the installer and verify the installer is wiped
        device.uninstallPackage(WHITELIST_PKG)
        assertTargetInstaller(null)

        // Verify whitelist restriction retained by attempting and failing to grant permission
        assertPermission(false, permission)
        grantPermission(permission)
        assertGrantState(finalState, permission)
        revokePermission(permission)

        // Attempt exploit to take over installer package and have target whitelist itself
        Utils.runDeviceTests(device, TARGET_PKG, ".PermissionRequestTest",
                "setSelfAsInstallerAndWhitelistPermission", mPrimaryUserId,
                testArgs.plus(SHOULD_THROW_EXCEPTION_KEY to (!failSilently).toString()))

        // Assert nothing changed about whitelist restriction
        assertTargetInstaller(null)
        grantPermission(permission)
        assertGrantState(finalState, permission)
    }

    private fun setChangeState() {
        val state = if (failSilently) "disable" else "enable"
        device.executeShellCommand("am compat $state $CHANGE_ID $TARGET_PKG")
        device.executeShellCommand("am compat $state $CHANGE_ID $WHITELIST_PKG")
    }

    private fun assertTargetInstaller(installer: String?) {
        assertThat(device.executeShellCommand("pm list packages -i | grep $TARGET_PKG").trim())
                .isEqualTo("package:$TARGET_PKG  installer=$installer")
    }

    private fun assertPermission(granted: Boolean, permission: String) {
        assertThat(device.executeShellCommand("dumpsys package $TARGET_PKG | grep $permission"))
                .contains("$permission: granted=$granted")
    }

    private fun grantPermission(permission: String) {
        device.executeShellCommand("pm grant $TARGET_PKG $permission")
    }

    private fun revokePermission(permission: String) {
        device.executeShellCommand("pm revoke $TARGET_PKG $permission")
    }

    private fun assertGrantState(state: GrantState, permission: String) {
        val output = device.executeShellCommand(
                "dumpsys package $TARGET_PKG | grep \"$permission: granted\"").trim()

        // Make sure only the expected output line is returned
        assertWithMessage(output).that(output.lines().size).isEqualTo(1)

        when (state) {
            GrantState.TRUE -> {
                assertThat(output).contains("granted=true")
                assertThat(output).doesNotContain("RESTRICTION")
                assertThat(output).doesNotContain("EXEMPT")
            }
            GrantState.TRUE_EXEMPT -> {
                assertThat(output).contains("granted=true")
                assertThat(output).contains("RESTRICTION_INSTALLER_EXEMPT")
            }
            GrantState.TRUE_RESTRICTED -> {
                assertThat(output).contains("granted=true")
                assertThat(output).contains("APPLY_RESTRICTION")
                assertThat(output).doesNotContain("EXEMPT")
            }
            GrantState.FALSE -> {
                assertThat(output).contains("granted=false")
            }
        }
    }

    enum class GrantState {
        // Granted in full, unrestricted
        TRUE,

        // Granted in full by exemption
        TRUE_EXEMPT,

        // Granted in part
        TRUE_RESTRICTED,

        // Not granted at all
        FALSE
    }
}
