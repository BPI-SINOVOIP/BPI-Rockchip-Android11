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

package com.android.compatibility.common.util

import android.app.Activity
import android.content.Intent
import java.util.concurrent.CompletableFuture
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

/**
 * An [Activity] that exposes a special [startActivityForResult],
 * returning future resultCode as a [CompletableFuture]
 */
class FutureResultActivity : Activity() {

    companion object {

        /** requestCode -> Future<resultCode> */
        private val requests = ConcurrentHashMap<Int, CompletableFuture<Int>>()
        private val nextRequestCode = AtomicInteger(0)

        fun doAndAwaitStart(act: () -> Unit): CompletableFuture<Int> {
            val requestCode = nextRequestCode.get()
            act()
            PollingCheck.waitFor(60_000) {
                nextRequestCode.get() >= requestCode + 1
            }
            return requests[requestCode]!!
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        requests[requestCode]!!.complete(resultCode)
    }

    fun startActivityForResult(intent: Intent): CompletableFuture<Int> {
        val requestCode = nextRequestCode.getAndIncrement()
        val future = CompletableFuture<Int>()
        requests[requestCode] = future
        startActivityForResult(intent, requestCode)
        return future
    }
}
