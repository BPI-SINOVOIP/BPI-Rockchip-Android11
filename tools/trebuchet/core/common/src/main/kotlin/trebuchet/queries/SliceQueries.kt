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

package trebuchet.queries.slices

import kotlin.sequences.Sequence
import kotlin.sequences.SequenceScope

import trebuchet.model.Model
import trebuchet.model.ProcessModel
import trebuchet.model.ThreadModel
import trebuchet.model.base.Slice
import trebuchet.model.base.SliceGroup
import trebuchet.util.slices.parseSliceName
import trebuchet.util.time.*

enum class TraverseAction {
    /**
     * Continue iterating over any child slices
     */
    VISIT_CHILDREN,
    /**
     * There is no need to process any child slices
     */
    DONE,
}

interface SliceTraverser {
    fun beginSlice(slice: Slice): TraverseAction = TraverseAction.VISIT_CHILDREN
    fun endSlice(slice: Slice) {}
}

class MissingSliceException : Exception {
    constructor(pid : Int, type : String, value : String?) {
        Exception("Unable to find slice. PID = $pid, Type = $type" +
                  (if (value == null) "" else ", Value = $value"))
    }

    constructor(pid : Int, type : String, value : String?, lowerBound : Double, upperBound : Double) {
        Exception("Unable to find slice. PID = $pid, Type = $type" +
                  (if (value == null) "" else ", Value = $value") +
                  " (Bounds: [${lowerBound.secondValueToMillisecondString()}," +
                  " ${upperBound.secondValueToMillisecondString()}])")
    }

    constructor (pid : Int, pattern : Regex) {
        Exception("Unable to find slice.  PID = $pid, Pattern = $pattern")
    }

    constructor (pid : Int, pattern : Regex, lowerBound : Double, upperBound : Double) {
        Exception("Unable to find slice.  PID = $pid, Pattern = $pattern" +
                  " (Bounds: [${lowerBound.secondValueToMillisecondString()}," +
                  " ${upperBound.secondValueToMillisecondString()}])")
    }
}

/**
 * Find all of the slices that match the given predicate.
 *
 * @param predicate  The predicate used to test slices
 */
fun Model.selectAll(predicate: (Slice) -> Boolean): List<Slice> {
    val ret = mutableListOf<Slice>()
    this.iterSlices {
        if (predicate(it)) {
            ret.add(it)
        }
    }
    return ret
}

/**
 * Find all of the slices that match the given predicate.
 *
 * @param predicate  The predicate used to test slices
 */
fun ProcessModel.selectAll(predicate : (Slice) -> Boolean) : List<Slice> {
    val ret = mutableListOf<Slice>()
    this.iterSlices {
        if (predicate(it)) {
            ret.add(it)
        }
    }
    return ret
}

fun ThreadModel.selectAll(predicate: (Slice) -> Boolean): List<Slice> {
    val ret = mutableListOf<Slice>()
    this.iterSlices {
        if (predicate(it)) {
            ret.add(it)
        }
    }
    return ret
}

/**
 * Find the first slice that matches the given predicate.
 *
 * @param predicate  The predicate used to test slices
 */
fun Model.selectFirst(predicate: (Slice) -> Boolean) : Slice? {
    return this.processes.values.mapNotNull { it.selectFirst(predicate) }.minBy { it.startTime }
}


/**
 * Find the first slice that matches the given predicate.
 *
 * @param predicate  The predicate used to test slices
 */
fun ProcessModel.selectFirst(predicate: (Slice) -> Boolean) : Slice? {
    var ret : Slice? = null

    this.asyncSlices.forEach {
        if (predicate(it)) {
            ret = it
            return@forEach
        }
    }

    this.threads.forEach { thread ->
        val threadSlice = thread.selectFirst(predicate)

        if (threadSlice != null && (ret == null || threadSlice.startTime < ret!!.startTime)) {
            ret = threadSlice
        }
    }

    return ret
}


/**
 * Find the first slice that matches the given predicate.
 *
 * @param predicate  The predicate used to test slices
 */
fun ThreadModel.selectFirst(predicate : (Slice) -> Boolean) : Slice? {
    var ret : Slice? = null
    this.iterSlices {
        if (predicate(it)) {
            ret = it
            return@iterSlices
        }
    }
    return ret
}

