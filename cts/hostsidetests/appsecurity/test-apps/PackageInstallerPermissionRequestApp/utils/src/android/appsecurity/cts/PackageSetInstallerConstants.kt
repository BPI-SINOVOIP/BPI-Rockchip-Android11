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

// Placed in CtsPkgInstallerPermRequestApp sources as its manifest declares what permissions to test
object PackageSetInstallerConstants {

    // The app that will be attempting to take over as installer and grant itself permissions
    const val TARGET_APK = "CtsPkgInstallerPermRequestApp.apk"
    const val TARGET_PKG = "com.android.cts.packageinstallerpermissionrequestapp"

    // The app acting as the original installer who can restrict permissions
    const val WHITELIST_APK = "CtsPkgInstallerPermWhitelistApp.apk"
    const val WHITELIST_PKG = "com.android.cts.packageinstallerpermissionwhitelistapp"

    const val PERMISSION_HARD_RESTRICTED = "android.permission.SEND_SMS"
    const val PERMISSION_NOT_RESTRICTED = "android.permission.ACCESS_COARSE_LOCATION"
    const val PERMISSION_IMMUTABLY_SOFT_RESTRICTED = "android.permission.READ_EXTERNAL_STORAGE"
    const val PERMISSION_KEY = "permission"

    // Whether setInstallerPackageName should throw
    const val SHOULD_THROW_EXCEPTION_KEY = "shouldThrowException"

    // Whether or not some boolean return method call should succeed
    const val SHOULD_SUCCEED_KEY = "shouldSucceed"

    // PackageManagerService.THROW_EXCEPTION_ON_REQUIRE_INSTALL_PACKAGES_TO_ADD_INSTALLER_PACKAGE
    const val CHANGE_ID = 150857253
}
