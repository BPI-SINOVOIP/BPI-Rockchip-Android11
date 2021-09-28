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

package android.cts.instantapp.resolver

import android.content.Intent
import android.content.pm.InstantAppRequestInfo
import android.os.Parcel
import com.google.common.truth.Truth.assertThat
import org.junit.Test
import java.util.UUID

class InstantAppRequestInfoTest {

    private val intent = Intent(Intent.ACTION_VIEW)
    private val hostDigestPrefix = intArrayOf(1)
    private val userHandle = android.os.Process.myUserHandle()
    private val isRequesterInstantApp = false
    private val token = UUID.randomUUID().toString()

    private val info = InstantAppRequestInfo(intent, hostDigestPrefix, userHandle,
            isRequesterInstantApp, token)

    @Test
    fun values() {
        assertValues(info)
    }

    @Test
    fun parcel() {
        Parcel.obtain()
                .run {
                    info.writeToParcel(this, 0)
                    setDataPosition(0)
                    InstantAppRequestInfo.CREATOR.createFromParcel(this)
                            .also { recycle() }
                }
                .run(::assertValues)
    }

    private fun assertValues(info: InstantAppRequestInfo) {
        assertThat(info.intent.filterEquals(intent)).isTrue()
        assertThat(info.hostDigestPrefix).isEqualTo(hostDigestPrefix)
        assertThat(info.userHandle).isEqualTo(userHandle)
        assertThat(info.isRequesterInstantApp).isEqualTo(isRequesterInstantApp)
        assertThat(info.token).isEqualTo(token)
    }
}
