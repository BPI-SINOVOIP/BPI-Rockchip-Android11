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

import com.android.testutils.ArrayTrackRecord
import com.android.testutils.ConcurrentIntepreter
import com.android.testutils.InterpretException
import com.android.testutils.InterpretMatcher
import com.android.testutils.SyntaxException
import com.android.testutils.TrackRecord
import com.android.testutils.__FILE__
import com.android.testutils.__LINE__
import com.android.testutils.intArg
import com.android.testutils.strArg
import com.android.testutils.timeArg
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import java.util.concurrent.CyclicBarrier
import java.util.concurrent.TimeUnit
import kotlin.system.measureTimeMillis
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.test.fail

val TEST_VALUES = listOf(4, 13, 52, 94, 41, 68, 11, 13, 51, 0, 91, 94, 33, 98, 14)
const val ABSENT_VALUE = 2
// Caution in changing these : some tests rely on the fact that TEST_TIMEOUT > 2 * SHORT_TIMEOUT
// and LONG_TIMEOUT > 2 * TEST_TIMEOUT
const val SHORT_TIMEOUT = 40L // ms
const val TEST_TIMEOUT = 200L // ms
const val LONG_TIMEOUT = 5000L // ms

@RunWith(JUnit4::class)
class TrackRecordTest {
    @Test
    fun testAddAndSizeAndGet() {
        val repeats = 22 // arbitrary
        val record = ArrayTrackRecord<Int>()
        assertEquals(0, record.size)
        repeat(repeats) { i -> record.add(i + 2) }
        assertEquals(repeats, record.size)
        record.add(2)
        assertEquals(repeats + 1, record.size)

        assertEquals(11, record[9])
        assertEquals(11, record.getOrNull(9))
        assertEquals(2, record[record.size - 1])
        assertEquals(2, record.getOrNull(record.size - 1))

        assertFailsWith<IndexOutOfBoundsException> { record[800] }
        assertFailsWith<IndexOutOfBoundsException> { record[-1] }
        assertFailsWith<IndexOutOfBoundsException> { record[repeats + 1] }
        assertNull(record.getOrNull(800))
        assertNull(record.getOrNull(-1))
        assertNull(record.getOrNull(repeats + 1))
        assertNull(record.getOrNull(800) { true })
        assertNull(record.getOrNull(-1) { true })
        assertNull(record.getOrNull(repeats + 1) { true })
    }

    @Test
    fun testIndexOf() {
        val record = ArrayTrackRecord<Int>()
        TEST_VALUES.forEach { record.add(it) }
        with(record) {
            assertEquals(9, indexOf(0))
            assertEquals(9, lastIndexOf(0))
            assertEquals(1, indexOf(13))
            assertEquals(7, lastIndexOf(13))
            assertEquals(3, indexOf(94))
            assertEquals(11, lastIndexOf(94))
            assertEquals(-1, indexOf(ABSENT_VALUE))
            assertEquals(-1, lastIndexOf(ABSENT_VALUE))
        }
    }

    @Test
    fun testContains() {
        val record = ArrayTrackRecord<Int>()
        TEST_VALUES.forEach { record.add(it) }
        TEST_VALUES.forEach { assertTrue(record.contains(it)) }
        assertFalse(record.contains(ABSENT_VALUE))
        assertTrue(record.containsAll(TEST_VALUES))
        assertTrue(record.containsAll(TEST_VALUES.sorted()))
        assertTrue(record.containsAll(TEST_VALUES.sortedDescending()))
        assertTrue(record.containsAll(TEST_VALUES.distinct()))
        assertTrue(record.containsAll(TEST_VALUES.subList(0, TEST_VALUES.size / 2)))
        assertTrue(record.containsAll(TEST_VALUES.subList(0, TEST_VALUES.size / 2).sorted()))
        assertTrue(record.containsAll(listOf()))
        assertFalse(record.containsAll(listOf(ABSENT_VALUE)))
        assertFalse(record.containsAll(TEST_VALUES + listOf(ABSENT_VALUE)))
    }

    @Test
    fun testEmpty() {
        val record = ArrayTrackRecord<Int>()
        assertTrue(record.isEmpty())
        record.add(1)
        assertFalse(record.isEmpty())
    }

    @Test
    fun testIterate() {
        val record = ArrayTrackRecord<Int>()
        record.forEach { fail("Expected nothing to iterate") }
        TEST_VALUES.forEach { record.add(it) }
        // zip relies on the iterator (this calls extension function Iterable#zip(Iterable))
        record.zip(TEST_VALUES).forEach { assertEquals(it.first, it.second) }
        // Also test reverse iteration (to test hasPrevious() and friends)
        record.reversed().zip(TEST_VALUES.reversed()).forEach { assertEquals(it.first, it.second) }
    }

