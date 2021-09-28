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

package android.net.testutils

import android.net.NetworkStats
import android.net.NetworkStats.SET_DEFAULT
import android.net.NetworkStats.TAG_NONE
import android.os.Build
import com.android.testutils.DevSdkIgnoreRule
import com.android.testutils.orderInsensitiveEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private const val TEST_IFACE = "test0"
private const val TEST_IFACE2 = "test2"
private const val TEST_START = 1194220800000L

@RunWith(JUnit4::class)
class NetworkStatsUtilsTest {
    // This is a unit test for a test utility that uses R APIs
    @Rule @JvmField
    val ignoreRule = DevSdkIgnoreRule(ignoreClassUpTo = Build.VERSION_CODES.Q)

    @Test
    fun testOrderInsensitiveEquals() {
        val testEntry = arrayOf(
                NetworkStats.Entry(TEST_IFACE, 100, SET_DEFAULT, TAG_NONE, 128L, 8L, 0L, 2L, 20L),
                NetworkStats.Entry(TEST_IFACE2, 100, SET_DEFAULT, TAG_NONE, 512L, 32L, 0L, 0L, 0L))

        // Verify equals of empty stats regardless of initial capacity.
        val red = NetworkStats(TEST_START, 0)
        val blue = NetworkStats(TEST_START, 1)
        assertTrue(orderInsensitiveEquals(red, blue))
        assertTrue(orderInsensitiveEquals(blue, red))

        // Verify not equal.
        red.combineValues(testEntry[1])
        blue.combineValues(testEntry[0]).combineValues(testEntry[1])
        assertFalse(orderInsensitiveEquals(red, blue))
        assertFalse(orderInsensitiveEquals(blue, red))

        // Verify equals even if the order of entries are not the same.
        red.combineValues(testEntry[0])
        assertTrue(orderInsensitiveEquals(red, blue))
        assertTrue(orderInsensitiveEquals(blue, red))
    }
}