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
package com.android.tradefed.invoker;

import static org.mockito.Mockito.verify;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IReportNotExecuted;
import com.android.tradefed.testtype.StubTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link UnexecutedTestReporterThread}. */
@RunWith(JUnit4.class)
public class UnexecutedTestReporterThreadTest {

    private UnexecutedTestReporterThread mReporterThread;
    private ITestInvocationListener mMockListener;
    private List<IRemoteTest> mTests;

    @Before
    public void setUp() {
        mTests = new ArrayList<>();
        mMockListener = Mockito.mock(ITestInvocationListener.class);
        mReporterThread = new UnexecutedTestReporterThread(mMockListener, mTests);
    }

    @Test
    public void testNoTests() {
        mReporterThread.run();

        verify(mMockListener, Mockito.times(0)).testRunStarted(Mockito.any(), Mockito.anyInt());
    }

    @Test
    public void testOneTests() {
        IRemoteTest test = new TestReport();
        mTests.add(test);
        mReporterThread.run();

        verify(mMockListener, Mockito.times(1)).testRunStarted(Mockito.any(), Mockito.anyInt());
    }

    /** Tests that do not implement IReportNotExecuted are not reported. */
    @Test
    public void testSeveralTests_oneReporter() {
        IRemoteTest test = new TestReport();
        mTests.add(test);
        mTests.add(new StubTest());
        mReporterThread.run();

        verify(mMockListener, Mockito.times(1)).testRunStarted(Mockito.any(), Mockito.anyInt());
    }

    class TestReport implements IRemoteTest, IReportNotExecuted {
        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {}

        @Override
        public void reportNotExecuted(ITestInvocationListener listener) {
            listener.testRunStarted("test", 0);
        }
    }
}
