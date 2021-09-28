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

package android.app.appops.cts.appthatcanbeforcedintoforegroundstates

import android.app.Notification
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
import android.os.Binder
import android.os.IBinder
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class AppOpsForegroundControlLocationForegroundService : Service() {
    companion object {
        private val lock = ReentrantLock()
        private val condition = lock.newCondition()

        private var instance: AppOpsForegroundControlLocationForegroundService? = null
        private var isStarted = false

        fun waitUntilStarted() {
            lock.withLock {
                while (!isStarted) {
                    condition.await()
                }
            }
        }

        fun stop() {
            instance?.stopSelf()

            lock.withLock {
                isStarted = false
                condition.signalAll()
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? {
        return Binder()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        ensureNotificationChannel(this)
        startForeground(2, Notification.Builder(this, NOTIFICATION_CHANNEL_ID)
                .setSmallIcon(android.R.drawable.ic_info)
                .setContentTitle("Location foreground service").build(),
                FOREGROUND_SERVICE_TYPE_LOCATION)

        lock.withLock {
            isStarted = true
            instance = this
            condition.signalAll()
        }

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onUnbind(intent: Intent?): Boolean {
        lock.withLock {
            isStarted = false
            instance = null
            condition.signalAll()
        }

        return super.onUnbind(intent)
    }
}