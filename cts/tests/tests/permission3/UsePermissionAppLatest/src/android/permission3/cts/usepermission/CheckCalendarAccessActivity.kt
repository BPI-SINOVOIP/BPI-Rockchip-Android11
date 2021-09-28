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

package android.permission3.cts.usepermission

import android.app.Activity
import android.content.ContentValues
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.provider.CalendarContract

/**
 * An activity that can check calendar access.
 */
class CheckCalendarAccessActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val supportsRuntimePermissions = applicationInfo.targetSdkVersion >= Build.VERSION_CODES.M
        val hasAccess: Boolean
        val uri = try {
            contentResolver.insert(
                CalendarContract.Calendars.CONTENT_URI, ContentValues().apply {
                    put(CalendarContract.Calendars.NAME, "cts" + System.nanoTime())
                    put(CalendarContract.Calendars.CALENDAR_DISPLAY_NAME, "cts")
                    put(CalendarContract.Calendars.CALENDAR_COLOR, 0xffff0000)
                }
            )!!
        } catch (e: SecurityException) {
            null
        }
        hasAccess = if (uri != null) {
            val count = contentResolver.query(uri, null, null, null).use { it!!.count }
            if (supportsRuntimePermissions) {
                assert(count == 1)
                true
            } else {
                // Without access we're handed back a "fake" Uri that doesn't contain
                // any of the data we tried persisting
                assert(count == 0 || count == 1)
                count == 1
            }
        } else {
            assert(supportsRuntimePermissions)
            try {
                contentResolver.query(CalendarContract.Calendars.CONTENT_URI, null, null, null)
                    .use {}
                error("Expected SecurityException")
            } catch (e: SecurityException) {}
            false
        }
        setResult(RESULT_OK, Intent().apply { putExtra("$packageName.HAS_ACCESS", hasAccess) })
        finish()
    }
}
