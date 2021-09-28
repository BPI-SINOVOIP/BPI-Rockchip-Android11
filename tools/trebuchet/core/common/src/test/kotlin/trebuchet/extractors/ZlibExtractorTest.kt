/*
 * Copyright 2018 Google Inc.
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

package trebuchet.extractors

import org.junit.Test
import trebuchet.extras.InputStreamAdapter
import trebuchet.extras.findSampleData
import trebuchet.importers.FatalImportFeedback
import trebuchet.io.BufferProducer
import trebuchet.io.StreamingReader
import trebuchet.testutils.NeedsSampleData
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ZlibExtractorTest {
    @Test @NeedsSampleData
    fun testFactorySuccess() {
        val reader = StreamingReader(openSample())
        reader.loadIndex(reader.keepLoadedSize.toLong())
        assertNotNull(ZlibExtractor.Factory.extractorFor(reader, FatalImportFeedback))
    }

    @Test @NeedsSampleData
    fun testFactoryNotDeflated() {
        val reader = StreamingReader(openSample("sample.ftrace"))
        reader.loadIndex(reader.keepLoadedSize.toLong())
        assertNull(ZlibExtractor.Factory.extractorFor(reader, FatalImportFeedback))
    }

    @Test @NeedsSampleData
    fun testFactoryNoHeader() {
        val reader = StreamingReader(openSample("caltrace1.html"))
        reader.loadIndex(reader.keepLoadedSize.toLong())
        assertNull(ZlibExtractor.Factory.extractorFor(reader, FatalImportFeedback))
    }

    @Test @NeedsSampleData
    fun testExtractInitial() {
        val expected = "# tracer: nop\n#\n# ent"
        val extractor = ZlibExtractor(FatalImportFeedback)
        extractor.extract(StreamingReader(openSample())) {
            val first = it.next()
            assertNotNull(first)
            assertTrue(first.length > expected.length)
            assertEquals(expected, first.slice(0, expected.length).toString())
            it.close()
        }
    }

    private fun openSample(name: String = "missed_traces.trace"): BufferProducer {
        val file = File(findSampleData(), name)
        assertTrue(file.exists(), "Unable to find '$name'")
        return InputStreamAdapter(file)
    }
}