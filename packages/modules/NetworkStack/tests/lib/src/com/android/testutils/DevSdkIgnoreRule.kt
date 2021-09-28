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

import android.os.Build
import org.junit.Assume.assumeTrue
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement

/**
 * Returns true if the development SDK version of the device is in the provided range.
 *
 * If the device is not using a release SDK, the development SDK is considered to be higher than
 * [Build.VERSION.SDK_INT].
 */
fun isDevSdkInRange(minExclusive: Int?, maxInclusive: Int?): Boolean {
    // In-development API n+1 will have SDK_INT == n and CODENAME != REL.
    // Stable API n has SDK_INT == n and CODENAME == REL.
    val release = "REL" == Build.VERSION.CODENAME
    val sdkInt = Build.VERSION.SDK_INT
    val devApiLevel = sdkInt + if (release) 0 else 1

    return (minExclusive == null || devApiLevel > minExclusive) &&
            (maxInclusive == null || devApiLevel <= maxInclusive)
}

/**
 * A test rule to ignore tests based on the development SDK level.
 *
 * If the device is not using a release SDK, the development SDK is considered to be higher than
 * [Build.VERSION.SDK_INT].
 *
 * @param ignoreClassUpTo Skip all tests in the class if the device dev SDK is <= this value.
 * @param ignoreClassAfter Skip all tests in the class if the device dev SDK is > this value.
 */
class DevSdkIgnoreRule @JvmOverloads constructor(
    private val ignoreClassUpTo: Int? = null,
    private val ignoreClassAfter: Int? = null
) : TestRule {
    override fun apply(base: Statement, description: Description): Statement {
        return IgnoreBySdkStatement(base, description)
    }

    /**
     * Ignore the test for any development SDK that is strictly after [value].
     *
     * If the device is not using a release SDK, the development SDK is considered to be higher
     * than [Build.VERSION.SDK_INT].
     */
    annotation class IgnoreAfter(val value: Int)

    /**
     * Ignore the test for any development SDK that lower than or equal to [value].
     *
     * If the device is not using a release SDK, the development SDK is considered to be higher
     * than [Build.VERSION.SDK_INT].
     */
    annotation class IgnoreUpTo(val value: Int)

    private inner class IgnoreBySdkStatement(
        private val base: Statement,
        private val description: Description
    ) : Statement() {
        override fun evaluate() {
            val ignoreAfter = description.getAnnotation(IgnoreAfter::class.java)
            val ignoreUpTo = description.getAnnotation(IgnoreUpTo::class.java)

            val message = "Skipping test for build ${Build.VERSION.CODENAME} " +
                    "with SDK ${Build.VERSION.SDK_INT}"
            assumeTrue(message, isDevSdkInRange(ignoreClassUpTo, ignoreClassAfter))
            assumeTrue(message, isDevSdkInRange(ignoreUpTo?.value, ignoreAfter?.value))
            base.evaluate()
        }
    }
}