/*
 * Copyright (C) 2010 The Android Open Source Project
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

package libcore.java.lang;

import dalvik.system.InMemoryDexClassLoader;

import java.io.InputStream;
import java.lang.reflect.Method;
import java.lang.Thread.UncaughtExceptionHandler;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.locks.ReentrantLock;

import junit.framework.Assert;
import junit.framework.TestCase;

import org.mockito.InOrder;
import org.mockito.Mockito;

import libcore.io.Streams;
import libcore.java.lang.ref.FinalizationTester;

public final class ThreadTest extends TestCase {
    static {
        System.loadLibrary("javacoretests");
    }

    /**
     * getContextClassLoader returned a non-application class loader.
     * http://code.google.com/p/android/issues/detail?id=5697
     */
    public void testJavaContextClassLoader() throws Exception {
        Assert.assertNotNull("Must have a Java context ClassLoader",
                Thread.currentThread().getContextClassLoader());
    }

    public void testLeakingStartedThreads() {
        final AtomicInteger finalizedThreadsCount = new AtomicInteger();
        for (int i = 0; true; i++) {
            try {
                newThread(finalizedThreadsCount, 1024 << i).start();
            } catch (OutOfMemoryError expected) {
                break;
            }
        }
        FinalizationTester.induceFinalization();
        assertTrue("Started threads were never finalized!", finalizedThreadsCount.get() > 0);
    }

    public void testLeakingUnstartedThreads() {
        final AtomicInteger finalizedThreadsCount = new AtomicInteger();
        for (int i = 0; true; i++) {
            try {
                newThread(finalizedThreadsCount, 1024 << i);
            } catch (OutOfMemoryError expected) {
                break;
            }
        }
        FinalizationTester.induceFinalization();
        assertTrue("Unstarted threads were never finalized!", finalizedThreadsCount.get() > 0);
    }

    public void testThreadSleep() throws Exception {
        int millis = 1000;
        long start = System.currentTimeMillis();

        Thread.sleep(millis);

        long elapsed = System.currentTimeMillis() - start;
        long offBy = Math.abs(elapsed - millis);

        assertTrue("Actual sleep off by " + offBy + " ms", offBy <= 250);
    }

    public void testThreadInterrupted() throws Exception {
        Thread.currentThread().interrupt();
        try {
            Thread.sleep(0);
            fail();
        } catch (InterruptedException e) {
            assertFalse(Thread.currentThread().isInterrupted());
        }
    }

    public void testThreadSleepIllegalArguments() throws Exception {

        try {
            Thread.sleep(-1);
            fail();
        } catch (IllegalArgumentException expected) {
        }

        try {
            Thread.sleep(0, -1);
            fail();
        } catch (IllegalArgumentException expected) {
        }

        try {
            Thread.sleep(0, 1000000);
            fail();
        } catch (IllegalArgumentException expected) {
        }
    }

    public void testThreadWakeup() throws Exception {
        WakeupTestThread t1 = new WakeupTestThread();
        WakeupTestThread t2 = new WakeupTestThread();

        t1.start();
        t2.start();
        assertTrue("Threads already finished", !t1.done && !t2.done);

        t1.interrupt();
        t2.interrupt();

        Thread.sleep(1000);
        assertTrue("Threads did not finish", t1.done && t2.done);
    }

    public void testContextClassLoaderIsNotNull() {
        assertNotNull(Thread.currentThread().getContextClassLoader());
    }

    public void testContextClassLoaderIsInherited() {
        Thread other = new Thread();
        assertSame(Thread.currentThread().getContextClassLoader(), other.getContextClassLoader());
    }

    public void testSetPriority_unstarted() throws Exception {
        Thread thread = new Thread();
        checkSetPriority_inBounds_succeeds(thread);
        checkSetPriority_outOfBounds_fails(thread);
    }

    public void testSetPriority_starting() throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        Thread thread = new Thread("starting thread") {
            @Override public void run() { try { latch.await(); } catch (Exception e) { } }
        };
        // priority set while thread was not started should carry over to started thread
        int priority = thread.getPriority() + 1;
        if (priority > Thread.MAX_PRIORITY) {
            priority = Thread.MIN_PRIORITY;
        }
        thread.setPriority(priority);
        thread.start();
        assertEquals(priority, thread.getPriority());
        latch.countDown();
        thread.join();
    }

    public void testSetPriority_started() throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        Thread startedThread = new Thread("started thread") {
            @Override public void run() { try { latch.await(); } catch (Exception e) { } }
        };
        startedThread.start();
        checkSetPriority_inBounds_succeeds(startedThread);
        checkSetPriority_outOfBounds_fails(startedThread);
        latch.countDown();
        startedThread.join();
    }

    public void testSetPriority_joined() throws Exception {
        Thread joinedThread = new Thread();
        joinedThread.start();
        joinedThread.join();

        int originalPriority = joinedThread.getPriority();
        for (int p = Thread.MIN_PRIORITY; p <= Thread.MAX_PRIORITY; p++) {
            joinedThread.setPriority(p);
            // setting the priority of a not-alive Thread should not succeed
            assertEquals(originalPriority, joinedThread.getPriority());
        }
        checkSetPriority_outOfBounds_fails(joinedThread);
    }

    private static void checkSetPriority_inBounds_succeeds(Thread thread) {
        int oldPriority = thread.getPriority();
        try {
            for (int priority = Thread.MIN_PRIORITY; priority <= Thread.MAX_PRIORITY; priority++) {
                thread.setPriority(priority);
                assertEquals(priority, thread.getPriority());
            }
        } finally {
            thread.setPriority(oldPriority);
        }
        assertEquals(oldPriority, thread.getPriority());
    }

    private static void checkSetPriority_outOfBounds_fails(Thread thread) {
        checkSetPriority_outOfBounds_fails(thread, Thread.MIN_PRIORITY - 1);
        checkSetPriority_outOfBounds_fails(thread, Thread.MAX_PRIORITY + 1);
        checkSetPriority_outOfBounds_fails(thread, Integer.MIN_VALUE);
        checkSetPriority_outOfBounds_fails(thread, Integer.MAX_VALUE);
    }

    private static void checkSetPriority_outOfBounds_fails(Thread thread, int invalidPriority) {
        int oldPriority = thread.getPriority();
        try {
            thread.setPriority(invalidPriority);
            fail();
        } catch (IllegalArgumentException expected) {
        }
        assertEquals(oldPriority, thread.getPriority()); // priority shouldn't have changed
    }

    public void testUncaughtExceptionPreHandler_calledBeforeDefaultHandler() {
        UncaughtExceptionHandler initialHandler = Mockito.mock(UncaughtExceptionHandler.class);
        UncaughtExceptionHandler defaultHandler = Mockito.mock(UncaughtExceptionHandler.class);
        InOrder inOrder = Mockito.inOrder(initialHandler, defaultHandler);

        UncaughtExceptionHandler originalDefaultHandler
                = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setUncaughtExceptionPreHandler(initialHandler);
        Thread.setDefaultUncaughtExceptionHandler(defaultHandler);
        try {
            Thread t = new Thread();
            Throwable e = new Throwable();
            t.dispatchUncaughtException(e);
            inOrder.verify(initialHandler).uncaughtException(t, e);
            inOrder.verify(defaultHandler).uncaughtException(t, e);
            inOrder.verifyNoMoreInteractions();
        } finally {
            Thread.setDefaultUncaughtExceptionHandler(originalDefaultHandler);
            Thread.setUncaughtExceptionPreHandler(null);
        }
    }

    public void testUncaughtExceptionPreHandler_noDefaultHandler() {
        UncaughtExceptionHandler initialHandler = Mockito.mock(UncaughtExceptionHandler.class);
        UncaughtExceptionHandler originalDefaultHandler
                = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setUncaughtExceptionPreHandler(initialHandler);
        Thread.setDefaultUncaughtExceptionHandler(null);
        try {
            Thread t = new Thread();
            Throwable e = new Throwable();
            t.dispatchUncaughtException(e);
            Mockito.verify(initialHandler).uncaughtException(t, e);
            Mockito.verifyNoMoreInteractions(initialHandler);
        } finally {
            Thread.setDefaultUncaughtExceptionHandler(originalDefaultHandler);
            Thread.setUncaughtExceptionPreHandler(null);
        }
    }

    /**
     * Thread.getStackTrace() is broken. http://b/1252043
     */
    public void testGetStackTrace() throws Exception {
        Thread t1 = new Thread("t1") {
            @Override public void run() {
                doSomething();
            }
            public void doSomething() {
                try {
                    Thread.sleep(4000);
                } catch (InterruptedException ignored) {
                }
            }
        };
        t1.start();
        Thread.sleep(1000);
        StackTraceElement[] traces = t1.getStackTrace();
        StackTraceElement trace = traces[traces.length - 2];

        // Expect to find MyThread.doSomething in the trace
        assertTrue(trace.getClassName().contains("ThreadTest")
                && trace.getMethodName().equals("doSomething"));
        t1.join();
    }

    /**
     * Checks that a stacktrace reports the expected debug metadata
     * (source-filename, line number) hard-coded in a class loaded from
     * pre-built test resources.
     */
    public void testGetStackTrace_debugInfo() throws Exception {
        StackTraceElement ste = getStackTraceElement("debugInfo");

        // Verify that this StackTraceElement appears as we expect it to
        // e.g. when an exception is printed.
        assertEquals("java.lang.ThreadTestHelper.debugInfo(ThreadTestHelper.java:9)",
                ste.toString());

        // Since we emit debug information for ThreadTestHelper.debugInfo,
        // the Runtime will symbolicate this frame with the correct file name.
        assertEquals("ThreadTestHelper.java", ste.getFileName());

        // We explicitly specify this in the test resource.
        assertEquals(9, ste.getLineNumber());
    }

    /**
     * Checks that a stacktrace reports the expected dex PC in place of
     * a line number when debug info is missing for a method; the method is
     * declared on a class loaded from pre-built test resources.
     */
    public void testGetStackTrace_noDebugInfo() throws Exception {
        StackTraceElement ste = getStackTraceElement("noDebugInfo");

        // Verify that this StackTraceElement appears as we expect it to
        // e.g. when an exception is printed.
        assertEquals("java.lang.ThreadTestHelper.noDebugInfo(Unknown Source:3)", ste.toString());

        // Since we don't have any debug info for this method, the Runtime
        // doesn't symbolicate this with a file name (even though the
        // enclosing class may have the file name specified).
        assertEquals(null, ste.getFileName());

        // In the test resource we emit 3 nops before generating a stack
        // trace; each nop advances the dex PC by 1 because a nop is a
        // single code unit wide.
        assertEquals(3, ste.getLineNumber());
    }

    /**
     * Calls the given static method declared on ThreadTestHelper, which
     * is loaded from precompiled test resources.
     *
     * @param methodName either {@quote "debugInfo"} or {@quote "noDebugInfo"}
     * @return the StackTraceElement corresponding to said method's frame
     */
    private static StackTraceElement getStackTraceElement(String methodName) throws Exception {
        final String className = "java.lang.ThreadTestHelper";
        byte[] data;
        try (InputStream is =
                ThreadTest.class.getClassLoader().getResourceAsStream("core-tests-smali.dex")) {
            data = Streams.readFullyNoClose(is);
        }
        ClassLoader imcl = new InMemoryDexClassLoader(ByteBuffer.wrap(data),
                ThreadTest.class.getClassLoader());
        Class<?> helper = imcl.loadClass(className);
        Method m = helper.getDeclaredMethod(methodName);
        StackTraceElement[] stes = (StackTraceElement[]) m.invoke(null);

        // The top of the stack trace looks like:
        // - VMStack.getThreadStackTrace()
        // - Thread.getStackTrace()
        // - ThreadTestHelper.createStackTrace()
        // - ThreadTestHelper.{debugInfo,noDebugInfo}
        StackTraceElement result = stes[3];

        // Sanity check before we return
        assertEquals(result.getClassName(), className);
        assertEquals(result.getMethodName(), methodName);
        assertFalse(result.isNativeMethod());
        return result;
    }

    public void testGetAllStackTracesIncludesAllGroups() throws Exception {
        final AtomicInteger visibleTraces = new AtomicInteger();
        ThreadGroup group = new ThreadGroup("1");
        Thread t2 = new Thread(group, "t2") {
            @Override public void run() {
                visibleTraces.set(Thread.getAllStackTraces().size());
            }
        };
        t2.start();
        t2.join();

        // Expect to see the traces of all threads (not just t2)
        assertTrue("Must have traces for all threads", visibleTraces.get() > 1);
    }

    // http://b/27748318
    public void testNativeThreadNames() throws Exception {
        String testResult = nativeTestNativeThreadNames();
        // Not using assertNull here because this results in a better error message.
        if (testResult != null) {
            fail(testResult);
        }
    }

    // http://b/29746125
    public void testParkUntilWithUnderflowValue() throws Exception {
        final Thread current = Thread.currentThread();

        // watchdog to unpark the tread in case it will be parked
        AtomicBoolean afterPark = new AtomicBoolean(false);
        AtomicBoolean wasParkedForLongTime = new AtomicBoolean(false);
        Thread watchdog = new Thread() {
            @Override public void run() {
                try {
                    sleep(5000);
                } catch(InterruptedException expected) {}

                if (!afterPark.get()) {
                    wasParkedForLongTime.set(true);
                    LockSupport.unpark(current);
                }
            }
        };
        watchdog.start();

        // b/29746125 is caused by underflow: parkUntilArg - System.currentTimeMillis() > 0.
        // parkUntil$ should return immediately for everyargument that's <=
        // System.currentTimeMillis().
        LockSupport.parkUntil(Long.MIN_VALUE);
        if (wasParkedForLongTime.get()) {
            fail("Current thread was parked, but was expected to return immediately");
        }
        afterPark.set(true);
        watchdog.interrupt();
        watchdog.join();
    }

    /**
     * Check that call Thread.start for already started thread
     * throws {@code IllegalThreadStateException}
     */
    public void testThreadDoubleStart() {
        final ReentrantLock lock = new ReentrantLock();
        Thread thread = new Thread() {
            public void run() {
                // Lock should be acquired by the main thread and
                // this thread should block on this operation.
                lock.lock();
            }
        };
        // Acquire lock to ensure that new thread is not finished
        // when we call start() second time.
        lock.lock();
        try {
            thread.start();
            try {
                thread.start();
                fail();
            } catch (IllegalThreadStateException expected) {
            }
        } finally {
            lock.unlock();
        }
        try {
            thread.join();
        } catch (InterruptedException ignored) {
        }
    }

    /**
     * Check that call Thread.start for already finished thread
     * throws {@code IllegalThreadStateException}
     */
    public void testThreadRestart() {
        Thread thread = new Thread();
        thread.start();
        try {
            thread.join();
        } catch (InterruptedException ignored) {
        }
        try {
            thread.start();
            fail();
        } catch (IllegalThreadStateException expected) {
        }
    }

    // This method returns {@code null} if all tests pass, or a non-null String containing
    // failure details if an error occured.
    private static native String nativeTestNativeThreadNames();

    private Thread newThread(final AtomicInteger finalizedThreadsCount, final int size) {
        return new Thread() {
            long[] memoryPressure = new long[size];
            @Override protected void finalize() throws Throwable {
                super.finalize();
                finalizedThreadsCount.incrementAndGet();
            }
        };
    }

    private class WakeupTestThread extends Thread {
        public boolean done;

        public void run() {
            done = false;

            // Sleep for a while (1 min)
            try {
                Thread.sleep(60000);
            } catch (InterruptedException ignored) {
            }

            done = true;
        }
    }
}
