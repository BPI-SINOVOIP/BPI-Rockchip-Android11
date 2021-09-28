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

import java.util.concurrent.TimeUnit
import java.util.concurrent.locks.Condition
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

/**
 * A List that additionally offers the ability to append via the add() method, and to retrieve
 * an element by its index optionally waiting for it to become available.
 */
interface TrackRecord<E> : List<E> {
    /**
     * Adds an element to this queue, waking up threads waiting for one. Returns true, as
     * per the contract for List.
     */
    fun add(e: E): Boolean

    /**
     * Returns the first element after {@param pos}, possibly blocking until one is available, or
     * null if no such element can be found within the timeout.
     * If a predicate is given, only elements matching the predicate are returned.
     *
     * @param timeoutMs how long, in milliseconds, to wait at most (best effort approximation).
     * @param pos the position at which to start polling.
     * @param predicate an optional predicate to filter elements to be returned.
     * @return an element matching the predicate, or null if timeout.
     */
    fun poll(timeoutMs: Long, pos: Int, predicate: (E) -> Boolean = { true }): E?
}

/**
 * A thread-safe implementation of TrackRecord that is backed by an ArrayList.
 *
 * This class also supports the creation of a read-head for easier single-thread access.
 * Refer to the documentation of {@link ArrayTrackRecord.ReadHead}.
 */
class ArrayTrackRecord<E> : TrackRecord<E> {
    private val lock = ReentrantLock()
    private val condition = lock.newCondition()
    // Backing store. This stores the elements in this ArrayTrackRecord.
    private val elements = ArrayList<E>()

    // The list iterator for RecordingQueue iterates over a snapshot of the collection at the
    // time the operator is created. Because TrackRecord is only ever mutated by appending,
    // that makes this iterator thread-safe as it sees an effectively immutable List.
    class ArrayTrackRecordIterator<E>(
        private val list: ArrayList<E>,
        start: Int,
        private val end: Int
    ) : ListIterator<E> {
        var index = start
        override fun hasNext() = index < end
        override fun next() = list[index++]
        override fun hasPrevious() = index > 0
        override fun nextIndex() = index + 1
        override fun previous() = list[--index]
        override fun previousIndex() = index - 1
    }

    // List<E> implementation
    override val size get() = lock.withLock { elements.size }
    override fun contains(element: E) = lock.withLock { elements.contains(element) }
    override fun containsAll(elements: Collection<E>) = lock.withLock {
        this.elements.containsAll(elements)
    }
    override operator fun get(index: Int) = lock.withLock { elements[index] }
    override fun indexOf(element: E): Int = lock.withLock { elements.indexOf(element) }
    override fun lastIndexOf(element: E): Int = lock.withLock { elements.lastIndexOf(element) }
    override fun isEmpty() = lock.withLock { elements.isEmpty() }
    override fun listIterator(index: Int) = ArrayTrackRecordIterator(elements, index, size)
    override fun listIterator() = listIterator(0)
    override fun iterator() = listIterator()
    override fun subList(fromIndex: Int, toIndex: Int): List<E> = lock.withLock {
        elements.subList(fromIndex, toIndex)
    }

    // TrackRecord<E> implementation
    override fun add(e: E): Boolean {
        lock.withLock {
            elements.add(e)
            condition.signalAll()
        }
        return true
    }
    override fun poll(timeoutMs: Long, pos: Int, predicate: (E) -> Boolean) = lock.withLock {
        elements.getOrNull(pollForIndexReadLocked(timeoutMs, pos, predicate))
    }

    // For convenience
    fun getOrNull(pos: Int, predicate: (E) -> Boolean) = lock.withLock {
        if (pos < 0 || pos > size) null else elements.subList(pos, size).find(predicate)
    }

    // Returns the index of the next element whose position is >= pos matching the predicate, if
    // necessary waiting until such a time that such an element is available, with a timeout.
    // If no such element is found within the timeout -1 is returned.
    private fun pollForIndexReadLocked(timeoutMs: Long, pos: Int, predicate: (E) -> Boolean): Int {
        val deadline = System.currentTimeMillis() + timeoutMs
        var index = pos
        do {
            while (index < elements.size) {
                if (predicate(elements[index])) return index
                ++index
            }
        } while (condition.await(deadline - System.currentTimeMillis()))
        return -1
    }

    /**
     * Returns a ReadHead over this ArrayTrackRecord. The returned ReadHead is tied to the
     * current thread.
     */
    fun newReadHead() = ReadHead()

