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
package com.android.test.notificationprovider

import android.app.Activity
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Bundle

/**
 * Used by NotificationManagerTest for testing policy around content uris.
 */
class RichNotificationActivity : Activity() {
    companion object {
        const val NOTIFICATION_CHANNEL_ID = "NotificationManagerTest"
        const val EXTRA_ACTION = "action"
        const val ACTION_SEND_7 = "send-7"
        const val ACTION_SEND_8 = "send-8"
        const val ACTION_CANCEL_7 = "cancel-7"
        const val ACTION_CANCEL_8 = "cancel-8"
    }

    enum class NotificationPreset(val id: Int) {
        Preset7(7),
        Preset8(8);

        fun build(context: Context): Notification {
            val extras = Bundle()
            extras.putString(Notification.EXTRA_BACKGROUND_IMAGE_URI,
                    "content://com.android.test.notificationprovider.provider/background$id.png")
            return Notification.Builder(context, NOTIFICATION_CHANNEL_ID)
                    .setContentTitle("Rich Notification #$id")
                    .setSmallIcon(android.R.drawable.sym_def_app_icon)
                    .addExtras(extras)
                    .build()
        }
    }

    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity)
        when (intent?.extras?.getString(EXTRA_ACTION)) {
            ACTION_SEND_7 -> sendNotification(NotificationPreset.Preset7)
            ACTION_SEND_8 -> sendNotification(NotificationPreset.Preset8)
            ACTION_CANCEL_7 -> cancelNotification(NotificationPreset.Preset7)
            ACTION_CANCEL_8 -> cancelNotification(NotificationPreset.Preset8)
            else -> {
                // reset both
                cancelNotification(NotificationPreset.Preset7)
                cancelNotification(NotificationPreset.Preset8)
            }
        }
        finish()
    }

    private val notificationManager by lazy { getSystemService(NotificationManager::class.java)!! }

    private fun sendNotification(preset: NotificationPreset) {
        notificationManager.createNotificationChannel(NotificationChannel(NOTIFICATION_CHANNEL_ID,
                "Notifications", NotificationManager.IMPORTANCE_DEFAULT))
        notificationManager.notify(preset.id, preset.build(this))
    }

    private fun cancelNotification(preset: NotificationPreset) {
        notificationManager.cancel(preset.id)
    }
}