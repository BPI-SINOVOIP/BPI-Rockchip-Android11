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

package android.permission3.cts.usepermission

import android.app.IntentService
import android.content.Intent
import android.os.ResultReceiver

/**
 * A service that can check if a permission is currently granted
 */
class CheckPermissionService : IntentService(CheckPermissionService::class.java.simpleName) {
    companion object {
        private const val TEST_PACKAGE_NAME = "android.permission3.cts"
    }

    override fun onHandleIntent(intent: Intent?) {
        val extras = intent!!.extras!!
        // Load bundle with context of client package so ResultReceiver class can be resolved
        val testContext = createPackageContext(
            TEST_PACKAGE_NAME, CONTEXT_INCLUDE_CODE or CONTEXT_IGNORE_SECURITY
        )
        extras.classLoader = testContext.classLoader
        val result = extras.getParcelable<ResultReceiver>("$packageName.RESULT")!!
        val permission = extras.getString("$packageName.PERMISSION")!!
        result.send(checkSelfPermission(permission), null)
    }
}