/**
 * This function is a more powerful version of [iterSlices].  It takes a [SliceTraverser], which
 * allows code to be run at the beginning and end of processing a slice.  This allows the
 * [SliceTraverser] to, for example, keep track of how deep it is within the tree.  The
 * [SliceTraverser] can also indicate whether [traverseSlices] should continue processing child
 * slices in the case of a [SliceGroup].
 */
fun Model.traverseSlices(visitor: SliceTraverser) {
    this.processes.values.forEach { it.traverseSlices(visitor) }
}

/**
 * This function is a more powerful version of [iterSlices].  It takes a [SliceTraverser], which
 * allows code to be run at the beginning and end of processing a slice.  This allows the
 * [SliceTraverser] to, for example, keep track of how deep it is within the tree.  The
 * [SliceTraverser] can also indicate whether [traverseSlices] should continue processing child
 * slices in the case of a [SliceGroup].
 */
fun ProcessModel.traverseSlices(visitor: SliceTraverser) {
    this.asyncSlices.forEach {
        visitor.beginSlice(it)
        visitor.endSlice(it)
    }

    this.threads.forEach { it.traverseSlices(visitor) }
}

/**
 * This function is a more powerful version of [iterSlices].  It takes a [SliceTraverser], which
 * allows code to be run at the beginning and end of processing a slice.  This allows the
 * [SliceTraverser] to, for example, keep track of how deep it is within the tree.  The
 * [SliceTraverser] can also indicate whether [traverseSlices] should continue processing child
 * slices in the case of a [SliceGroup].
 */
fun ThreadModel.traverseSlices(visitor: SliceTraverser) {
    this.slices.traverseSlices(visitor)
}

/**
 * This function is a more powerful version of [iterSlices].  It takes a [SliceTraverser], which
 * allows code to be run at the beginning and end of processing a slice.  This allows the
 * [SliceTraverser] to, for example, keep track of how deep it is within the tree.  The
 * [SliceTraverser] can also indicate whether [traverseSlices] should continue processing child
 * slices in the case of a [SliceGroup].
 */
fun List<SliceGroup>.traverseSlices(visitor: SliceTraverser) {
    this.forEach {
        val action = visitor.beginSlice(it)
        if (action == TraverseAction.VISIT_CHILDREN) {
            it.children.traverseSlices(visitor)
        }
        visitor.endSlice(it)
    }
}

/**
 * Call the provided lambda on every slice in the model.
 */
fun Model.iterSlices(consumer: (Slice) -> Unit) {
    this.processes.values.forEach { it.iterSlices(consumer) }
}

/**
 * Call the provided lambda on every slice in the model.
 */
fun ProcessModel.iterSlices(consumer: (Slice) -> Unit) {
    this.asyncSlices.forEach { consumer(it) }
    this.threads.forEach { it.iterSlices(consumer) }
}

/**
 * Call the provided lambda on every slice in the model.
 */
fun ThreadModel.iterSlices(consumer: (Slice) -> Unit) {
    this.slices.iterSlices(consumer)
}

/**
 * Call the provided lambda on every slice in the model.
 */
fun List<SliceGroup>.iterSlices(consumer: (Slice) -> Unit) {
    this.forEach {
        consumer(it)
        it.children.iterSlices(consumer)
    }
}

/**
 * Call the provided lambda on every slice in the list.
 */
fun List<SliceGroup>.any(predicate: (Slice) -> Boolean): Boolean {
    this.forEach {
        if (predicate(it)) return true
        if (it.children.any(predicate)) return true
    }
    return false
}

/**
 * Find the first slice that meets the provided criteria.
 *
 * @param queryType  The "type" of the slice being searched for
 * @param queryValue  The "value" of the slice being searched for
 * @param lowerBound  Slice must occur after this timestamp
 * @param upperBound  Slice must occur before this timestamp
 *
 * @throws MissingSliceException  If no matching slice is found.
 */
fun ProcessModel.findFirstSlice(queryType : String,
                                queryValue : String?,
                                lowerBound : Double = this.model.beginTimestamp,
                                upperBound : Double = this.model.endTimestamp) : Slice {

    val foundSlice = this.findFirstSliceOrNull(queryType, queryValue, lowerBound, upperBound)

    if (foundSlice != null) {
        return foundSlice
    } else if (lowerBound == this.model.beginTimestamp && upperBound == this.model.endTimestamp) {
        throw MissingSliceException(this.id, queryType, queryValue)
    } else {
        throw MissingSliceException(this.id, queryType, queryValue, lowerBound, upperBound)
    }
}

