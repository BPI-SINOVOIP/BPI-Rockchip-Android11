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

package com.android.internal.net.utils;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.android.internal.net.utils.SimpleStateMachine.SimpleState;

import org.junit.Before;
import org.junit.Test;

public class SimpleStateMachineTest {
    private static final String INPUT = "input";
    private static final String OUTPUT = "output";

    private SimpleStateMachine<String, String> mSimpleStateMachine;
    private SimpleState mMockStartState;
    private SimpleState mMockFinalState;

    @Before
    public void setUp() {
        mMockStartState = mock(SimpleState.class);
        mMockFinalState = mock(SimpleState.class);

        mSimpleStateMachine = new SimpleStateMachine(){};
        mSimpleStateMachine.transitionTo(mMockStartState);
    }

    @Test
    public void testProcess() {
        doReturn(OUTPUT).when(mMockStartState).process(INPUT);
        String result = mSimpleStateMachine.process(INPUT);
        assertEquals(OUTPUT, result);
        verify(mMockStartState).process(INPUT);
    }

    @Test
    public void testTransitionTo() {
        mSimpleStateMachine.transitionTo(mMockFinalState);
        assertEquals(mMockFinalState, mSimpleStateMachine.mState);
    }

    @Test
    public void testTransitionToNull() {
        try {
            mSimpleStateMachine.transitionTo(null);
            fail("IllegalArgumentException expected");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testTransitionAndProcess() {
        doReturn(OUTPUT).when(mMockFinalState).process(INPUT);
        String result = mSimpleStateMachine.transitionAndProcess(mMockFinalState, INPUT);
        assertEquals(OUTPUT, result);
        verify(mMockFinalState).process(INPUT);
    }

    @Test
    public void testTransitionAndProcessToNull() {
        try {
            mSimpleStateMachine.transitionAndProcess(null, INPUT);
            fail("IllegalArgumentException expected");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testProcessNullState() {
        SimpleStateMachine simpleStateMachine = new SimpleStateMachine() {};
        try {
            simpleStateMachine.process(new Object());
            fail("IllegalStateException expected");
        } catch (IllegalStateException expected) {
        }
    }
}
