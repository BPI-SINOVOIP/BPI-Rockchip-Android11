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

package com.android.testutils

import android.net.DnsResolver
import android.net.InetAddresses
import android.os.Looper
import android.os.Handler
import com.android.internal.annotations.GuardedBy
import java.net.InetAddress
import java.util.concurrent.Executor
import org.mockito.invocation.InvocationOnMock
import org.mockito.Mockito.any
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doAnswer

const val TYPE_UNSPECIFIED = -1
// TODO: Integrate with NetworkMonitorTest.
class FakeDns(val mockResolver: DnsResolver) {
    class DnsEntry(val hostname: String, val type: Int, val addresses: List<InetAddress>) {
        fun match(host: String, type: Int) = hostname.equals(host) && type == type
    }

    @GuardedBy("answers")
    val answers = ArrayList<DnsEntry>()

    fun getAnswer(hostname: String, type: Int): DnsEntry? = synchronized(answers) {
        return answers.firstOrNull { it.match(hostname, type) }
    }

    fun setAnswer(hostname: String, answer: Array<String>, type: Int) = synchronized(answers) {
        val ans = DnsEntry(hostname, type, generateAnswer(answer))
        // Replace or remove the existing one.
        when (val index = answers.indexOfFirst { it.match(hostname, type) }) {
            -1 -> answers.add(ans)
            else -> answers[index] = ans
        }
    }

    private fun generateAnswer(answer: Array<String>) =
            answer.filterNotNull().map { InetAddresses.parseNumericAddress(it) }

    fun startMocking() {
        // Mock DnsResolver.query() w/o type
        doAnswer {
            mockAnswer(it, 1, -1, 3, 5)
        }.`when`(mockResolver).query(any() /* network */, any() /* domain */, anyInt() /* flags */,
                any() /* executor */, any() /* cancellationSignal */, any() /*callback*/)
        // Mock DnsResolver.query() w/ type
        doAnswer {
            mockAnswer(it, 1, 2, 4, 6)
        }.`when`(mockResolver).query(any() /* network */, any() /* domain */, anyInt() /* nsType */,
                anyInt() /* flags */, any() /* executor */, any() /* cancellationSignal */,
        any() /*callback*/)
    }

    private fun mockAnswer(
        it: InvocationOnMock,
        posHos: Int,
        posType: Int,
        posExecutor: Int,
        posCallback: Int
    ) {
        val hostname = it.arguments[posHos] as String
        val executor = it.arguments[posExecutor] as Executor
        val callback = it.arguments[posCallback] as DnsResolver.Callback<List<InetAddress>>
        var type = if (posType != -1) it.arguments[posType] as Int else TYPE_UNSPECIFIED
        val answer = getAnswer(hostname, type)

        if (!answer?.addresses.isNullOrEmpty()) {
            Handler(Looper.getMainLooper()).post({ executor.execute({
                    callback.onAnswer(answer?.addresses, 0); }) })
        }
    }

    /** Clears all entries. */
    fun clearAll() = synchronized(answers) {
        answers.clear()
    }
}
