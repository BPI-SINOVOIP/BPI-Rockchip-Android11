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
 * limitations under the License.
 */

package android.net.testutils

import android.os.HandlerThread
import com.android.testutils.waitForIdle
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import kotlin.test.assertEquals

const val ATTEMPTS = 50 // Causes the test to take about 150ms on aosp_crosshatch-eng.
const val TIMEOUT_MS = 200

@RunWith(JUnit4::class)
class HandlerUtilsTest {
    @Test
    fun testWaitForIdle() {
        val handlerThread = HandlerThread("testHandler").apply { start() }

        // Tests that waitForIdle can be called many times without ill impact if the service is
        // already idle.
        repeat(ATTEMPTS) {
            handlerThread.waitForIdle(TIMEOUT_MS)
        }

        // Tests that calling waitForIdle waits for messages to be processed. Use both an
        // inline runnable that's instantiated at each loop run and a runnable that's instantiated
        // once for all.
        val tempRunnable = object : Runnable {
            // Use StringBuilder preferentially to StringBuffer because StringBuilder is NOT
            // thread-safe. It's part of the point that both runnables run on the same thread
            // so if anything is wrong in that space it's better to opportunistically use a class
            // where things might go wrong, even if there is no guarantee of failure.
            var memory = StringBuilder()
            override fun run() {
                memory.append("b")
            }
        }
        repeat(ATTEMPTS) { i ->
            handlerThread.threadHandler.post({ tempRunnable.memory.append("a"); })
            handlerThread.threadHandler.post(tempRunnable)
            handlerThread.waitForIdle(TIMEOUT_MS)
            assertEquals(tempRunnable.memory.toString(), "ab".repeat(i + 1))
        }
    }
}