/**
 * Find the first slice that meets the provided criteria.
 *
 * @param pattern  A pattern the slice text must match
 * @param lowerBound  Slice must occur after this timestamp
 * @param upperBound  Slice must occur before this timestamp
 *
 * @throws MissingSliceException  If no matching slice is found.
 */
fun ProcessModel.findFirstSlice(pattern : Regex,
                                lowerBound : Double = this.model.beginTimestamp,
                                upperBound : Double = this.model.endTimestamp) : Slice {

    val foundSlice = this.findFirstSliceOrNull(pattern, lowerBound, upperBound)

    if (foundSlice != null) {
        return foundSlice
    } else if (lowerBound == this.model.beginTimestamp && upperBound == this.model.endTimestamp) {
        throw MissingSliceException(this.id, pattern)
    } else {
        throw MissingSliceException(this.id, pattern, lowerBound, upperBound)
    }
}

/**
 * Find the first slice that meets the provided criteria.
 *
 * @param queryType  The "type" of the slice being searched for
 * @param queryValue  The "value" of the slice being searched for
 * @param lowerBound  Slice must occur after this timestamp
 * @param upperBound  Slice must occur before this timestamp
 *
 * @return Slice or null if no match is found.
 */
fun ProcessModel.findFirstSliceOrNull(queryType : String,
                                      queryValue : String?,
                                      lowerBound : Double = this.model.beginTimestamp,
                                      upperBound : Double = this.model.endTimestamp) : Slice? {

    return this.selectFirst { slice ->
        val sliceInfo = parseSliceName(slice.name)

        sliceInfo.name == queryType &&
        (queryValue == null || sliceInfo.value == queryValue) &&
        lowerBound <= slice.startTime &&
        slice.startTime <= upperBound
    }
}

/**
 * Find the first slice that meets the provided criteria.
 *
 * @param pattern  A pattern the slice text must match
 * @param lowerBound  Slice must occur after this timestamp
 * @param upperBound  Slice must occur before this timestamp
 *
 * @return Slice or null if no match is found.
 */
fun ProcessModel.findFirstSliceOrNull(pattern : Regex,
                                      lowerBound : Double = this.model.beginTimestamp,
                                      upperBound : Double = this.model.endTimestamp) : Slice? {

    return this.selectFirst { slice ->
        pattern.find(slice.name) != null &&
        lowerBound <= slice.startTime &&
        slice.startTime <= upperBound
    }
}

/**
 * Find all slices that meet the provided criteria.
 *
 * @param queryType  The "type" of the slice being searched for
 * @param queryValue  The "value" of the slice being searched for
 * @param lowerBound  Slice must occur after this timestamp
 * @param upperBound  Slice must occur before this timestamp
 *
 * @throws MissingSliceException  If no matching slice is found.
 */
fun ProcessModel.findAllSlices(queryType : String,
                               queryValue : String?,
                               lowerBound : Double = this.model.beginTimestamp,
                               upperBound : Double = this.model.endTimestamp) : List<Slice> {

    return this.selectAll{ slice ->
        val sliceInfo = parseSliceName(slice.name)

        sliceInfo.name == queryType &&
        (queryValue == null || sliceInfo.value == queryValue) &&
        lowerBound <= slice.startTime &&
        slice.startTime <= upperBound
    }
}

/**
 * Find all slices that meet the provided criteria.
 *
 * @param pattern  A pattern the slice text must match
 * @param lowerBound  Slice must occur after this timestamp
 * @param upperBound  Slice must occur before this timestamp
 *
 * @throws MissingSliceException  If no matching slice is found.
 */
fun ProcessModel.findAllSlices(pattern : Regex,
                               lowerBound : Double = this.model.beginTimestamp,
                               upperBound : Double = this.model.endTimestamp) : List<Slice> {

    return this.selectAll { slice ->
        pattern.find(slice.name) != null &&
        lowerBound <= slice.startTime &&
        slice.startTime <= upperBound
    }
}

private suspend fun SequenceScope<Slice>.yieldSlices(slices: List<SliceGroup>) {
    slices.forEach {
        yield(it)
        yieldSlices(it.children)
    }
}

fun Model.slices(includeAsync: Boolean = true): Sequence<Slice> {
    val model = this
    return sequence {
        model.processes.values.forEach { process ->
            if (includeAsync) {
                yieldAll(process.asyncSlices)
            }
            process.threads.forEach { thread ->
                yieldSlices(thread.slices)
            }
        }
    }
}