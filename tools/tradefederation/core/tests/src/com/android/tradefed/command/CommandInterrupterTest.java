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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.util.RunInterruptedException;

import com.google.common.base.Stopwatch;
import com.google.common.base.Throwables;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link CommandInterrupter} */
@RunWith(JUnit4.class)
public class CommandInterrupterTest {

    private static final String MESSAGE = "message";

    private CommandInterrupter mInterrupter;

    @Before
    public void setUp() {
        mInterrupter = new CommandInterrupter();
    }

    @Test
    public void testAllowInterrupt() throws InterruptedException {
        execute(
                () -> {
                    // interrupts initially blocked
                    assertFalse(mInterrupter.isInterruptible());

                    // thread can be made interruptible
                    mInterrupter.allowInterrupt();
                    assertTrue(mInterrupter.isInterruptible());
                });
    }

    @Test
    public void testInterrupt() throws InterruptedException {
        execute(
                () -> {
                    // flag thread for interruption
                    mInterrupter.allowInterrupt();
                    mInterrupter.interrupt(Thread.currentThread(), MESSAGE);
                    assertTrue(Thread.interrupted());

                    try {
                        // will be interrupted
                        mInterrupter.checkInterrupted();
                        fail("RunInterruptedException was expected");
                    } catch (RunInterruptedException e) {
                        assertEquals(MESSAGE, e.getMessage());
                    }
                });
    }

    @Test
    public void testInterrupt_blocked() throws InterruptedException {
        execute(
                () -> {
                    // block interrupts, but flag for interruption
                    mInterrupter.blockInterrupt();
                    mInterrupter.interrupt(Thread.currentThread(), MESSAGE);
                    assertFalse(Thread.interrupted());

                    // not interrupted
                    mInterrupter.checkInterrupted();

                    try {
                        // will be interrupted once interrupts are allowed
                        mInterrupter.allowInterrupt();
                        fail("RunInterruptedException was expected");
                    } catch (RunInterruptedException e) {
                        assertEquals(MESSAGE, e.getMessage());
                    }
                });
    }

    @Test
    public void testInterrupt_clearsFlag() throws InterruptedException {
        execute(
                () -> {
                    // flag thread for interruption
                    mInterrupter.allowInterrupt();
                    mInterrupter.interrupt(Thread.currentThread(), MESSAGE);
                    assertTrue(Thread.interrupted());

                    try {
                        // interrupt the thread
                        mInterrupter.checkInterrupted();
                        fail("RunInterruptedException was expected");
                    } catch (RunInterruptedException e) {
                        // ignore
                    }

                    // interrupt flag was cleared, exception no longer thrown
                    mInterrupter.checkInterrupted();
                });
    }

    @Test
    public void testAllowInterruptAsync() throws InterruptedException {
        execute(
                () -> {
                    // allow interruptions after a delay
                    Stopwatch stopwatch = Stopwatch.createStarted();
                    Future<?> future =
                            mInterrupter.allowInterruptAsync(
                                    Thread.currentThread(), 200L, TimeUnit.MILLISECONDS);

                    // not yet marked as interruptible
                    assertFalse(mInterrupter.isInterruptible());

                    try {
                        // marked as interruptible after enough time has passed
                        future.get(500L, TimeUnit.MILLISECONDS);
                        assertTrue(mInterrupter.isInterruptible());
                        assertTrue(stopwatch.elapsed(TimeUnit.MILLISECONDS) >= 200L);

                    } catch (InterruptedException | ExecutionException | TimeoutException e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    @Test
    public void testAllowInterruptsAsync_alreadyAllowed() throws InterruptedException {
        execute(
                () -> {
                    // interrupts allowed
                    mInterrupter.allowInterrupt();

                    // unchanged after asynchronously allowing interrupt
                    Future<?> future =
                            mInterrupter.allowInterruptAsync(
                                    Thread.currentThread(), 200L, TimeUnit.MILLISECONDS);
                    assertTrue(future.isDone());
                    assertTrue(mInterrupter.isInterruptible());
                });
    }

    // Execute test in separate thread
    private static void execute(Runnable runnable) throws InterruptedException {
        AtomicReference<Throwable> throwableRef = new AtomicReference<>();

        Thread thread = new Thread(runnable, "CommandInterrupterTest");
        thread.setDaemon(true);
        thread.setUncaughtExceptionHandler((t, throwable) -> throwableRef.set(throwable));
        thread.start();
        thread.join();

        Throwable throwable = throwableRef.get();
        if (throwable != null) {
            throw Throwables.propagate(throwable);
        }
    }
}
