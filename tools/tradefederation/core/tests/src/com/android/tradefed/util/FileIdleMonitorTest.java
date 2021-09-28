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
package com.android.tradefed.util;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.longThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.time.Duration;
import java.util.Arrays;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link FileIdleMonitor}. */
@RunWith(JUnit4.class)
public class FileIdleMonitorTest {

    private static final Duration TIMEOUT = Duration.ofSeconds(10L);

    private ScheduledExecutorService mExecutor;
    private Runnable mCallback;

    @Before
    public void setUp() {
        mExecutor = mock(ScheduledExecutorService.class);
        mCallback = mock(Runnable.class);
    }

    @Test
    public void testSchedulesCheckIfNoTimestamp() {
        // no modification timestamp found
        FileIdleMonitor monitor = createMonitor();
        monitor.start();

        // callback not executed, and new idle check scheduled after TIMEOUT
        verifyZeroInteractions(mCallback);
        verify(mExecutor, times(1))
                .schedule(any(Runnable.class), eq(TIMEOUT.toMillis()), eq(TimeUnit.MILLISECONDS));
    }

    @Test
    public void testSchedulesCheckIfTimeoutNotReached() {
        // last modification was 3 seconds ago (less than timeout)
        long first = System.currentTimeMillis() - 20_000L; // will be ignored
        long second = System.currentTimeMillis() - 3_000L;
        FileIdleMonitor monitor = createMonitor(first, second);
        monitor.start();

        // callback not executed, and new idle check scheduled after approx. TIMEOUT - 3s
        long maxDelay = TIMEOUT.minusSeconds(3L).toMillis();
        verifyZeroInteractions(mCallback);
        verify(mExecutor, times(1))
                .schedule(
                        any(Runnable.class),
                        longThat(delay -> delay <= maxDelay),
                        eq(TimeUnit.MILLISECONDS));
    }

    @Test
    public void testExecutesCallbackIfTimeoutExceeded() {
        // last modification was 20 seconds ago (more than timeout)
        FileIdleMonitor monitor = createMonitor(System.currentTimeMillis() - 20_000L);
        monitor.start();

        // callback executed, and new idle check scheduled after TIMEOUT
        verify(mCallback, times(1)).run();
        verify(mExecutor, times(1))
                .schedule(any(Runnable.class), eq(TIMEOUT.toMillis()), eq(TimeUnit.MILLISECONDS));
    }

    // Helpers

    /**
     * Creates a mock file which was last modified at the specified time.
     *
     * @param timestamp mock modification timestamp
     * @return mock file
     */
    private static File mockFile(long timestamp) {
        File file = mock(File.class);
        when(file.lastModified()).thenReturn(timestamp);
        return file;
    }

    /**
     * Creates a file monitor which monitors mock files with the specified modification times.
     *
     * @param timestamps modification timestamps of the mock files
     * @return file monitor monitoring mock files
     */
    private FileIdleMonitor createMonitor(long... timestamps) {
        File[] files =
                Arrays.stream(timestamps)
                        .mapToObj(FileIdleMonitorTest::mockFile)
                        .toArray(File[]::new);
        return new FileIdleMonitor(() -> mExecutor, TIMEOUT, mCallback, files);
    }
}
