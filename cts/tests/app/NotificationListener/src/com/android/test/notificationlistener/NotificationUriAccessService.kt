/*
 * Copyright 2020 The Android Open Source Project
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
package com.android.test.notificationlistener

import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.net.Uri
import android.os.IBinder
import java.io.IOException

class NotificationUriAccessService : Service() {
    private inner class MyNotificationUriAccessService : INotificationUriAccessService.Stub() {
        @Throws(IllegalStateException::class)
        override fun ensureNotificationListenerServiceConnected(ensureConnected: Boolean) {
            val nm = getSystemService(NotificationManager::class.java)!!
            val testListener = TestNotificationListener.componentName
            check(nm.isNotificationListenerAccessGranted(testListener) == ensureConnected) {
                "$testListener has incorrect listener access; expected=$ensureConnected"
            }
            val listener = TestNotificationListener.instance
            if (ensureConnected) {
                check(listener != null) {
                    "$testListener has not been created, but should be connected"
                }
            }
            val isConnected = listener?.isConnected ?: false
            check(isConnected == ensureConnected) {
                "$testListener has incorrect listener connection state; expected=$ensureConnected"
            }
        }

        override fun isFileUriAccessible(uri: Uri?): Boolean {
            try {
                contentResolver.openAssetFile(uri!!, "r", null).use { return true }
            } catch (e: SecurityException) {
                return false
            } catch (e: IOException) {
                throw IllegalStateException("Exception without security error", e)
            }
        }
    }

    private val mBinder = MyNotificationUriAccessService()
    override fun onBind(intent: Intent): IBinder? {
        return mBinder
    }
}