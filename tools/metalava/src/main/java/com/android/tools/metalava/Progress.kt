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
package com.android.tools.metalava

import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

private val PROGRESS_TIME_FORMATTER = DateTimeFormatter.ofPattern("HH:mm:ss.SSS")
private var beginningOfLine = true
private var firstProgress = true

/** Print a progress message with a timestamp when --verbose is enabled. */
fun progress(message: String) {
    if (!options.verbose) {
        return
    }
    if (!beginningOfLine) {
        options.stdout.println()
    }
    val now = LocalDateTime.now().format(PROGRESS_TIME_FORMATTER)

    if (!firstProgress) {
        options.stdout.print(now)
        options.stdout.print("   CPU: ")
        options.stdout.println(getCpuStats())

        options.stdout.print(now)
        options.stdout.print("   MEM: ")
        options.stdout.println(getMemoryStats())
    }
    firstProgress = false

    options.stdout.print(now)
    options.stdout.print(" ")
    options.stdout.print(message)
    options.stdout.flush()
    beginningOfLine = message.endsWith('\n')
}

private var lastMillis: Long = -1L
private var lastUserMillis: Long = -1L
private var lastCpuMillis: Long = -1L

private fun getCpuStats(): String {
    val nowMillis = System.currentTimeMillis()
    val userMillis = ManagementWrapper.getUserTimeNano() / 1000_000
    val cpuMillis = ManagementWrapper.getCpuTimeNano() / 1000_000

    if (lastMillis == -1L) {
        lastMillis = nowMillis
    }
    if (lastUserMillis == -1L) {
        lastUserMillis = userMillis
    }
    if (lastCpuMillis == -1L) {
        lastCpuMillis = cpuMillis
    }

    val realDeltaMs = nowMillis - lastMillis
    val userDeltaMillis = userMillis - lastUserMillis
    // Sometimes we'd get "-0.0" without the max.
    val sysDeltaMillis = Math.max(0, cpuMillis - lastCpuMillis - userDeltaMillis)

    lastMillis = nowMillis
    lastUserMillis = userMillis
    lastCpuMillis = cpuMillis

    return String.format(
        "+%.1freal +%.1fusr +%.1fsys",
        realDeltaMs / 1_000.0,
        userDeltaMillis / 1_000.0,
        sysDeltaMillis / 1_000.0)
}

private fun getMemoryStats(): String {
    val mu = ManagementWrapper.getHeapUsage()

    return String.format(
        "%dmi %dmu %dmc %dmx",
        mu.init / 1024 / 1024,
        mu.used / 1024 / 1024,
        mu.committed / 1024 / 1024,
        mu.max / 1024 / 1024)
}

/** Used for verbose output to show progress bar */
private var tick = 0

/** Needed for tests to ensure we don't get unpredictable behavior of "." in output */
fun resetTicker() {
    tick = 0
}

/** Print progress */
fun tick() {
    tick++
    if (tick % 100 == 0) {
        if (!options.verbose) {
            return
        }
        beginningOfLine = false
        options.stdout.print(".")
        options.stdout.flush()
    }
}
