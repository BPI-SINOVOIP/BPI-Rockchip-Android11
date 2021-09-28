/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.permission3.cts.permissionpolicy

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.PermissionInfo
import android.os.Bundle

/**
 * An activity that can test platform permission protection flags.
 */
class TestProtectionFlagsActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setResult(
            RESULT_OK, Intent().apply {
                putExtra("$packageName.ERROR_MESSAGE", getProtectionFlagsErrorMessage())
            }
        )
        finish()
    }

    private fun getProtectionFlagsErrorMessage(): String {
        val packageInfo = packageManager.getPackageInfo("android", PackageManager.GET_PERMISSIONS)
        val errorMessageBuilder = StringBuilder()
        for (declaredPermissionInfo in packageInfo.permissions) {
            val permissionInfo = packageManager.getPermissionInfo(declaredPermissionInfo.name, 0)
            val protection = permissionInfo.protectionLevel and (
                PermissionInfo.PROTECTION_NORMAL
                    or PermissionInfo.PROTECTION_DANGEROUS
                    or PermissionInfo.PROTECTION_SIGNATURE
                )
            val protectionFlags = permissionInfo.protectionLevel and protection.inv()
            if ((protection == PermissionInfo.PROTECTION_NORMAL ||
                    protection == PermissionInfo.PROTECTION_DANGEROUS) && protectionFlags != 0) {
                errorMessageBuilder.apply {
                    if (isNotEmpty()) {
                        append("\n")
                    }
                    append("Cannot add protection flags ${protectionFlagsToString(protectionFlags)
                    } to a ${protectionToString(protection)} protection permission: ${
                    permissionInfo.name}")
                }
            }
        }
        return errorMessageBuilder.toString()
    }

    private fun protectionToString(protection: Int): String =
        when (protection) {
            PermissionInfo.PROTECTION_NORMAL -> "normal"
            PermissionInfo.PROTECTION_DANGEROUS -> "dangerous"
            PermissionInfo.PROTECTION_SIGNATURE -> "signature"
            else -> Integer.toHexString(protection)
        }

    private fun protectionFlagsToString(protectionFlags: Int): String {
        var unknownProtectionFlags = protectionFlags
        val stringBuilder = StringBuilder()
        val appendProtectionFlag = { protectionFlag: Int, protectionFlagString: String ->
            if (unknownProtectionFlags and protectionFlag == protectionFlag) {
                stringBuilder.apply {
                    if (isNotEmpty()) {
                        append("|")
                    }
                    append(protectionFlagString)
                }
                unknownProtectionFlags = unknownProtectionFlags and protectionFlag.inv()
            }
        }
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_PRIVILEGED, "privileged")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_DEVELOPMENT, "development")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_APPOP, "appop")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_PRE23, "pre23")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_INSTALLER, "installer")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_VERIFIER, "verifier")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_PREINSTALLED, "preinstalled")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_SETUP, "setup")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_INSTANT, "instant")
        appendProtectionFlag(PermissionInfo.PROTECTION_FLAG_RUNTIME_ONLY, "runtimeOnly")
        if (unknownProtectionFlags != 0) {
            appendProtectionFlag(
                unknownProtectionFlags, Integer.toHexString(unknownProtectionFlags)
            )
        }
        return stringBuilder.toString()
    }
}
