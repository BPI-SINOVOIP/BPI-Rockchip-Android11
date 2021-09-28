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

import android.app.Activity
import android.app.Instrumentation
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.support.test.uiautomator.By
import android.support.test.uiautomator.BySelector
import android.support.test.uiautomator.UiScrollable
import android.support.test.uiautomator.UiSelector
import android.text.Spanned
import android.text.style.ClickableSpan
import android.view.View
import com.android.compatibility.common.util.SystemUtil.eventually
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import java.util.concurrent.TimeUnit
import java.util.regex.Pattern

abstract class BaseUsePermissionTest : BasePermissionTest() {
    companion object {
        const val APP_APK_PATH_22 = "$APK_DIRECTORY/CtsUsePermissionApp22.apk"
        const val APP_APK_PATH_22_CALENDAR_ONLY =
            "$APK_DIRECTORY/CtsUsePermissionApp22CalendarOnly.apk"
        const val APP_APK_PATH_23 = "$APK_DIRECTORY/CtsUsePermissionApp23.apk"
        const val APP_APK_PATH_25 = "$APK_DIRECTORY/CtsUsePermissionApp25.apk"
        const val APP_APK_PATH_26 = "$APK_DIRECTORY/CtsUsePermissionApp26.apk"
        const val APP_APK_PATH_28 = "$APK_DIRECTORY/CtsUsePermissionApp28.apk"
        const val APP_APK_PATH_29 = "$APK_DIRECTORY/CtsUsePermissionApp29.apk"
        const val APP_APK_PATH_LATEST = "$APK_DIRECTORY/CtsUsePermissionAppLatest.apk"
        const val APP_APK_PATH_LATEST_WITH_BACKGROUND =
                "$APK_DIRECTORY/CtsUsePermissionAppLatestWithBackground.apk"
        const val APP_APK_PATH_WITH_OVERLAY = "$APK_DIRECTORY/CtsUsePermissionAppWithOverlay.apk"
        const val APP_PACKAGE_NAME = "android.permission3.cts.usepermission"

        const val ALLOW_BUTTON =
                "com.android.permissioncontroller:id/permission_allow_button"
        const val ALLOW_FOREGROUND_BUTTON =
                "com.android.permissioncontroller:id/permission_allow_foreground_only_button"
        const val DENY_BUTTON = "com.android.permissioncontroller:id/permission_deny_button"
        const val DENY_AND_DONT_ASK_AGAIN_BUTTON =
                "com.android.permissioncontroller:id/permission_deny_and_dont_ask_again_button"
        const val NO_UPGRADE_BUTTON =
                "com.android.permissioncontroller:id/permission_no_upgrade_button"
        const val NO_UPGRADE_AND_DONT_ASK_AGAIN_BUTTON =
                "com.android.permissioncontroller:" +
                        "id/permission_no_upgrade_and_dont_ask_again_button"

        const val ALLOW_BUTTON_TEXT = "grant_dialog_button_allow"
        const val ALLOW_FOREGROUND_BUTTON_TEXT = "grant_dialog_button_allow_foreground"
        const val DENY_BUTTON_TEXT = "grant_dialog_button_deny"
        const val DENY_AND_DONT_ASK_AGAIN_BUTTON_TEXT =
                "grant_dialog_button_deny_and_dont_ask_again"
        const val NO_UPGRADE_AND_DONT_ASK_AGAIN_BUTTON_TEXT = "grant_dialog_button_no_upgrade"
    }

    enum class PermissionState {
        ALLOWED,
        DENIED,
        DENIED_WITH_PREJUDICE
    }

