/*
 * Copyright (C) 2020 The Android Open Source Project
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
 * limitations under the License
 */

package org.conscrypt;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import androidx.test.InstrumentationRegistry;
import androidx.test.internal.runner.listener.InstrumentationRunListener;
import com.android.org.conscrypt.Conscrypt;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.Objects;
import javax.net.ssl.SSLSocketFactory;
import org.junit.runner.Description;
import org.junit.runner.Result;

/**
 * An @link(InstrumentationRunListener) which can be used in CTS tests to control the
 * implementation of @link{SSLSocket} returned by Conscrypt, allowing both implementations
 * to be tested using the same test classes.
 *
 * This listener looks for an instrumentation arg named "conscrypt_sslsocket_implementation".
 * If its value is "fd" then the file descriptor based implementation will be used, or
 * if its value is "engine" then the SSLEngine based implementation will be used. Any other
 * value is invalid.
 *
 * The default is set from an @code{testRunStarted} method, i.e. before any tests run and
 * persists until the ART VM exits, i.e. until all tests in this @code{<test>} clause complete.
 */
public class ConscryptInstrumentationListener extends InstrumentationRunListener {
    private static final String IMPLEMENTATION_ARG_NAME = "conscrypt_sslsocket_implementation";
    private static final String LOG_TAG = "ConscryptInstList";
    // Signal used to trigger a dump of Clang coverage information.
    // See {@code maybeDumpNativeCoverage} below.
    private final int COVERAGE_SIGNAL = 37;

    private enum Implementation {
        ENGINE(true, "com.android.org.conscrypt.ConscryptEngineSocket"),
        FD(false, "com.android.org.conscrypt.ConscryptFileDescriptorSocket");

        private final boolean useEngine;
        private final String expectedClassName;

        private Implementation(boolean useEngine, String expectedClassName) {
            this.useEngine = useEngine;
            this.expectedClassName = expectedClassName;
        }

        private boolean shouldUseEngine() {
            return useEngine;
        }

        private Class<? extends Socket> getExpectedClass() {
            try {
                return Class.forName(expectedClassName).asSubclass(Socket.class);
            } catch (ClassNotFoundException e) {
                throw new IllegalStateException(
                        "Invalid SSLSocket class: '" + expectedClassName + "'");
            }
        }

        private static Implementation lookup(String name) {
            try {
                return valueOf(name.toUpperCase());
            } catch (Exception e) {
                throw new IllegalArgumentException(
                        "Invalid SSLSocket implementation: '" + name + "'");
            }
        }
    }

    @Override
    public void testRunStarted(Description description) throws Exception {
        Bundle argsBundle = InstrumentationRegistry.getArguments();
        String implementationName = argsBundle.getString(IMPLEMENTATION_ARG_NAME);
        Implementation implementation = Implementation.lookup(implementationName);
        selectImplementation(implementation);
        super.testRunStarted(description);
    }

    @Override
    public void testRunFinished(Result result) throws Exception {
        maybeDumpNativeCoverage();
        super.testRunFinished(result);
    }

    /**
     * If this test process is instrumented for native coverage, then trigger a dump
     * of the coverage data and wait until either we detect the dumping has finished or 60 seconds,
     * whichever is shorter.
     *
     * Background: Coverage builds install a signal handler for signal 37 which flushes coverage
     * data to disk, which may take a few seconds.  Tests running as an app process will get
     * killed with SIGKILL once the app code exits, even if the coverage handler is still running.
     *
     * Method: If a handler is installed for signal 37, then assume this is a coverage run and
     * send signal 37.  The handler is non-reentrant and so signal 37 will then be blocked until
     * the handler completes. So after we send the signal, we loop checking the blocked status
     * for signal 37 until we hit the 60 second deadline.  If the signal is blocked then sleep for
     * 2 seconds, and if it becomes unblocked then the handler exitted so we can return early.
     * If the signal is not blocked at the start of the loop then most likely the handler has
     * not yet been invoked.  This should almost never happen as it should get blocked on delivery
     * when we call {@code Os.kill()}, so sleep for a shorter duration (100ms) and try again.  There
     * is a race condition here where the handler is delayed but then runs for less than 100ms and
     * gets missed, in which case this method will loop with 100ms sleeps until the deadline.
     *
     * In the case where the handler runs for more than 60 seconds, the test process will be allowed
     * to exit so coverage information may be incomplete.
     *
     * There is no API for determining signal dispositions, so this method uses the
     * {@link SignalMaskInfo} class to read the data from /proc.  If there is an error parsing
     * the /proc data then this method will also loop until the 60s deadline passes.
     *
     */
    private void maybeDumpNativeCoverage() {
        SignalMaskInfo siginfo = new SignalMaskInfo();
        if (!siginfo.isValid()) {
            Log.e(LOG_TAG, "Invalid signal info");
            return;
        }

        if (!siginfo.isCaught(COVERAGE_SIGNAL)) {
            // Process is not instrumented for coverage
            Log.i(LOG_TAG, "Not dumping coverage, no handler installed");
            return;
        }

        Log.i(LOG_TAG,
                String.format("Sending coverage dump signal %d to pid %d uid %d", COVERAGE_SIGNAL,
                        Os.getpid(), Os.getuid()));
        try {
            Os.kill(Os.getpid(), COVERAGE_SIGNAL);
        } catch (ErrnoException e) {
            Log.e(LOG_TAG, "Unable to send coverage signal", e);
            return;
        }

        long start = System.currentTimeMillis();
        long deadline = start + 60 * 1000L;
        while (System.currentTimeMillis() < deadline) {
            siginfo.refresh();
            try {
                if (siginfo.isValid() && siginfo.isBlocked(COVERAGE_SIGNAL)) {
                    // Signal is currently blocked so assume a handler is running
                    Thread.sleep(2000L);
                    siginfo.refresh();
                    if (siginfo.isValid() && !siginfo.isBlocked(COVERAGE_SIGNAL)) {
                        // Coverage handler exited while we were asleep
                        Log.i(LOG_TAG,
                                String.format("Coverage dump detected finished after %dms",
                                        System.currentTimeMillis() - start));
                        break;
                    }
                } else {
                    // Coverage signal handler not yet started or invalid siginfo
                    Thread.sleep(100L);
                }
            } catch (InterruptedException e) {
                // ignored
            }
        }
    }

    private void selectImplementation(Implementation implementation) {
        // Invoke setUseEngineSocketByDefault by reflection as it is an "ExperimentalApi which is
        // not visible to tests.
        try {
            Method method =
                    Conscrypt.class.getDeclaredMethod("setUseEngineSocketByDefault", boolean.class);
            method.invoke(null, implementation.shouldUseEngine());
        } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
            throw new IllegalStateException("Unable to set SSLSocket implementation", e);
        }

        // Verify that the default socket factory returns the expected implementation class for
        // SSLSocket or, more likely, a subclass of it.
        Socket socket;
        try {
            socket = SSLSocketFactory.getDefault().createSocket();
        } catch (IOException e) {
            throw new IllegalStateException("Unable to create an SSLSocket", e);
        }

        Objects.requireNonNull(socket);
        Class<? extends Socket> expectedClass = implementation.getExpectedClass();
        if (!expectedClass.isAssignableFrom(socket.getClass())) {
            throw new IllegalArgumentException("Expected SSLSocket class or subclass of "
                    + expectedClass.getSimpleName() + " but got "
                    + socket.getClass().getSimpleName());
        }
    }
}
