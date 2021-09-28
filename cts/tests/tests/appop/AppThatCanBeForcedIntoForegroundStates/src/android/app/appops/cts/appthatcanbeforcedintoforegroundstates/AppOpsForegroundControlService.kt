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

import android.app.Service
import android.app.appops.cts.IAppOpsForegroundControlService
import android.content.Intent
import android.os.IBinder

class AppOpsForegroundControlService : Service() {
    override fun onBind(intent: Intent?): IBinder? {
        return object : IAppOpsForegroundControlService.Stub() {
            override fun waitUntilCreated() {
                AppOpsForegroundControlActivity.waitUntilCreated()
            }

            override fun waitUntilBackground() {
                AppOpsForegroundControlActivity.waitUntilBackground()
            }

            override fun waitUntilForeground() {
                AppOpsForegroundControlActivity.waitUntilForeground()
            }

            override fun finishActivity() {
                AppOpsForegroundControlActivity.finish()
            }

            override fun waitUntilForegroundServiceStarted() {
                AppOpsForegroundControlForegroundService.waitUntilStarted()
            }

            override fun stopForegroundService() {
                AppOpsForegroundControlForegroundService.stop()
            }

            override fun waitUntilLocationForegroundServiceStarted() {
                AppOpsForegroundControlLocationForegroundService.waitUntilStarted()
            }

            override fun stopLocationForegroundService() {
                AppOpsForegroundControlLocationForegroundService.stop()
            }

            override fun cleanup() {
                AppOpsForegroundControlActivity.finish()
                AppOpsForegroundControlForegroundService.stop()
                AppOpsForegroundControlLocationForegroundService.stop()
            }
        }
    }
}
