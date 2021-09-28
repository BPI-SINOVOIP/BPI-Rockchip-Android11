/*
 * Copyright (C) 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.tools.metalava.doclava1

class ApiParseException : Exception {
    private var file: String? = null
    private var line = 0

    internal constructor(message: String) : super(message)
    internal constructor(message: String, file: String, cause: Exception?) : super(message, cause) {
        this.file = file
    }
    internal constructor(message: String, tokenizer: ApiFile.Tokenizer) : super(message) {
        file = tokenizer.fileName
        line = tokenizer.line
    }

    override val message: String
        get() {
            val sb = StringBuilder()
            if (file != null) {
                sb.append(file).append(':')
            }
            if (line > 0) {
                sb.append(line).append(':')
            }
            if (sb.isNotEmpty()) {
                sb.append(' ')
            }
            sb.append(super.message)
            return sb.toString()
        }
}