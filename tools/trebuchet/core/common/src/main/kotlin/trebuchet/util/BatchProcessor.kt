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

package trebuchet.util

import java.io.Closeable
import java.util.*
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.Callable
import java.util.concurrent.CompletableFuture
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import kotlin.collections.ArrayList
import kotlin.concurrent.thread
import kotlin.system.measureNanoTime
import kotlin.system.measureTimeMillis

fun ns2ms(ns: Long) = ns / 1000000

class WorkQueue {
    private val workArray = ArrayDeque<Runnable?>(8192)
    private val _finished = AtomicBoolean(false)

    private val finished get() = _finished.get()
    fun finish() = _finished.set(true)

    private fun spinLoop(duration: Long) {
        val loopUntil = System.nanoTime() + duration
        while (System.nanoTime() < loopUntil) {}
    }

    fun submit(task: Runnable) {
        synchronized(workArray) {
            workArray.add(task)
        }
    }

    fun processAll() {
        while (!finished) {
            var next: Runnable?
            do {
                synchronized(workArray) {
                    next = workArray.poll()
                }
                if (next == null) {
                    spinLoop(10000)
                    synchronized(workArray) {
                        next = workArray.poll()
                    }
                }
                next?.run()
            } while (next != null)
            spinLoop(60000)
        }

        var remaining: Runnable?
        do {
            synchronized(workArray) {
                remaining = workArray.poll()
            }
            remaining?.run()
        } while (remaining != null)
    }
}

private val ThreadCount = minOf(10, Runtime.getRuntime().availableProcessors())

class WorkPool : Closeable {

    private var closed = false
    private val workQueue = WorkQueue()
    private val threadPool = Array(ThreadCount) {
        Thread { workQueue.processAll() }.apply { start() }
    }

    fun submit(task: Runnable) {
        workQueue.submit(task)
    }

    override fun close() {
        if (closed) return
        closed = true
        workQueue.finish()
        threadPool.forEach { it.join() }
    }
}

fun<T, U, S> par_map(iterator: Iterator<T>, threadState: () -> S, chunkSize: Int = 50,
                     map: (S, T) -> U): Iterator<U> {
    val endOfStreamMarker = CompletableFuture<List<U>>()
    val resultPipe = ArrayBlockingQueue<Future<List<U>>>(1024, false)
    thread {
        val threadLocal = ThreadLocal.withInitial { threadState() }
        WorkPool().use { pool ->
            while (iterator.hasNext()) {
                val source = ArrayList<T>(chunkSize)
                while (source.size < chunkSize && iterator.hasNext()) {
                    source.add(iterator.next())
                }
                val future = CompletableFuture<List<U>>()
                pool.submit(Runnable {
                    val state = threadLocal.get()
                    future.complete(source.map { map(state, it) })
                })
                resultPipe.put(future)
            }
            resultPipe.put(endOfStreamMarker)
        }
    }

    return object : Iterator<U> {
        var current: Iterator<U>? = null

        init {
            takeNext()
        }

        private fun takeNext() {
            val next = resultPipe.take()
            if (next != endOfStreamMarker) {
                current = next.get().iterator()
            }
        }

        override fun next(): U {
            val ret = current!!.next()
            if (!current!!.hasNext()) {
                current = null
                takeNext()
            }
            return ret
        }

        override fun hasNext(): Boolean {
            return current != null
        }

    }
}