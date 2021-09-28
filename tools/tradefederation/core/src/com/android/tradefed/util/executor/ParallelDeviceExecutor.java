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
 * limitations under the License.
 */
package com.android.tradefed.util.executor;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Wrapper of {@link ExecutorService} to execute a function on all devices in parallel. */
public class ParallelDeviceExecutor<V> {

    private List<ITestDevice> mDevices;
    private List<Throwable> mErrors;

    public ParallelDeviceExecutor(List<ITestDevice> devices) {
        mDevices = devices;
        mErrors = new ArrayList<>();
    }

    /**
     * Invoke all the {@link Callable} with the timeout limit.
     *
     * @param callableTasks The List of tasks.
     * @param timeout The timeout to apply.
     * @param unit The unit of the timeout.
     * @return The list of results for each callable task.
     */
    public List<V> invokeAll(List<Callable<V>> callableTasks, long timeout, TimeUnit unit) {
        ExecutorService executor =
                Executors.newFixedThreadPool(
                        mDevices.size(),
                        new ThreadFactory() {
                            @Override
                            public Thread newThread(Runnable r) {
                                Thread t = Executors.defaultThreadFactory().newThread(r);
                                t.setDaemon(true);
                                return t;
                            }
                        });
        List<V> results = new ArrayList<>();
        try {
            List<Future<V>> futures = executor.invokeAll(callableTasks);
            for (Future<V> future : futures) {
                try {
                    results.add(future.get(timeout, unit));
                } catch (TimeoutException timeoutException) {
                    mErrors.add(timeoutException);
                } catch (ExecutionException execException) {
                    mErrors.add(execException.getCause());
                }
            }
        } catch (InterruptedException e) {
            CLog.e(e);
            mErrors.add(e);
        } finally {
            executor.shutdown();
        }
        return results;
    }

    /** Whether or not some errors occurred or not. */
    public boolean hasErrors() {
        return !mErrors.isEmpty();
    }

    /** The list of errors from the execution of all tasks. */
    public List<Throwable> getErrors() {
        return mErrors;
    }
}
