/*
 * Copyright 2018 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package trebuchet.model.fragments

import trebuchet.model.InvalidId
import trebuchet.model.base.Slice

data class AsyncSlice(
    override val startTime: Double,
    override var endTime: Double,
    override val name: String,
    val cookie: Int,
    override var didNotFinish: Boolean,
    val startThreadId: Int,
    var endThreadId: Int
) : Slice

class AsyncSlicesBuilder {
    val openSlices = mutableMapOf<String, AsyncSlice>()
    val asyncSlices = mutableListOf<AsyncSlice>()

    private fun key(name: String, cookie: Int): String {
        return "$name:$cookie"
    }

    fun openAsyncSlice(pid: Int, name: String, cookie: Int, startTime: Double) {
        openSlices[key(name, cookie)] = AsyncSlice(startTime, Double.NaN, name, cookie,
            true, pid, InvalidId)
    }

    fun closeAsyncSlice(pid: Int, name: String, cookie: Int, endTime: Double) {
        val slice = openSlices.remove(key(name, cookie)) ?: return
        slice.endTime = endTime
        slice.didNotFinish = false
        slice.endThreadId = pid
        asyncSlices.add(slice)
    }

    fun autoCloseOpenSlices(maxTimestamp: Double) {
        for (slice in openSlices.values) {
            slice.endTime = maxTimestamp
            slice.didNotFinish = true
            slice.endThreadId = InvalidId
            asyncSlices.add(slice)
        }
        openSlices.clear()
    }

}