    protected val isTv = packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
    protected val isWatch = packageManager.hasSystemFeature(PackageManager.FEATURE_WATCH)
    protected val isAutomotive = packageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)

    private val platformResources = context.createPackageContext("android", 0).resources
    private val permissionToLabelResNameMap =
        if (!packageManager.arePermissionsIndividuallyControlled()) {
            mapOf(
                // Contacts
                android.Manifest.permission.READ_CONTACTS
                    to "@android:string/permgrouplab_contacts",
                android.Manifest.permission.WRITE_CONTACTS
                    to "@android:string/permgrouplab_contacts",
                // Calendar
                android.Manifest.permission.READ_CALENDAR
                    to "@android:string/permgrouplab_calendar",
                android.Manifest.permission.WRITE_CALENDAR
                    to "@android:string/permgrouplab_calendar",
                // SMS
                android.Manifest.permission.SEND_SMS to "@android:string/permgrouplab_sms",
                android.Manifest.permission.RECEIVE_SMS to "@android:string/permgrouplab_sms",
                android.Manifest.permission.READ_SMS to "@android:string/permgrouplab_sms",
                android.Manifest.permission.RECEIVE_WAP_PUSH to "@android:string/permgrouplab_sms",
                android.Manifest.permission.RECEIVE_MMS to "@android:string/permgrouplab_sms",
                "android.permission.READ_CELL_BROADCASTS" to "@android:string/permgrouplab_sms",
                // Storage
                android.Manifest.permission.READ_EXTERNAL_STORAGE
                    to "@android:string/permgrouplab_storage",
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                    to "@android:string/permgrouplab_storage",
                // Location
                android.Manifest.permission.ACCESS_FINE_LOCATION
                    to "@android:string/permgrouplab_location",
                android.Manifest.permission.ACCESS_COARSE_LOCATION
                    to "@android:string/permgrouplab_location",
                // Phone
                android.Manifest.permission.READ_PHONE_STATE
                    to "@android:string/permgrouplab_phone",
                android.Manifest.permission.CALL_PHONE to "@android:string/permgrouplab_phone",
                "android.permission.ACCESS_IMS_CALL_SERVICE"
                    to "@android:string/permgrouplab_phone",
                android.Manifest.permission.READ_CALL_LOG to "@android:string/permgrouplab_phone",
                android.Manifest.permission.WRITE_CALL_LOG to "@android:string/permgrouplab_phone",
                android.Manifest.permission.ADD_VOICEMAIL to "@android:string/permgrouplab_phone",
                android.Manifest.permission.USE_SIP to "@android:string/permgrouplab_phone",
                android.Manifest.permission.PROCESS_OUTGOING_CALLS
                    to "@android:string/permgrouplab_phone",
                // Microphone
                android.Manifest.permission.RECORD_AUDIO
                    to "@android:string/permgrouplab_microphone",
                // Camera
                android.Manifest.permission.CAMERA to "@android:string/permgrouplab_camera",
                // Body sensors
                android.Manifest.permission.BODY_SENSORS to "@android:string/permgrouplab_sensors"
            )
        } else {
            mapOf(
                // Contacts
                android.Manifest.permission.READ_CONTACTS to "@android:string/permlab_readContacts",
                android.Manifest.permission.WRITE_CONTACTS
                    to "@android:string/permlab_writeContacts",
                // Calendar
                android.Manifest.permission.READ_CALENDAR
                    to "@android:string/permgrouplab_calendar",
                android.Manifest.permission.WRITE_CALENDAR
                    to "@android:string/permgrouplab_calendar",
                // SMS
                android.Manifest.permission.SEND_SMS to "@android:string/permlab_sendSms",
                android.Manifest.permission.RECEIVE_SMS to "@android:string/permlab_receiveSms",
                android.Manifest.permission.READ_SMS to "@android:string/permlab_readSms",
                android.Manifest.permission.RECEIVE_WAP_PUSH
                    to "@android:string/permlab_receiveWapPush",
                android.Manifest.permission.RECEIVE_MMS to "@android:string/permlab_receiveMms",
                "android.permission.READ_CELL_BROADCASTS"
                    to "@android:string/permlab_readCellBroadcasts",
                // Storage
                android.Manifest.permission.READ_EXTERNAL_STORAGE
                    to "@android:string/permgrouplab_storage",
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                    to "@android:string/permgrouplab_storage",
                // Location
                android.Manifest.permission.ACCESS_FINE_LOCATION
                    to "@android:string/permgrouplab_location",
                android.Manifest.permission.ACCESS_COARSE_LOCATION
                    to "@android:string/permgrouplab_location",
                // Phone
                android.Manifest.permission.READ_PHONE_STATE
                    to "@android:string/permlab_readPhoneState",
                android.Manifest.permission.CALL_PHONE to "@android:string/permlab_callPhone",
                "android.permission.ACCESS_IMS_CALL_SERVICE"
                    to "@android:string/permlab_accessImsCallService",
                android.Manifest.permission.READ_CALL_LOG to "@android:string/permlab_readCallLog",
                android.Manifest.permission.WRITE_CALL_LOG
                    to "@android:string/permlab_writeCallLog",
                android.Manifest.permission.ADD_VOICEMAIL to "@android:string/permlab_addVoicemail",
                android.Manifest.permission.USE_SIP to "@android:string/permlab_use_sip",
                android.Manifest.permission.PROCESS_OUTGOING_CALLS
                    to "@android:string/permlab_processOutgoingCalls",
                // Microphone
                android.Manifest.permission.RECORD_AUDIO
                    to "@android:string/permgrouplab_microphone",
                // Camera
                android.Manifest.permission.CAMERA to "@android:string/permgrouplab_camera",
                // Body sensors
                android.Manifest.permission.BODY_SENSORS to "@android:string/permgrouplab_sensors"
            )
        }

    @Before
    @After
    fun uninstallApp() {
        uninstallPackage(APP_PACKAGE_NAME, requireSuccess = false)
    }

    protected fun clearTargetSdkWarning() =
        click(By.res("android:id/button1"))

    protected fun clickPermissionReviewContinue() {
        if (isAutomotive) {
            click(By.text(getPermissionControllerString("review_button_continue")))
        } else {
            click(By.res("com.android.permissioncontroller:id/continue_button"))
        }
    }

    protected fun clickPermissionReviewCancel() {
        if (isAutomotive) {
            click(By.text(getPermissionControllerString("review_button_cancel")))
        } else {
            click(By.res("com.android.permissioncontroller:id/cancel_button"))
        }
    }

    protected fun approvePermissionReview() {
        startAppActivityAndAssertResultCode(Activity.RESULT_OK) {
            clickPermissionReviewContinue()
        }
    }

    protected fun cancelPermissionReview() {
        startAppActivityAndAssertResultCode(Activity.RESULT_CANCELED) {
            clickPermissionReviewCancel()
        }
    }

    protected fun assertAppDoesNotNeedPermissionReview() {
        startAppActivityAndAssertResultCode(Activity.RESULT_OK) {}
    }

    protected inline fun startAppActivityAndAssertResultCode(
        expectedResultCode: Int,
        block: () -> Unit
    ) {
        val future = startActivityForFuture(
            Intent().apply {
                component = ComponentName(
                    APP_PACKAGE_NAME, "$APP_PACKAGE_NAME.FinishOnCreateActivity"
                )
            }
        )
        block()
        assertEquals(
            expectedResultCode, future.get(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS).resultCode
        )
    }

    protected inline fun requestAppPermissionsForNoResult(
        vararg permissions: String?,
        block: () -> Unit
    ) {
        // Request the permissions
        context.startActivity(
                Intent().apply {
                    component = ComponentName(
                            APP_PACKAGE_NAME, "$APP_PACKAGE_NAME.RequestPermissionsActivity"
                    )
                    putExtra("$APP_PACKAGE_NAME.PERMISSIONS", permissions)
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                }
        )
        waitForIdle()
        // Perform the post-request action
        block()
    }

    protected inline fun requestAppPermissions(
        vararg permissions: String?,
        block: () -> Unit
    ): Instrumentation.ActivityResult {
        // Request the permissions
        val future = startActivityForFuture(
            Intent().apply {
                component = ComponentName(
                    APP_PACKAGE_NAME, "$APP_PACKAGE_NAME.RequestPermissionsActivity"
                )
                putExtra("$APP_PACKAGE_NAME.PERMISSIONS", permissions)
            }
        )
        waitForIdle()
        // Perform the post-request action
        block()
        return future.get(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)
    }

    protected inline fun requestAppPermissionsAndAssertResult(
        permissions: Array<out String?>,
        permissionAndExpectedGrantResults: Array<out Pair<String?, Boolean>>,
        block: () -> Unit
    ) {
        val result = requestAppPermissions(*permissions, block = block)
        assertEquals(Activity.RESULT_OK, result.resultCode)
        assertEquals(
            permissionAndExpectedGrantResults.toList(),
            result.resultData!!.getStringArrayExtra("$APP_PACKAGE_NAME.PERMISSIONS")!!
                .zip(
                    result.resultData!!.getIntArrayExtra("$APP_PACKAGE_NAME.GRANT_RESULTS")!!
                        .map { it == PackageManager.PERMISSION_GRANTED }
                )
        )
        permissionAndExpectedGrantResults.forEach {
            it.first?.let { permission ->
                assertAppHasPermission(permission, it.second)
            }
        }
    }

    protected inline fun requestAppPermissionsAndAssertResult(
        vararg permissionAndExpectedGrantResults: Pair<String?, Boolean>,
        block: () -> Unit
    ) = requestAppPermissionsAndAssertResult(
        permissionAndExpectedGrantResults.map { it.first }.toTypedArray(),
        permissionAndExpectedGrantResults,
        block
    )

    protected fun clickPermissionRequestAllowButton() {
        if (isAutomotive) {
            click(By.text(getPermissionControllerString(ALLOW_BUTTON_TEXT)))
        } else {
            click(By.res(ALLOW_BUTTON))
        }
    }

    protected fun clickPermissionRequestSettingsLinkAndAllowAlways() {
        clickPermissionRequestSettingsLink()
        eventually({
            clickAllowAlwaysInSettings()
        }, TIMEOUT_MILLIS * 2)
        pressBack()
    }

    protected fun clickAllowAlwaysInSettings() {
        if (isAutomotive || isTv) {
            click(By.text(getPermissionControllerString("app_permission_button_allow_always")))
        } else {
            click(By.res("com.android.permissioncontroller:id/allow_always_radio_button"))
        }
    }

    protected fun clickPermissionRequestAllowForegroundButton(timeoutMillis: Long = 10_000) {
        if (isAutomotive) {
            click(By.text(
                    getPermissionControllerString(ALLOW_FOREGROUND_BUTTON_TEXT)), timeoutMillis)
        } else {
            click(By.res(ALLOW_FOREGROUND_BUTTON), timeoutMillis)
        }
    }

    protected fun clickPermissionRequestDenyButton() {
        if (isAutomotive) {
            click(By.text(getPermissionControllerString(DENY_BUTTON_TEXT)))
        } else {
            click(By.res(DENY_BUTTON))
        }
    }

    protected fun clickPermissionRequestSettingsLinkAndDeny() {
        clickPermissionRequestSettingsLink()
        if (isAutomotive) {
            click(By.text(getPermissionControllerString("app_permission_button_deny")))
        } else {
            click(By.res("com.android.permissioncontroller:id/deny_radio_button"))
        }
        waitForIdle()
        pressBack()
    }

    protected fun clickPermissionRequestSettingsLink() {
        waitForIdle()
        eventually {
            // UiObject2 doesn't expose CharSequence.
            val node = if (isAutomotive) {
                uiAutomation.rootInActiveWindow.findAccessibilityNodeInfosByText(
                        "Allow in settings."
                )[0]
            } else {
                uiAutomation.rootInActiveWindow.findAccessibilityNodeInfosByViewId(
                        "com.android.permissioncontroller:id/detail_message"
                )[0]
            }
            assertTrue(node.isVisibleToUser)
            val text = node.text as Spanned
            val clickableSpan = text.getSpans(0, text.length, ClickableSpan::class.java)[0]
            // We could pass in null here in Java, but we need an instance in Kotlin.
            clickableSpan.onClick(View(context))
        }
        waitForIdle()
    }

    protected fun clickPermissionRequestDenyAndDontAskAgainButton() {
        if (isAutomotive) {
            click(By.text(getPermissionControllerString(DENY_AND_DONT_ASK_AGAIN_BUTTON_TEXT)))
        } else {
            click(By.res(DENY_AND_DONT_ASK_AGAIN_BUTTON))
        }
    }

    // Only used in TV and Watch form factors
    protected fun clickPermissionRequestDontAskAgainButton() =
        click(By.res("com.android.permissioncontroller:id/permission_deny_dont_ask_again_button"))

    protected fun clickPermissionRequestNoUpgradeAndDontAskAgainButton() {
        if (isAutomotive) {
            click(By.text(getPermissionControllerString(NO_UPGRADE_AND_DONT_ASK_AGAIN_BUTTON_TEXT)))
        } else {
            click(By.res(NO_UPGRADE_AND_DONT_ASK_AGAIN_BUTTON))
        }
    }

    protected fun grantAppPermissions(vararg permissions: String, targetSdk: Int = 30) {
        setAppPermissionState(*permissions, state = PermissionState.ALLOWED, isLegacyApp = false,
                targetSdk = targetSdk)
    }

    protected fun revokeAppPermissions(
        vararg permissions: String,
        isLegacyApp: Boolean = false,
        targetSdk: Int = 30
    ) {
        setAppPermissionState(*permissions, state = PermissionState.DENIED,
                isLegacyApp = isLegacyApp, targetSdk = targetSdk)
    }

    private fun setAppPermissionState(
        vararg permissions: String,
        state: PermissionState,
        isLegacyApp: Boolean,
        targetSdk: Int
    ) {
        pressBack()
        pressBack()
        pressBack()
        if (isTv) {
            pressHome()
        }

        // Try multiple times as the AppInfo page might have read stale data
        eventually({
            try {
                // Open the app details settings
                context.startActivity(
                        Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                            data = Uri.fromParts("package", APP_PACKAGE_NAME, null)
                            addCategory(Intent.CATEGORY_DEFAULT)
                            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                            addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)
                        }
                )
                // Open the permissions UI
                click(byTextRes(R.string.permissions).enabled(true))
            } catch (e: Exception) {
                pressBack()
                throw e
            }
        }, TIMEOUT_MILLIS)

        for (permission in permissions) {
            // Find the permission screen
            val permissionLabel = getPermissionLabel(permission)
            click(By.text(permissionLabel))
            val wasGranted = if (isAutomotive) {
                // Automotive doesn't support one time permissions, and thus
                // won't show an "Ask every time" message
                !waitFindObject(byTextRes(R.string.deny)).isChecked
            } else {
                !(waitFindObject(byTextRes(R.string.deny)).isChecked ||
                    (!isLegacyApp && hasAskButton(permission) &&
                        waitFindObject(byTextRes(R.string.ask)).isChecked))
            }
            var alreadyChecked = false
            val button = waitFindObject(
                byTextRes(
                    if (isAutomotive) {
                        // Automotive doesn't support one time permissions, and thus
                        // won't show an "Ask every time" message
                        when (state) {
                            PermissionState.ALLOWED -> R.string.allow
                            PermissionState.DENIED -> R.string.deny
                            PermissionState.DENIED_WITH_PREJUDICE -> R.string.deny
                        }
                    } else {
                        when (state) {
                            PermissionState.ALLOWED ->
                                if (showsForegroundOnlyButton(permission)) {
                                    R.string.allow_foreground
                                } else if (isMediaStorageButton(permission, targetSdk)) {
                                    R.string.allow_media_storage
                                } else if (isAllStorageButton(permission, targetSdk)) {
                                    R.string.allow_external_storage
                                } else {
                                    R.string.allow
                                }
                            PermissionState.DENIED ->
                                if (!isLegacyApp && hasAskButton(permission)) {
                                    R.string.ask
                                } else {
                                    R.string.deny
                                }
                            PermissionState.DENIED_WITH_PREJUDICE -> R.string.deny
                        }
                    }
                )
            )
            alreadyChecked = button.isChecked
            if (!alreadyChecked) {
                button.click()
            }
            if (!alreadyChecked && isLegacyApp && wasGranted) {
                scrollToBottom()
                val resources = context.createPackageContext(
                    packageManager.permissionControllerPackageName, 0
                ).resources
                val confirmTextRes = resources.getIdentifier(
                    "com.android.permissioncontroller:string/grant_dialog_button_deny_anyway", null,
                    null
                )
                val confirmText = resources.getString(confirmTextRes)
                click(byTextStartsWithCaseInsensitive(confirmText))
            }
            pressBack()
        }
        pressBack()
        pressBack()
    }

    private fun getPermissionLabel(permission: String): String {
        val labelResName = permissionToLabelResNameMap[permission]
        assertNotNull("Unknown permission $permission", labelResName)
        val labelRes = platformResources.getIdentifier(labelResName, null, null)
        return platformResources.getString(labelRes)
    }

    private fun hasAskButton(permission: String): Boolean =
        when (permission) {
            android.Manifest.permission.CAMERA,
            android.Manifest.permission.RECORD_AUDIO,
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION -> true
            else -> false
        }

    private fun showsForegroundOnlyButton(permission: String): Boolean =
        when (permission) {
            android.Manifest.permission.CAMERA,
            android.Manifest.permission.RECORD_AUDIO -> true
            else -> false
        }

    private fun isMediaStorageButton(permission: String, targetSdk: Int): Boolean =
            if (isTv) {
                false
            } else {
                when (permission) {
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    android.Manifest.permission.ACCESS_MEDIA_LOCATION ->
                        // Default behavior, can cause issues if OPSTR_LEGACY_STORAGE is set
                        targetSdk >= Build.VERSION_CODES.P
                    else -> false
                }
            }

    private fun isAllStorageButton(permission: String, targetSdk: Int): Boolean =
            if (isTv) {
                false
            } else {
                when (permission) {
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    android.Manifest.permission.ACCESS_MEDIA_LOCATION ->
                        // Default behavior, can cause issues if OPSTR_LEGACY_STORAGE is set
                        targetSdk < Build.VERSION_CODES.P
                    android.Manifest.permission.MANAGE_EXTERNAL_STORAGE -> true
                    else -> false
                }
            }

    private fun scrollToBottom() {
        val scrollable = UiScrollable(UiSelector().scrollable(true)).apply {
            swipeDeadZonePercentage = 0.25
        }
        waitForIdle()
        if (scrollable.exists()) {
            scrollable.flingToEnd(10)
        }
    }

    private fun byTextRes(textRes: Int): BySelector = By.text(context.getString(textRes))

    private fun byTextStartsWithCaseInsensitive(prefix: String): BySelector =
        By.text(Pattern.compile("(?i)^${Pattern.quote(prefix)}.*$"))

    protected fun assertAppHasPermission(permissionName: String, expectPermission: Boolean) {
        assertEquals(
            if (expectPermission) {
                PackageManager.PERMISSION_GRANTED
            } else {
                PackageManager.PERMISSION_DENIED
            },
            packageManager.checkPermission(permissionName, APP_PACKAGE_NAME)
        )
    }

    protected fun assertAppHasCalendarAccess(expectAccess: Boolean) {
        val future = startActivityForFuture(
            Intent().apply {
                component = ComponentName(
                    APP_PACKAGE_NAME, "$APP_PACKAGE_NAME.CheckCalendarAccessActivity"
                )
            }
        )
        val result = future.get(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)
        assertEquals(Activity.RESULT_OK, result.resultCode)
        assertTrue(result.resultData!!.hasExtra("$APP_PACKAGE_NAME.HAS_ACCESS"))
        assertEquals(
            expectAccess,
            result.resultData!!.getBooleanExtra("$APP_PACKAGE_NAME.HAS_ACCESS", false)
        )
    }
}
