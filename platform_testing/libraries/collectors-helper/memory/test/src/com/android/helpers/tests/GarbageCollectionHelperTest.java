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
package com.android.helpers.tests;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.support.test.uiautomator.UiDevice;

import com.android.helpers.GarbageCollectionHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link GarbageCollectionHelper}.
 */
@RunWith(JUnit4.class)
public final class GarbageCollectionHelperTest {
    // Most tests don't actually need the memory to stabilize so no point in waiting.
    private static final int TEST_POST_GC_WAIT_TIME_MS = 0;

    private @Mock UiDevice mUiDevice;

    private GarbageCollectionHelper mHelper;
    private int mPidCounter = 0;

    @Before
    public void setUp() throws Throwable {
        MockitoAnnotations.initMocks(this);
        // Return a fake pid for our fake processes and an empty string otherwise.
        doAnswer((inv) -> {
            String cmd = (String) inv.getArguments()[0];
            if (cmd.startsWith("pidof package")) {
                mPidCounter++;
                return String.valueOf(mPidCounter);
            } else {
                return "";
            }
        }).when(mUiDevice).executeShellCommand(any());
        mHelper = new TestableGarbageCollectionHelper();
    }

    /**
     * Tests that nothing happens when the helper has not been properly set up.
     */
    @Test
    public void testNoSetUp() throws Throwable {
        mHelper.garbageCollect(TEST_POST_GC_WAIT_TIME_MS);

        verifyZeroInteractions(mUiDevice);
    }

    /**
     * Tests that this rule will gc one app, if supplied.
     */
    @Test
    public void testOneAppToGc() throws Throwable {
        mHelper.setUp("package.name1");
        mHelper.garbageCollect(TEST_POST_GC_WAIT_TIME_MS);

        InOrder inOrder = inOrder(mUiDevice);
        inOrder.verify(mUiDevice).executeShellCommand("pidof package.name1");
        inOrder.verify(mUiDevice).executeShellCommand("kill -10 1");
    }

    /**
     * Tests that this rule will gc multiple apps before the test, if supplied.
     */
    @Test
    public void testMultipleAppsToGc() throws Throwable {
        mHelper.setUp("package.name1", "package.name2", "package.name3");
        mHelper.garbageCollect(TEST_POST_GC_WAIT_TIME_MS);

        InOrder inOrder = inOrder(mUiDevice);
        inOrder.verify(mUiDevice).executeShellCommand("pidof package.name1");
        inOrder.verify(mUiDevice).executeShellCommand("kill -10 1");
        inOrder.verify(mUiDevice).executeShellCommand("pidof package.name2");
        inOrder.verify(mUiDevice).executeShellCommand("kill -10 2");
        inOrder.verify(mUiDevice).executeShellCommand("pidof package.name3");
        inOrder.verify(mUiDevice).executeShellCommand("kill -10 3");
    }

    /**
     * Tests that this rule will skip unavailable apps, if supplied.
     */
    @Test
    public void testSkipsGcOnDneApp() throws Throwable {
        mHelper.setUp("does.not.exist", "package.name1");
        mHelper.garbageCollect(TEST_POST_GC_WAIT_TIME_MS);

        InOrder inOrder = inOrder(mUiDevice);
        inOrder.verify(mUiDevice).executeShellCommand("pidof does.not.exist");
        inOrder.verify(mUiDevice).executeShellCommand("pidof package.name1");
        inOrder.verify(mUiDevice).executeShellCommand("kill -10 1");
    }

    private final class TestableGarbageCollectionHelper extends GarbageCollectionHelper {
        @Override
        protected UiDevice initUiDevice() {
            return mUiDevice;
        }
    }
}
