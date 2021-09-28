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

import com.android.testutils.ExceptionUtils.ThrowingRunnable
import java.lang.reflect.Modifier
import kotlin.system.measureTimeMillis
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private const val TAG = "Connectivity unit test"

fun <T> assertEmpty(ts: Array<T>) = ts.size.let { len ->
    assertEquals(0, len, "Expected empty array, but length was $len")
}

fun <T> assertLength(expected: Int, got: Array<T>) = got.size.let { len ->
    assertEquals(expected, len, "Expected array of length $expected, but was $len for $got")
}

// Bridge method to help write this in Java. If you're writing Kotlin, consider using native
// kotlin.test.assertFailsWith instead, as that method is reified and inlined.
fun <T : Exception> assertThrows(expected: Class<T>, block: ThrowingRunnable): T {
    return assertFailsWith(expected.kotlin) { block.run() }
}

fun <T : Exception> assertThrows(msg: String, expected: Class<T>, block: ThrowingRunnable): T {
    return assertFailsWith(expected.kotlin, msg) { block.run() }
}

fun <T> assertEqualBothWays(o1: T, o2: T) {
    assertTrue(o1 == o2)
    assertTrue(o2 == o1)
}

fun <T> assertNotEqualEitherWay(o1: T, o2: T) {
    assertFalse(o1 == o2)
    assertFalse(o2 == o1)
}

fun assertStringContains(got: String, want: String) {
    assertTrue(got.contains(want), "$got did not contain \"${want}\"")
}

fun assertContainsExactly(actual: IntArray, vararg expected: Int) {
    // IntArray#sorted() returns a list, so it's fine to test with equals()
    assertEquals(actual.sorted(), expected.sorted(),
            "$actual does not contain exactly $expected")
}

fun assertContainsStringsExactly(actual: Array<String>, vararg expected: String) {
    assertEquals(actual.sorted(), expected.sorted(),
            "$actual does not contain exactly $expected")
}

fun <T> assertContainsAll(list: Collection<T>, vararg elems: T) {
    assertContainsAll(list, elems.asList())
}

fun <T> assertContainsAll(list: Collection<T>, elems: Collection<T>) {
    elems.forEach { assertTrue(list.contains(it), "$it not in list") }
}

fun assertRunsInAtMost(descr: String, timeLimit: Long, fn: Runnable) {
    assertRunsInAtMost(descr, timeLimit) { fn.run() }
}

fun assertRunsInAtMost(descr: String, timeLimit: Long, fn: () -> Unit) {
    val timeTaken = measureTimeMillis(fn)
    val msg = String.format("%s: took %dms, limit was %dms", descr, timeTaken, timeLimit)
    assertTrue(timeTaken <= timeLimit, msg)
}

/**
 * Verifies that the number of nonstatic fields in a java class equals a given count.
 * Note: this is essentially not useful for Kotlin code where fields are not really a thing.
 *
 * This assertion serves as a reminder to update test code around it if fields are added
 * after the test is written.
 * @param count Expected number of nonstatic fields in the class.
 * @param clazz Class to test.
 */
fun <T> assertFieldCountEquals(count: Int, clazz: Class<T>) {
    assertEquals(count, clazz.declaredFields.filter {
        !Modifier.isStatic(it.modifiers) && !Modifier.isTransient(it.modifiers)
    }.size)
}
