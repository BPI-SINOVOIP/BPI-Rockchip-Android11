/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tradefed.command;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.RunInterruptedException;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.MapMaker;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nonnull;

/** Service allowing TradeFederation commands to be interrupted or marked as uninterruptible. */
public class CommandInterrupter {

    /** Singleton. */
    public static final CommandInterrupter INSTANCE = new CommandInterrupter();

    private final ScheduledExecutorService mExecutor = Executors.newScheduledThreadPool(0);

    // tracks whether a thread is currently interruptible
    private ConcurrentMap<Thread, Boolean> mInterruptible = new MapMaker().weakKeys().makeMap();
    // presence of an interrupt error message indicates that the thread should be interrupted
    private ConcurrentMap<Thread, String> mInterruptMessage = new MapMaker().weakKeys().makeMap();

    @VisibleForTesting
    // FIXME: reduce visibility once RunUtil interrupt tests are removed
    public CommandInterrupter() {}

    /** Allow current thread to be interrupted. */
    public void allowInterrupt() {
        CLog.d("Interrupt allowed");
        mInterruptible.put(Thread.currentThread(), true);
        checkInterrupted();
    }

    /** Prevent current thread from being interrupted. */
    public void blockInterrupt() {
        CLog.d("Interrupt blocked");
        mInterruptible.put(Thread.currentThread(), false);
        checkInterrupted();
    }

    /** @return true if current thread is interruptible */
    public boolean isInterruptible() {
        return isInterruptible(Thread.currentThread());
    }

    /** @return true if specified thread is interruptible */
    public boolean isInterruptible(@Nonnull Thread thread) {
        return Boolean.TRUE.equals(mInterruptible.get(thread));
    }

    /**
     * Allow a specified thread to be interrupted after a delay.
     *
     * @param thread thread to mark as interruptible
     * @param delay time from now to delay execution
     * @param unit time unit of the delay parameter
     */
    // FIXME: reduce visibility once RunUtil interrupt methods are removed
    public Future<?> allowInterruptAsync(
            @Nonnull Thread thread, long delay, @Nonnull TimeUnit unit) {
        if (isInterruptible(thread)) {
            CLog.v("Thread already interruptible");
            return CompletableFuture.completedFuture(null);
        }

        CLog.w("Allowing interrupt in %d ms", unit.toMillis(delay));
        return mExecutor.schedule(
                () -> {
                    CLog.e("Interrupt allowed asynchronously");
                    mInterruptible.put(thread, true);
                },
                delay,
                unit);
    }

    /**
     * Flag a thread, interrupting it if and when it becomes interruptible.
     *
     * @param thread thread to mark for interruption
     * @param message interruption message
     */
    // FIXME: reduce visibility once RunUtil interrupt methods are removed
    public void interrupt(@Nonnull Thread thread, @Nonnull String message) {
        if (message == null) {
            throw new IllegalArgumentException("message cannot be null.");
        }
        mInterruptMessage.put(thread, message);
        if (isInterruptible(thread)) {
            thread.interrupt();
        }
    }

    /**
     * Interrupts the current thread if it should be interrupted. Threads are encouraged to
     * periodically call this method in order to throw the right {@link RunInterruptedException}.
     */
    public void checkInterrupted() throws RunInterruptedException {
        Thread thread = Thread.currentThread();
        if (isInterruptible()) {
            String message = mInterruptMessage.remove(thread);
            if (message != null) {
                throw new RunInterruptedException(message);
            }
        }
    }
}