    /**
     * ReadHead is an object that helps users of ArrayTrackRecord keep track of how far
     * it has read this far in the ArrayTrackRecord. A ReadHead is always associated with
     * a single instance of ArrayTrackRecord. Multiple ReadHeads can be created and used
     * on the same instance of ArrayTrackRecord concurrently, and the ArrayTrackRecord
     * instance can also be used concurrently. ReadHead maintains the current index that is
     * the next to be read, and calls this the "mark".
     *
     * A ReadHead delegates all TrackRecord methods to its associated ArrayTrackRecord, and
     * inherits its thread-safe properties. However, the additional methods that ReadHead
     * offers on top of TrackRecord do not share these properties and can only be used by
     * the thread that created the ReadHead. This is because by construction it does not
     * make sense to use a ReadHead on multiple threads concurrently (see below for details).
     *
     * In a ReadHead, {@link poll(Long, (E) -> Boolean)} works similarly to a LinkedBlockingQueue.
     * It can be called repeatedly and will return the elements as they arrive.
     *
     * Intended usage looks something like this :
     * val TrackRecord<MyObject> record = ArrayTrackRecord().newReadHead()
     * Thread().start {
     *   // do stuff
     *   record.add(something)
     *   // do stuff
     * }
     *
     * val obj1 = record.poll(timeout)
     * // do something with obj1
     * val obj2 = record.poll(timeout)
     * // do something with obj2
     *
     * The point is that the caller does not have to track the mark like it would have to if
     * it was using ArrayTrackRecord directly.
     *
     * Note that if multiple threads were using poll() concurrently on the same ReadHead, what
     * happens to the mark and the return values could be well defined, but it could not
     * be useful because there is no way to provide either a guarantee not to skip objects nor
     * a guarantee about the mark position at the exit of poll(). This is even more true in the
     * presence of a predicate to filter returned elements, because one thread might be
     * filtering out the events the other is interested in.
     * Instead, this use case is supported by creating multiple ReadHeads on the same instance
     * of ArrayTrackRecord. Each ReadHead is then guaranteed to see all events always and
     * guarantees are made on the value of the mark upon return. {@see poll(Long, (E) -> Boolean)}
     * for details. Be careful to create each ReadHead on the thread it is meant to be used on.
     *
     * Users of a ReadHead can ask for the current position of the mark at any time. This mark
     * can be used later to replay the history of events either on this ReadHead, on the associated
     * ArrayTrackRecord or on another ReadHead associated with the same ArrayTrackRecord. It
     * might look like this in the reader thread :
     *
     * val markAtStart = record.mark
     * // Start processing interesting events
     * while (val element = record.poll(timeout) { it.isInteresting() }) {
     *   // Do something with element
     * }
     * // Look for stuff that happened while searching for interesting events
     * val firstElementReceived = record.getOrNull(markAtStart)
     * val firstSpecialElement = record.getOrNull(markAtStart) { it.isSpecial() }
     * // Get the first special element since markAtStart, possibly blocking until one is available
     * val specialElement = record.poll(timeout, markAtStart) { it.isSpecial() }
     */
    inner class ReadHead : TrackRecord<E> by this@ArrayTrackRecord {
        private val owningThread = Thread.currentThread()
        private var readHead = 0

        /**
         * @return the current value of the mark.
         */
        var mark
            get() = readHead.also { checkThread() }
            set(v: Int) = rewind(v)
        fun rewind(v: Int) {
            checkThread()
            readHead = v
        }

        private fun checkThread() = check(Thread.currentThread() == owningThread) {
            "Must be called by the thread that created this object"
        }

        /**
         * Returns the first element after the mark, optionally blocking until one is available, or
         * null if no such element can be found within the timeout.
         * If a predicate is given, only elements matching the predicate are returned.
         *
         * Upon return the mark will be set to immediately after the returned element, or after
         * the last element in the queue if null is returned. This means this method will always
         * skip elements that do not match the predicate, even if it returns null.
         *
         * This method can only be used by the thread that created this ManagedRecordingQueue.
         * If used on another thread, this throws IllegalStateException.
         *
         * @param timeoutMs how long, in milliseconds, to wait at most (best effort approximation).
         * @param predicate an optional predicate to filter elements to be returned.
         * @return an element matching the predicate, or null if timeout.
         */
        fun poll(timeoutMs: Long, predicate: (E) -> Boolean = { true }): E? {
            checkThread()
            lock.withLock {
                val index = pollForIndexReadLocked(timeoutMs, readHead, predicate)
                readHead = if (index < 0) size else index + 1
                return getOrNull(index)
            }
        }

        /**
         * Returns the first element after the mark or null. This never blocks.
         *
         * This method can only be used by the thread that created this ManagedRecordingQueue.
         * If used on another thread, this throws IllegalStateException.
         */
        fun peek(): E? = getOrNull(readHead).also { checkThread() }
    }
}

// Private helper
private fun Condition.await(timeoutMs: Long) = this.await(timeoutMs, TimeUnit.MILLISECONDS)
