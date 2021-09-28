/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package trebuchet.extras

import trebuchet.io.BufferProducer
import trebuchet.io.DataSlice
import java.io.File
import java.io.InputStream

typealias ProgressCallback = ((Long, Long) -> Unit)

class InputStreamAdapter(val inputStream: InputStream,
                         val totalSize: Long = 0,
                         val progressCallback: ProgressCallback? = null) : BufferProducer {
    constructor(file: File, progressCallback: ProgressCallback? = null)
            : this(file.inputStream(), file.length(), progressCallback)

    var totalRead: Long = 0
    var hitEof = false

    override fun next(): DataSlice? {
        if (hitEof) return null
        val buffer = ByteArray(2 * 1024 * 1024)
        val read = inputStream.read(buffer)
        if (read == -1) {
            hitEof = true
            return null
        }
        totalRead += read
        progressCallback?.let { it.invoke(totalRead, totalSize) }
        return DataSlice(buffer, 0, read)
    }

    override fun close() {
        hitEof = true
        inputStream.close()
    }
}