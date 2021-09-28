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
package com.android.tradefed.invoker.logger;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import com.google.common.collect.ImmutableSet;
import com.google.common.util.concurrent.Futures;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link InvocationLocal}. */
@RunWith(JUnit4.class)
public final class InvocationLocalTest {

    private final AtomicInteger nextInvocationId = new AtomicInteger();

    @Test
    public void getInitialValueDefaultsToNull() {
        InvocationLocal<Integer> local = new InvocationLocal<>();

        Object actual = invocation(() -> local.get());

        assertThat(actual).isNull();
    }

    @Test
    public void getReturnsCustomInitialValue() {
        String expected = "!";
        InvocationLocal<String> local =
                new InvocationLocal<String>() {
                    @Override
                    protected String initialValue() {
                        return expected;
                    }
                };

        String actual = invocation(() -> local.get());

        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void getReturnsSameInitialValue() {
        InvocationLocal<Object> local =
                new InvocationLocal<Object>() {
                    @Override
                    protected Object initialValue() {
                        return new Object();
                    }
                };

        ImmutableSet<Object> values = invocation(() -> ImmutableSet.of(local.get(), local.get()));

        assertThat(values).hasSize(1);
    }

    @Test
    public void getReturnsDifferentValuePerInvocation() throws Exception {
        InvocationLocal<Object> local =
                new InvocationLocal<Object>() {
                    @Override
                    protected Object initialValue() {
                        return new Object();
                    }
                };

        Object value0 = invocation(() -> local.get());
        Object value1 = invocation(() -> local.get());

        assertThat(value0).isNotSameAs(value1);
    }

    /**
     * Runs code in the context of a ThreadGroup to simulate a Tradefed invocation and avoid
     * interfering with the currently executing test invocation.
     */
    private <T> T invocation(Callable<T> callable) {
        // TradeFed Invocations are identified by their ThreadGroup.
        String name = "invocation" + nextInvocationId.getAndIncrement();
        ExecutorService executor =
                Executors.newSingleThreadExecutor(r -> new Thread(new ThreadGroup(name), r));
        Future<T> future =
                executor.submit(
                        () -> {
                            try {
                                return callable.call();
                            } finally {
                                CurrentInvocation.clearInvocationInfos();
                            }
                        });
        executor.shutdown();
        return Futures.getUnchecked(future);
    }
}
