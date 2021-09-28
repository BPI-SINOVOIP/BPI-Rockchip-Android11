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

package com.android.testutils

import android.net.NetworkStats
import android.net.netstats.provider.INetworkStatsProviderCallback
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.test.fail

private const val DEFAULT_TIMEOUT_MS = 3000L

open class TestableNetworkStatsProviderCbBinder : INetworkStatsProviderCallback.Stub() {
    sealed class CallbackType {
        data class NotifyStatsUpdated(
            val token: Int,
            val ifaceStats: NetworkStats,
            val uidStats: NetworkStats
        ) : CallbackType()
        object NotifyLimitReached : CallbackType()
        object NotifyAlertReached : CallbackType()
        object Unregister : CallbackType()
    }

    private val history = ArrayTrackRecord<CallbackType>().ReadHead()

    override fun notifyStatsUpdated(token: Int, ifaceStats: NetworkStats, uidStats: NetworkStats) {
        history.add(CallbackType.NotifyStatsUpdated(token, ifaceStats, uidStats))
    }

    override fun notifyLimitReached() {
        history.add(CallbackType.NotifyLimitReached)
    }

    override fun notifyAlertReached() {
        history.add(CallbackType.NotifyAlertReached)
    }

    override fun unregister() {
        history.add(CallbackType.Unregister)
    }

    fun expectNotifyStatsUpdated() {
        val event = history.poll(DEFAULT_TIMEOUT_MS)
        assertTrue(event is CallbackType.NotifyStatsUpdated)
    }

    fun expectNotifyStatsUpdated(ifaceStats: NetworkStats, uidStats: NetworkStats) {
        val event = history.poll(DEFAULT_TIMEOUT_MS)!!
        if (event !is CallbackType.NotifyStatsUpdated) {
            throw Exception("Expected NotifyStatsUpdated callback, but got ${event::class}")
        }
        // TODO: verify token.
        assertNetworkStatsEquals(ifaceStats, event.ifaceStats)
        assertNetworkStatsEquals(uidStats, event.uidStats)
    }

    fun expectNotifyLimitReached() =
            assertEquals(CallbackType.NotifyLimitReached, history.poll(DEFAULT_TIMEOUT_MS))

    fun expectNotifyAlertReached() =
            assertEquals(CallbackType.NotifyAlertReached, history.poll(DEFAULT_TIMEOUT_MS))

    // Assert there is no callback in current queue.
    fun assertNoCallback() {
        val cb = history.poll(0)
        cb?.let { fail("Expected no callback but got $cb") }
    }
}
