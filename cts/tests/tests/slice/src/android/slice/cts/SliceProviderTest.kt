/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package android.slice.cts

import android.app.slice.Slice
import android.app.slice.SliceSpec
import android.content.ContentResolver
import android.net.Uri
import android.os.Bundle

import android.platform.test.annotations.SecurityTest
import androidx.test.rule.ActivityTestRule
import androidx.test.runner.AndroidJUnit4
import org.junit.Before

import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

private const val VALID_AUTHORITY = "android.slice.cts"
private const val SUSPICIOUS_AUTHORITY = "com.suspicious.www"
private const val ACTION_BLUETOOTH = "/action/bluetooth"
private const val VALID_BASE_URI_STRING = "content://$VALID_AUTHORITY"
private const val VALID_ACTION_URI_STRING = "content://$VALID_AUTHORITY$ACTION_BLUETOOTH"
private const val SHADY_ACTION_URI_STRING = "content://$SUSPICIOUS_AUTHORITY$ACTION_BLUETOOTH"

@RunWith(AndroidJUnit4::class)
class SliceProviderTest {

    @Rule @JvmField var activityTestRule = ActivityTestRule(Launcher::class.java)

    private val validBaseUri = Uri.parse(VALID_BASE_URI_STRING)
    private val validActionUri = Uri.parse(VALID_ACTION_URI_STRING)
    private val shadyActionUri = Uri.parse(SHADY_ACTION_URI_STRING)

    private lateinit var contentResolver: ContentResolver

    @Before
    fun setUp() {
        contentResolver = activityTestRule.activity.contentResolver
    }

    @Test
    @SecurityTest(minPatchLevel = "2019-11-01")
    fun testCallSliceUri_ValidAuthority() {
        doQuery(validActionUri)
    }

    @Test(expected = SecurityException::class)
    @SecurityTest(minPatchLevel = "2019-11-01")
    fun testCallSliceUri_ShadyAuthority() {
        doQuery(shadyActionUri)
    }

    private fun doQuery(actionUri: Uri): Slice {
        val extras = Bundle().apply {
            putParcelable("slice_uri", actionUri)
            putParcelableArrayList("supported_specs", ArrayList(listOf(
                    SliceSpec("androidx.slice.LIST", 1),
                    SliceSpec("androidx.app.slice.BASIC", 1),
                    SliceSpec("androidx.slice.BASIC", 1),
                    SliceSpec("androidx.app.slice.LIST", 1)
            )))
        }
        val result = contentResolver.call(
                validBaseUri,
                SliceProvider.METHOD_SLICE,
                null,
                extras
        )
        return result.getParcelable(SliceProvider.EXTRA_SLICE)
    }
}
