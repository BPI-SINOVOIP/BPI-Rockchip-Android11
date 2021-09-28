/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.server.net.integrationtests

import android.os.Parcel
import android.os.Parcelable

data class HttpResponse(
    val requestUrl: String,
    val responseCode: Int,
    val contentLength: Long,
    val redirectUrl: String?
) : Parcelable {
    constructor(p: Parcel): this(p.readString(), p.readInt(), p.readLong(), p.readString())

    override fun writeToParcel(dest: Parcel, flags: Int) {
        with(dest) {
            writeString(requestUrl)
            writeInt(responseCode)
            writeLong(contentLength)
            writeString(redirectUrl)
        }
    }

    override fun describeContents() = 0
    companion object CREATOR : Parcelable.Creator<HttpResponse> {
        override fun createFromParcel(source: Parcel) = HttpResponse(source)
        override fun newArray(size: Int) = arrayOfNulls<HttpResponse?>(size)
    }
}