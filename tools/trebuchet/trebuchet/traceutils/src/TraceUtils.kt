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

import trebuchet.extractors.ExtractorRegistry
import trebuchet.extras.InputStreamAdapter
import trebuchet.extras.findSampleData
import trebuchet.importers.ImporterRegistry
import trebuchet.io.BufferProducer
import trebuchet.io.StreamingReader
import trebuchet.util.PrintlnImportFeedback
import java.io.File
import java.io.OutputStream

fun validateSrcDest(source: File, destDir: File) {
    if (!source.exists()) {
        throw IllegalArgumentException("No such file '$source'")
    }
    if (!destDir.exists()) {
        destDir.mkdirs()
        if (!destDir.exists() && destDir.isDirectory) {
            throw IllegalArgumentException("'$destDir' isn't a directory")
        }
    }
}

fun findTraces(stream: BufferProducer,
               onlyKnownFormats: Boolean,
               traceFound: (StreamingReader) -> Unit) {
    val importFeedback = PrintlnImportFeedback()
    try {
        val reader = StreamingReader(stream)
        reader.loadIndex(reader.keepLoadedSize.toLong())
        val extractor = ExtractorRegistry.extractorFor(reader, importFeedback)
        if (extractor != null) {
            extractor.extract(reader, {
                subStream -> findTraces(subStream, onlyKnownFormats, traceFound)
            })
        } else {
            if (onlyKnownFormats) {
                val importer = ImporterRegistry.importerFor(reader, importFeedback)
                if (importer != null) {
                    traceFound(reader)
                }
            } else {
                traceFound(reader)
            }
        }
    } catch (ex: Throwable) {
        importFeedback.reportImportException(ex)
    } finally {
        stream.close()
    }
}

fun copy(reader: StreamingReader, output: OutputStream) {
    reader.windows.forEach {
        val slice = it.slice
        output.write(slice.buffer, slice.startIndex, slice.length)
    }
    var next = reader.source.next()
    while (next != null) {
        output.write(next.buffer, next.startIndex, next.length)
        next = reader.source.next()
    }
}

fun extract(source: File, destDir: File) {
    validateSrcDest(source, destDir)
    println("Extracting ${source.name} to ${destDir.path}")
    var traceCount = 0
    findTraces(InputStreamAdapter(source), onlyKnownFormats = false) {
        trace ->
        val destFile = File(destDir, "${source.nameWithoutExtension}_$traceCount")
        println("Found subtrace, extracting to $destFile")
        val outputStream = destFile.outputStream()
        copy(trace, outputStream)
        traceCount++
    }

}

fun main(args: Array<String>) {
    if (args.size != 2) {
        println("Usage: <source file> <dest dir>")
        return
    }
    try {
        val source = File(args[0])
        val destDir = File(args[1])
        extract(source, destDir)
    } catch (ex: Exception) {
        println(ex.message)
    }
}