    @Test
    fun testIteratorIsSnapshot() {
        val record = ArrayTrackRecord<Int>()
        TEST_VALUES.forEach { record.add(it) }
        val iterator = record.iterator()
        val expectedSize = record.size
        record.add(ABSENT_VALUE)
        record.add(ABSENT_VALUE)
        var measuredSize = 0
        iterator.forEach {
            ++measuredSize
            assertNotEquals(ABSENT_VALUE, it)
        }
        assertEquals(expectedSize, measuredSize)
    }

    @Test
    fun testSublist() {
        val record = ArrayTrackRecord<Int>()
        TEST_VALUES.forEach { record.add(it) }
        assertEquals(record.subList(3, record.size - 3),
                TEST_VALUES.subList(3, TEST_VALUES.size - 3))
    }

    fun testPollReturnsImmediately(record: TrackRecord<Int>) {
        record.add(4)
        val elapsed = measureTimeMillis { assertEquals(4, record.poll(LONG_TIMEOUT, 0)) }
        // Should not have waited at all, in fact.
        assertTrue(elapsed < LONG_TIMEOUT)
        record.add(7)
        record.add(9)
        // Can poll multiple times for the same position, in whatever order
        assertEquals(9, record.poll(0, 2))
        assertEquals(7, record.poll(Long.MAX_VALUE, 1))
        assertEquals(9, record.poll(0, 2))
        assertEquals(4, record.poll(0, 0))
        assertEquals(9, record.poll(0, 2) { it > 5 })
        assertEquals(7, record.poll(0, 0) { it > 5 })
    }

    @Test
    fun testPollReturnsImmediately() {
        testPollReturnsImmediately(ArrayTrackRecord())
        testPollReturnsImmediately(ArrayTrackRecord<Int>().newReadHead())
    }

    @Test
    fun testPollTimesOut() {
        val record = ArrayTrackRecord<Int>()
        var delay = measureTimeMillis { assertNull(record.poll(SHORT_TIMEOUT, 0)) }
        assertTrue(delay >= SHORT_TIMEOUT, "Delay $delay < $SHORT_TIMEOUT")
        delay = measureTimeMillis { assertNull(record.poll(SHORT_TIMEOUT, 0) { it < 10 }) }
        assertTrue(delay >= SHORT_TIMEOUT)
    }

    @Test
    fun testPollWakesUp() {
        val record = ArrayTrackRecord<Int>()
        val barrier = CyclicBarrier(2)
        Thread {
            barrier.await(LONG_TIMEOUT, TimeUnit.MILLISECONDS) // barrier 1
            barrier.await() // barrier 2
            Thread.sleep(SHORT_TIMEOUT * 2)
            record.add(31)
        }.start()
        barrier.await() // barrier 1
        // Should find the element in more than SHORT_TIMEOUT but less than TEST_TIMEOUT
        var delay = measureTimeMillis {
            barrier.await() // barrier 2
            assertEquals(31, record.poll(TEST_TIMEOUT, 0))
        }
        assertTrue(delay in SHORT_TIMEOUT..TEST_TIMEOUT)
        // Polling for an element already added in anothe thread (pos 0) : should return immediately
        delay = measureTimeMillis { assertEquals(31, record.poll(TEST_TIMEOUT, 0)) }
        assertTrue(delay < TEST_TIMEOUT, "Delay $delay > $TEST_TIMEOUT")
        // Waiting for an element that never comes
        delay = measureTimeMillis { assertNull(record.poll(SHORT_TIMEOUT, 1)) }
        assertTrue(delay >= SHORT_TIMEOUT, "Delay $delay < $SHORT_TIMEOUT")
        // Polling for an element that doesn't match what is already there
        delay = measureTimeMillis { assertNull(record.poll(SHORT_TIMEOUT, 0) { it < 10 }) }
        assertTrue(delay >= SHORT_TIMEOUT)
    }

    // Just make sure the interpreter actually throws an exception when the spec
    // does not conform to the behavior. The interpreter is just a tool to test a
    // tool used for a tool for test, let's not have hundreds of tests for it ;
    // if it's broken one of the tests using it will break.
    @Test
    fun testInterpreter() {
        val interpretLine = __LINE__ + 2
        try {
            TRTInterpreter.interpretTestSpec(useReadHeads = true, spec = """
                add(4) | poll(1, 0) = 5
            """)
            fail("This spec should have thrown")
        } catch (e: InterpretException) {
            assertTrue(e.cause is AssertionError)
            assertEquals(interpretLine + 1, e.stackTrace[0].lineNumber)
            assertTrue(e.stackTrace[0].fileName.contains(__FILE__))
            assertTrue(e.stackTrace[0].methodName.contains("testInterpreter"))
            assertTrue(e.stackTrace[0].methodName.contains("thread1"))
        }
    }

