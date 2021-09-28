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
package com.android.tradefed.suite.checker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for {@link ShellStatusChecker}.
 *
 * <pre> Run with:
 * cd ${ANDROID_BUILD_TOP}/tools/tradefederation/core/tests
 * mm
 * run_tradefed_tests.sh --class com.android.tradefed.suite.checker.ShellStatusCheckerTest
 * </pre>
 */
@RunWith(JUnit4.class)
public class ShellStatusCheckerTest {

    private ShellStatusChecker mChecker;
    private ITestDevice mMockDevice;
    private static final String FAIL_STRING = "Reset failed";

    /* The expected root state at pre- and post-check. */
    private boolean mExpectRoot = false;

    @Before
    public void setUp() {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mChecker = new ShellStatusChecker();
    }

    /* Verify pre- and post-state easily. Shorthand to always revert ok. */
    private void expectPreAndPost(boolean preRoot, boolean postRoot) throws Exception {
        expectPreAndPost(preRoot, postRoot, true, true);
    }

    /* Verify revert-state easily. Shorthand to fail expectation and verify revert result. */
    private void expectRevertOk(boolean preRevertOk, boolean postRevertOk) throws Exception {
        expectPreAndPost(!mExpectRoot, !mExpectRoot, preRevertOk, postRevertOk);
    }

    /* Extended helper to verify pre- and post-state fully. */
    private void expectPreAndPost(
            boolean preRoot, boolean postRoot, boolean preRevertOk, boolean postRevertOk)
            throws Exception {
        CheckStatus preState = CheckStatus.SUCCESS;
        CheckStatus postState = CheckStatus.SUCCESS;

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(preRoot);
        if (preRoot != mExpectRoot) {
            preState = CheckStatus.FAILED;
            if (mExpectRoot) {
                EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(preRevertOk);
            } else {
                EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(preRevertOk);
            }
        }

        EasyMock.expect(mMockDevice.isAdbRoot()).andReturn(postRoot);
        if (postRoot != mExpectRoot) {
            postState = CheckStatus.FAILED;
            if (mExpectRoot) {
                EasyMock.expect(mMockDevice.enableAdbRoot()).andReturn(postRevertOk);
            } else {
                EasyMock.expect(mMockDevice.disableAdbRoot()).andReturn(postRevertOk);
            }
        }

        EasyMock.replay(mMockDevice);

        StatusCheckerResult res = mChecker.preExecutionCheck(mMockDevice);
        assertEquals("preExecutionCheck1", preState, res.getStatus());
        String msg = res.getErrorMessage();
        // Assume that preRevertOk flag is only false when the intention was to test it.
        if (preState != CheckStatus.SUCCESS || !preRevertOk) {
            assertNotNull("preExecutionCheck2", msg);
            assertEquals("preExecutionCheck3", !preRevertOk, msg.contains(FAIL_STRING));
        } else {
            assertNull("preExecutionCheck4", msg);
        }

        res = mChecker.postExecutionCheck(mMockDevice);
        assertEquals("postExecutionCheck1", postState, res.getStatus());
        msg = res.getErrorMessage();
        if (postState != CheckStatus.SUCCESS || !postRevertOk) {
            assertNotNull("postExecutionCheck2", msg);
            assertEquals("postExecutionCheck3", !postRevertOk, msg.contains(FAIL_STRING));
        } else {
            assertNull("postExecutionCheck4", msg);
        }

        EasyMock.verify(mMockDevice);
    }

    /** Test the system checker if the non-expected state does not change. */
    @Test
    public void testUnexpectedRemainUnchanged() throws Exception {
        setExpectedRoot(false);
        expectPreAndPost(false, false);
    }

    /** Test the system checker if the non-expected state changes. */
    @Test
    public void testUnexpectedChanged() throws Exception {
        setExpectedRoot(true);
        expectPreAndPost(false, true);
    }

    /** Test the system checker if the expected state does not change. */
    @Test
    public void testExpectedRemainUnchanged() throws Exception {
        setExpectedRoot(true);
        expectPreAndPost(true, true);
    }

    /** Test the system checker if the expected state changes. */
    @Test
    public void testExpectedChanged() throws Exception {
        setExpectedRoot(false);
        expectPreAndPost(true, false);
    }

    /** Test that error message warns for every revert failed. */
    @Test
    public void testRevertFails() throws Exception {
        setExpectedRoot(false);
        expectRevertOk(false, false);
    }

    /** Test that error message warns for failed reverts independently. */
    @Test
    public void testRevertFailIndependant() throws Exception {
        setExpectedRoot(true);
        expectRevertOk(true, false);
    }

    private void setExpectedRoot(boolean expectRoot) throws ConfigurationException {
        mExpectRoot = expectRoot;
        OptionSetter setter = new OptionSetter(mChecker);
        setter.setOptionValue("expect-root", Boolean.toString(expectRoot));
    }
}
