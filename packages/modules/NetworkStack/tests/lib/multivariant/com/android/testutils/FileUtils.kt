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

// This function is private because the 2 is hardcoded here, and is not correct if not called
// directly from __LINE__ or __FILE__.
private fun callerStackTrace(): StackTraceElement = try {
    throw RuntimeException()
} catch (e: RuntimeException) {
    e.stackTrace[2] // 0 is here, 1 is get() in __FILE__ or __LINE__
}
val __FILE__: String get() = callerStackTrace().fileName
val __LINE__: Int get() = callerStackTrace().lineNumber
