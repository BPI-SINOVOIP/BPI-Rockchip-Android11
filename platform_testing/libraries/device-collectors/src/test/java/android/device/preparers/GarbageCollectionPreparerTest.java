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

package android.device.preparers;

import static android.device.preparers.GarbageCollectionPreparer.GC_RUN_END;
import static android.device.preparers.GarbageCollectionPreparer.GC_WAIT_TIME_KEY;
import static android.device.preparers.GarbageCollectionPreparer.PROCESS_NAMES_KEY;
import static android.device.preparers.GarbageCollectionPreparer.PROCESS_SEPARATOR;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.app.Instrumentation;
import android.os.Bundle;

import com.android.helpers.GarbageCollectionHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Android Unit tests for {@link GarbageCollectionPreparer}.
 *
 * To run:
 * atest CollectorDeviceLibTest:android.device.preparers.GarbageCollectionPreparerTest
 */
public class GarbageCollectionPreparerTest {

    private static final String TEST_PROCESS_NAME_1 = "package.name1";
    private static final String TEST_PROCESS_NAME_2 = "package.name2";
    private static final long TEST_WAIT_TIME = 1234;
    private static final long INVALID_TEST_WAIT_TIME = -1234;

    @Mock
    private Instrumentation mInstrumentation;
    @Mock
    private GarbageCollectionHelper mGcHelper;

    private GarbageCollectionPreparer mPreparer;
    private Description mRunDesc;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
    }

    private GarbageCollectionPreparer initPreparer(Bundle b) {
        GarbageCollectionPreparer preparer = new GarbageCollectionPreparer(b, mGcHelper);
        preparer.setInstrumentation(mInstrumentation);
        return preparer;
    }

    @Test
    public void testHelperReceivesNoProcesses() throws Exception {
        Bundle b = new Bundle();
        mPreparer = initPreparer(b);

        mPreparer.testRunStarted(mRunDesc);
        mPreparer.testStarted(mRunDesc);
        mPreparer.testFinished(mRunDesc);

        verifyZeroInteractions(mGcHelper);
    }

    @Test
    public void testHelperReceivesOneProcess() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, TEST_PROCESS_NAME_1);
        mPreparer = initPreparer(b);

        mPreparer.testRunStarted(mRunDesc);

        verify(mGcHelper).setUp(TEST_PROCESS_NAME_1);
    }

    @Test
    public void testHelperReceivesMultipleProcesses() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, String.format("%s%s%s",
                TEST_PROCESS_NAME_1, PROCESS_SEPARATOR, TEST_PROCESS_NAME_2));
        mPreparer = initPreparer(b);

        mPreparer.testRunStarted(mRunDesc);

        verify(mGcHelper).setUp(TEST_PROCESS_NAME_1, TEST_PROCESS_NAME_2);
    }

    @Test
    public void testHelperGcsDefaultWaitTimeWhenUnprovided() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, TEST_PROCESS_NAME_1);
        mPreparer = initPreparer(b);
        mPreparer.testRunStarted(mRunDesc);

        mPreparer.testStarted(mRunDesc);
        mPreparer.testFinished(mRunDesc);

        verify(mGcHelper, times(2)).garbageCollect();
    }

    @Test
    public void testHelperGcsDefaultWaitTimeWhenInvalid() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, TEST_PROCESS_NAME_1);
        b.putString(GC_WAIT_TIME_KEY, Long.toString(INVALID_TEST_WAIT_TIME));
        mPreparer = initPreparer(b);
        mPreparer.testRunStarted(mRunDesc);

        mPreparer.testStarted(mRunDesc);
        mPreparer.testFinished(mRunDesc);

        verify(mGcHelper, times(2)).garbageCollect();
    }

    @Test
    public void testHelperGcsProvidedWaitTime() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, TEST_PROCESS_NAME_1);
        b.putString(GC_WAIT_TIME_KEY, Long.toString(TEST_WAIT_TIME));
        mPreparer = initPreparer(b);
        mPreparer.testRunStarted(mRunDesc);

        mPreparer.testStarted(mRunDesc);
        mPreparer.testFinished(mRunDesc);

        verify(mGcHelper, times(2)).garbageCollect(TEST_WAIT_TIME);
    }

    @Test
    public void testHelperGcsNoRunEnd() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, TEST_PROCESS_NAME_1);
        b.putString(GC_RUN_END, "false");
        mPreparer = initPreparer(b);
        mPreparer.testRunStarted(mRunDesc);

        mPreparer.testStarted(mRunDesc);
        mPreparer.testFinished(mRunDesc);

        verify(mGcHelper, times(1)).garbageCollect();
    }

    @Test
    public void testHelperGcsRunEnd() throws Exception {
        Bundle b = new Bundle();
        b.putString(PROCESS_NAMES_KEY, TEST_PROCESS_NAME_1);
        b.putString(GC_RUN_END, "true");
        mPreparer = initPreparer(b);
        mPreparer.testRunStarted(mRunDesc);

        mPreparer.testStarted(mRunDesc);
        mPreparer.testFinished(mRunDesc);

        verify(mGcHelper, times(2)).garbageCollect();
    }
}
