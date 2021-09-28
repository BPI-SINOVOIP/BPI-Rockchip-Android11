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
import kotlin.test.assertTrue

@JvmOverloads
fun orderInsensitiveEquals(
    leftStats: NetworkStats,
    rightStats: NetworkStats,
    compareTime: Boolean = false
): Boolean {
    if (leftStats == rightStats) return true
    if (compareTime && leftStats.getElapsedRealtime() != rightStats.getElapsedRealtime()) {
        return false
    }

    // While operations such as add/subtract will preserve empty entries. This will make
    // the result be hard to verify during test. Remove them before comparing since they
    // are not really affect correctness.
    // TODO (b/152827872): Remove empty entries after addition/subtraction.
    val leftTrimmedEmpty = leftStats.removeEmptyEntries()
    val rightTrimmedEmpty = rightStats.removeEmptyEntries()

    if (leftTrimmedEmpty.size() != rightTrimmedEmpty.size()) return false
    val left = NetworkStats.Entry()
    val right = NetworkStats.Entry()
    // Order insensitive compare.
    for (i in 0 until leftTrimmedEmpty.size()) {
        leftTrimmedEmpty.getValues(i, left)
        val j: Int = rightTrimmedEmpty.findIndexHinted(left.iface, left.uid, left.set, left.tag,
                left.metered, left.roaming, left.defaultNetwork, i)
        if (j == -1) return false
        rightTrimmedEmpty.getValues(j, right)
        if (left != right) return false
    }
    return true
}

/**
 * Assert that two {@link NetworkStats} are equals, assuming the order of the records are not
 * necessarily the same.
 *
 * @note {@code elapsedRealtime} is not compared by default, given that in test cases that is not
 *       usually used.
 */
@JvmOverloads
fun assertNetworkStatsEquals(
    expected: NetworkStats,
    actual: NetworkStats,
    compareTime: Boolean = false
) {
    assertTrue(orderInsensitiveEquals(expected, actual, compareTime),
            "expected: " + expected + " but was: " + actual)
}

/**
 * Assert that after being parceled then unparceled, {@link NetworkStats} is equal to the original
 * object.
 */
fun assertParcelingIsLossless(stats: NetworkStats) {
    assertParcelingIsLossless(stats, { a, b -> orderInsensitiveEquals(a, b) })
}