    @Test
    fun testMultipleAdds() {
        TRTInterpreter.interpretTestSpec(useReadHeads = false, spec = """
            add(2)         |                |                |
                           | add(4)         |                |
                           |                | add(6)         |
                           |                |                | add(8)
            poll(0, 0) = 2 time 0..1 | poll(0, 0) = 2 | poll(0, 0) = 2 | poll(0, 0) = 2
            poll(0, 1) = 4 time 0..1 | poll(0, 1) = 4 | poll(0, 1) = 4 | poll(0, 1) = 4
            poll(0, 2) = 6 time 0..1 | poll(0, 2) = 6 | poll(0, 2) = 6 | poll(0, 2) = 6
            poll(0, 3) = 8 time 0..1 | poll(0, 3) = 8 | poll(0, 3) = 8 | poll(0, 3) = 8
        """)
    }

    @Test
    fun testConcurrentAdds() {
        TRTInterpreter.interpretTestSpec(useReadHeads = false, spec = """
            add(2)             | add(4)             | add(6)             | add(8)
            add(1)             | add(3)             | add(5)             | add(7)
            poll(0, 1) is even | poll(0, 0) is even | poll(0, 3) is even | poll(0, 2) is even
            poll(0, 5) is odd  | poll(0, 4) is odd  | poll(0, 7) is odd  | poll(0, 6) is odd
        """)
    }

    @Test
    fun testMultiplePoll() {
        TRTInterpreter.interpretTestSpec(useReadHeads = false, spec = """
            add(4)         | poll(1, 0) = 4
                           | poll(0, 1) = null time 0..1
                           | poll(1, 1) = null time 1..2
            sleep; add(7)  | poll(2, 1) = 7 time 1..2
            sleep; add(18) | poll(2, 2) = 18 time 1..2
        """)
    }

    @Test
    fun testMultiplePollWithPredicate() {
        TRTInterpreter.interpretTestSpec(useReadHeads = false, spec = """
                     | poll(1, 0) = null          | poll(1, 0) = null
            add(6)   | poll(1, 0) = 6             |
            add(11)  | poll(1, 0) { > 20 } = null | poll(1, 0) { = 11 } = 11
                     | poll(1, 0) { > 8 } = 11    |
        """)
    }

    @Test
    fun testMultipleReadHeads() {
        TRTInterpreter.interpretTestSpec(useReadHeads = true, spec = """
                   | poll() = null | poll() = null | poll() = null
            add(5) |               | poll() = 5    |
                   | poll() = 5    |               |
            add(8) | poll() = 8    | poll() = 8    |
                   |               |               | poll() = 5
                   |               |               | poll() = 8
                   |               |               | poll() = null
                   |               | poll() = null |
        """)
    }

    @Test
    fun testReadHeadPollWithPredicate() {
        TRTInterpreter.interpretTestSpec(useReadHeads = true, spec = """
            add(5)  | poll() { < 0 } = null
                    | poll() { > 5 } = null
            add(10) |
                    | poll() { = 5 } = null   // The "5" was skipped in the previous line
            add(15) | poll() { > 8 } = 15     // The "10" was skipped in the previous line
                    | poll(1, 0) { > 8 } = 10 // 10 is the first element after pos 0 matching > 8
        """)
    }

    @Test
    fun testPollImmediatelyAdvancesReadhead() {
        TRTInterpreter.interpretTestSpec(useReadHeads = true, spec = """
            add(1)                  | add(2)              | add(3)   | add(4)
            mark = 0                | poll(0) { > 3 } = 4 |          |
            poll(0) { > 10 } = null |                     |          |
            mark = 4                |                     |          |
            poll() = null           |                     |          |
        """)
    }

    @Test
    fun testParallelReadHeads() {
        TRTInterpreter.interpretTestSpec(useReadHeads = true, spec = """
            mark = 0   | mark = 0   | mark = 0   | mark = 0
            add(2)     |            |            |
                       | add(4)     |            |
                       |            | add(6)     |
                       |            |            | add(8)
            poll() = 2 | poll() = 2 | poll() = 2 | poll() = 2
            poll() = 4 | poll() = 4 | poll() = 4 | poll() = 4
            poll() = 6 | poll() = 6 | poll() = 6 | mark = 2
            poll() = 8 | poll() = 8 | mark = 3   | poll() = 6
            mark = 4   | mark = 4   | poll() = 8 | poll() = 8
        """)
    }

