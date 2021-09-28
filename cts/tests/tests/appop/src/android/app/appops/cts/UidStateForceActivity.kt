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

package android.app.appops.cts

import android.app.Activity
import android.os.Bundle
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class UidStateForceActivity : Activity() {
    companion object {
        private val lock = ReentrantLock()
        private val condition = lock.newCondition()
        private var isActivityResumed = false
        private var isActivityDestroyed = false;

        var instance: UidStateForceActivity? = null

        fun waitForResumed() {
            lock.withLock {
                while (!isActivityResumed) {
                    condition.await()
                }
            }
        }

        fun waitForDestroyed() {
            lock.withLock {
                while (!isActivityDestroyed) {
                    condition.await()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        instance = this
    }

    override fun onResume() {
        super.onResume()

        lock.withLock {
            isActivityResumed = true
            condition.signalAll()
        }
    }

    override fun onPause() {
        super.onPause()

        lock.withLock {
            isActivityResumed = false
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        lock.withLock {
            isActivityDestroyed = true
            condition.signalAll()
        }
    }
}
