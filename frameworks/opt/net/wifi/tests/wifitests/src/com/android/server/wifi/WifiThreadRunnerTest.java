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

package com.android.server.wifi;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.os.Handler;
import android.os.HandlerThread;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import java.util.function.Supplier;

@SmallTest
public class WifiThreadRunnerTest {

    private static final int RESULT = 2;
    private static final int VALUE_ON_TIMEOUT = -1;

    private WifiThreadRunner mWifiThreadRunner;

    @Mock private Runnable mRunnable;

    private Handler mHandler;

    @Spy private Supplier<Integer> mSupplier = () -> RESULT;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        HandlerThread mHandlerThread = new HandlerThread("WifiThreadRunnerTestHandlerThread");
        mHandlerThread.start();

        // runWithScissors() is a final method that cannot be mocked, instead wrap it in a spy,
        // then it can be mocked
        mHandler = spy(mHandlerThread.getThreadHandler());

        mWifiThreadRunner = new WifiThreadRunner(mHandler);
    }

    @Test
    public void callSuccess_returnExpectedValue() {
        // doAnswer(<lambda>).when(<spy>).method(<args>) syntax is needed for spies instead of
        // when(<mock>.method(<args>)).thenAnswer(<lambda>) for mocks
        doAnswer(invocation -> {
            Object[] args = invocation.getArguments();
            Runnable runnable = (Runnable) args[0];
            runnable.run();
            return true;
        }).when(mHandler).runWithScissors(any(), anyLong());

        Integer result = mWifiThreadRunner.call(mSupplier, VALUE_ON_TIMEOUT);

        assertThat(result).isEqualTo(RESULT);
        verify(mSupplier).get();
    }

    @Test
    public void callFailure_returnValueOnTimeout() {
        doReturn(false).when(mHandler).post(any());

        Integer result = mWifiThreadRunner.call(mSupplier, VALUE_ON_TIMEOUT);

        assertThat(result).isEqualTo(VALUE_ON_TIMEOUT);
        verify(mSupplier, never()).get();
    }

    @Test
    public void runSuccess() {
        doAnswer(invocation -> {
            Object[] args = invocation.getArguments();
            Runnable runnable = (Runnable) args[0];
            runnable.run();
            return true;
        }).when(mHandler).runWithScissors(any(), anyLong());

        boolean result = mWifiThreadRunner.run(mRunnable);

        assertThat(result).isTrue();
        verify(mRunnable).run();
    }

    @Test
    public void runFailure() {
        doReturn(false).when(mHandler).post(any());

        boolean runSuccess = mWifiThreadRunner.run(mRunnable);

        assertThat(runSuccess).isFalse();
        verify(mRunnable, never()).run();
    }

    @Test
    public void postSuccess() {
        doReturn(true).when(mHandler).post(any());

        boolean postSuccess = mWifiThreadRunner.post(mRunnable);

        assertThat(postSuccess).isTrue();
        verify(mHandler).post(mRunnable);
        // assert that the runnable is not run on the calling thread
        verify(mRunnable, never()).run();
    }

    @Test
    public void postFailure() {
        doReturn(false).when(mHandler).post(any());

        boolean postSuccess = mWifiThreadRunner.post(mRunnable);

        assertThat(postSuccess).isFalse();
        verify(mHandler).post(mRunnable);
        verify(mRunnable, never()).run();
    }
}