    @Test
    fun testPeek() {
        TRTInterpreter.interpretTestSpec(useReadHeads = true, spec = """
            add(2)     |            |               |
                       | add(4)     |               |
                       |            | add(6)        |
                       |            |               | add(8)
            peek() = 2 | poll() = 2 | poll() = 2    | peek() = 2
            peek() = 2 | peek() = 4 | poll() = 4    | peek() = 2
            peek() = 2 | peek() = 4 | peek() = 6    | poll() = 2
            peek() = 2 | mark = 1   | mark = 2      | poll() = 4
            mark = 0   | peek() = 4 | peek() = 6    | peek() = 6
            poll() = 2 | poll() = 4 | poll() = 6    | poll() = 6
            poll() = 4 | mark = 2   | poll() = 8    | peek() = 8
            peek() = 6 | peek() = 6 | peek() = null | mark = 3
        """)
    }
}

private object TRTInterpreter : ConcurrentIntepreter<TrackRecord<Int>>(interpretTable) {
    fun interpretTestSpec(spec: String, useReadHeads: Boolean) = if (useReadHeads) {
        interpretTestSpec(spec, initial = ArrayTrackRecord(),
                threadTransform = { (it as ArrayTrackRecord).newReadHead() })
    } else {
        interpretTestSpec(spec, ArrayTrackRecord())
    }
}

/*
 * Quick ref of supported expressions :
 * sleep(x) : sleeps for x time units and returns Unit ; sleep alone means sleep(1)
 * add(x) : calls and returns TrackRecord#add.
 * poll(time, pos) [{ predicate }] : calls and returns TrackRecord#poll(x time units, pos).
 *   Optionally, a predicate may be specified.
 * poll() [{ predicate }] : calls and returns ReadHead#poll(1 time unit). Optionally, a predicate
 *   may be specified.
 * EXPR = VALUE : asserts that EXPR equals VALUE. EXPR is interpreted. VALUE can either be the
 *   string "null" or an int. Returns Unit.
 * EXPR time x..y : measures the time taken by EXPR and asserts it took at least x and at most
 *   y time units.
 * predicate must be one of "= x", "< x" or "> x".
 */
private val interpretTable = listOf<InterpretMatcher<TrackRecord<Int>>>(
    // Interpret "XXX is odd" : run XXX and assert its return value is odd ("even" works too)
    Regex("(.*)\\s+is\\s+(even|odd)") to { i, t, r ->
        i.interpret(r.strArg(1), t).also {
            assertEquals((it as Int) % 2, if ("even" == r.strArg(2)) 0 else 1)
        }
    },
    // Interpret "add(XXX)" as TrackRecord#add(int)
    Regex("""add\((\d+)\)""") to { i, t, r ->
        t.add(r.intArg(1))
    },
    // Interpret "poll(x, y)" as TrackRecord#poll(timeout = x * INTERPRET_TIME_UNIT, pos = y)
    // Accepts an optional {} argument for the predicate (see makePredicate for syntax)
    Regex("""poll\((\d+),\s*(\d+)\)\s*(\{.*\})?""") to { i, t, r ->
        t.poll(r.timeArg(1), r.intArg(2), makePredicate(r.strArg(3)))
    },
    // ReadHead#poll. If this throws in the cast, the code is malformed and has passed "poll()"
    // in a test that takes a TrackRecord that is not a ReadHead. It's technically possible to get
    // the test code to not compile instead of throw, but it's vastly more complex and this will
    // fail 100% at runtime any test that would not have compiled.
    Regex("""poll\((\d+)?\)\s*(\{.*\})?""") to { i, t, r ->
        (if (r.strArg(1).isEmpty()) i.interpretTimeUnit else r.timeArg(1)).let { time ->
            (t as ArrayTrackRecord<Int>.ReadHead).poll(time, makePredicate(r.strArg(2)))
        }
    },
    // ReadHead#mark. The same remarks apply as with ReadHead#poll.
    Regex("mark") to { i, t, _ -> (t as ArrayTrackRecord<Int>.ReadHead).mark },
    // ReadHead#peek. The same remarks apply as with ReadHead#poll.
    Regex("peek\\(\\)") to { i, t, _ -> (t as ArrayTrackRecord<Int>.ReadHead).peek() }
)

// Parses a { = x } or { < x } or { > x } string and returns the corresponding predicate
// Returns an always-true predicate for empty and null arguments
private fun makePredicate(spec: String?): (Int) -> Boolean {
    if (spec.isNullOrEmpty()) return { true }
    val match = Regex("""\{\s*([<>=])\s*(\d+)\s*\}""").matchEntire(spec)
            ?: throw SyntaxException("Predicate \"${spec}\"")
    val arg = match.intArg(2)
    return when (match.strArg(1)) {
        ">" -> { i -> i > arg }
        "<" -> { i -> i < arg }
        "=" -> { i -> i == arg }
        else -> throw RuntimeException("How did \"${spec}\" match this regexp ?")
    }
}
