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

package trebuchet.io

import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Test
import trebuchet.extras.findSampleData
import trebuchet.importers.DummyImportFeedback
import trebuchet.model.Model
import trebuchet.task.ImportTask
import trebuchet.testutils.makeReader
import trebuchet.testutils.NeedsSampleData
import java.io.BufferedReader
import java.io.File
import java.io.FileReader
import kotlin.test.assertNull

class StringStreamTest {
    @Test fun testLineReader() {
        expect1_20_300("1\n20\n300")
    }

    @Test fun testLineReaderCLRF() {
        expect1_20_300("1\r\n20\r\n300")
    }

    @Test @NeedsSampleData
    fun testLineReaderOnFile() {
        val file = File("${findSampleData()}/sample.ftrace")
        Assert.assertTrue(file.exists())
        val expected = BufferedReader(FileReader(file))
        StreamingReader(readFile(file)).iterLines().forEach {
            assertEquals(expected.readLine(), it.toString())
        }
        assertNull(expected.readLine())
    }

    @Test fun testLineReaderEmptyLines() {
        expect1_20_300("\n\n1\r\n\r\n20\n\n\n\n300\n\n\n")

    }

    private fun expect1_20_300(data: String) {
        val lines = mutableListOf<String>()
        data.makeReader().iterLines().forEach {
            lines.add(it.toString())
        }
        assertEquals(3, lines.size)
        assertEquals("1", lines[0])
        assertEquals("20", lines[1])
        assertEquals("300", lines[2])
    }

    fun readFile(file: File): BufferProducer {
        val inputStream = file.inputStream()
        return object : BufferProducer {
            override fun next(): DataSlice? {
                val buffer = ByteArray(2 * 1024 * 1024)
                val read = inputStream.read(buffer)
                if (read == -1) return null
                return buffer.asSlice(read)
            }
        }
    }
}