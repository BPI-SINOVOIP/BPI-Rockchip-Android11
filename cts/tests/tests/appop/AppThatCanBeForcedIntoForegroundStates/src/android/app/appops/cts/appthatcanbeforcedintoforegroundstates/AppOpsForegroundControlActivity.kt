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

import android.app.Activity
import android.os.Bundle
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class AppOpsForegroundControlActivity : Activity() {
    companion object {
        private val lock = ReentrantLock()
        private val condition = lock.newCondition()

        private var instance: AppOpsForegroundControlActivity? = null
        private var isCreated = false
        private var isForeground = false

        fun waitUntilCreated() {
            lock.withLock {
                while (!isCreated) {
                    condition.await()
                }
            }
        }

        fun waitUntilBackground() {
            lock.withLock {
                while (!isCreated || isForeground) {
                    condition.await()
                }
            }
        }

        fun waitUntilForeground() {
            lock.withLock {
                while (!isForeground) {
                    condition.await()
                }
            }
        }

        fun finish() {
            lock.withLock {
                instance?.finish()

                while (isCreated) {
                    condition.await()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        lock.withLock {
            instance = this
            isCreated = true
            condition.signalAll()
        }
    }

    override fun onResume() {
        super.onResume()

        lock.withLock {
            isForeground = true
            condition.signalAll()
        }
    }

    override fun onPause() {
        super.onPause()

        lock.withLock {
            isForeground = false
            condition.signalAll()
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        lock.withLock {
            instance = null
            isCreated = false
            condition.signalAll()
        }
    }
}