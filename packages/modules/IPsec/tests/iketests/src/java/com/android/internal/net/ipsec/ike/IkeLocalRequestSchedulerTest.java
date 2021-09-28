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

package com.android.internal.net.ipsec.ike;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;

import androidx.test.InstrumentationRegistry;

import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.IProcedureConsumer;
import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.LocalRequest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;

public final class IkeLocalRequestSchedulerTest {
    private IkeLocalRequestScheduler mScheduler;

    private IProcedureConsumer mMockConsumer;
    private LocalRequest[] mMockRequestArray;

    private ArgumentCaptor<LocalRequest> mLocalRequestCaptor =
            ArgumentCaptor.forClass(LocalRequest.class);

    private Context mSpyContext;
    private PowerManager mMockPowerManager;
    protected PowerManager.WakeLock mMockWakelock;

    @Before
    public void setUp() {
        mMockConsumer = mock(IProcedureConsumer.class);

        mSpyContext = spy(InstrumentationRegistry.getContext());
        mMockPowerManager = mock(PowerManager.class);
        mMockWakelock = mock(WakeLock.class);

        doReturn(mMockPowerManager).when(mSpyContext).getSystemService(eq(PowerManager.class));
        doReturn(mMockWakelock).when(mMockPowerManager).newWakeLock(anyInt(), anyString());

        mScheduler = new IkeLocalRequestScheduler(mMockConsumer, mSpyContext);

        mMockRequestArray = new LocalRequest[10];
        for (int i = 0; i < mMockRequestArray.length; i++) {
            mMockRequestArray[i] = mock(LocalRequest.class);
        }
    }

    @Test
    public void testAddMultipleRequestProcessOnlyOne() {
        for (LocalRequest r : mMockRequestArray) mScheduler.addRequest(r);

        // Verify that no procedure was preemptively pulled from the queue
        verify(mMockConsumer, never()).onNewProcedureReady(any());

        // Check that the onNewPrcedureReady called exactly once, on the first item
        mScheduler.readyForNextProcedure();
        verify(mMockConsumer, times(1)).onNewProcedureReady(any());
        verify(mMockConsumer, times(1)).onNewProcedureReady(mMockRequestArray[0]);
        for (int i = 1; i < mMockRequestArray.length; i++) {
            verify(mMockConsumer, never()).onNewProcedureReady(mMockRequestArray[i]);
        }
    }

    @Test
    public void testProcessOrder() {
        InOrder inOrder = inOrder(mMockConsumer);

        for (LocalRequest r : mMockRequestArray) mScheduler.addRequest(r);
        for (int i = 0; i < mMockRequestArray.length; i++) mScheduler.readyForNextProcedure();

        for (LocalRequest r : mMockRequestArray) {
            inOrder.verify(mMockConsumer).onNewProcedureReady(r);
        }
    }

    @Test
    public void testAddRequestToFrontProcessOrder() {
        InOrder inOrder = inOrder(mMockConsumer);

        LocalRequest[] mockHighPriorityRequestArray = new LocalRequest[10];
        for (int i = 0; i < mockHighPriorityRequestArray.length; i++) {
            mockHighPriorityRequestArray[i] = mock(LocalRequest.class);
        }

        for (LocalRequest r : mMockRequestArray) mScheduler.addRequest(r);
        for (LocalRequest r : mockHighPriorityRequestArray) mScheduler.addRequestAtFront(r);

        for (int i = 0; i < mockHighPriorityRequestArray.length + mMockRequestArray.length; i++) {
            mScheduler.readyForNextProcedure();
        }

        // Verify processing order. mockHighPriorityRequestArray is processed in reverse order
        for (int i = mockHighPriorityRequestArray.length - 1; i >= 0; i--) {
            inOrder.verify(mMockConsumer).onNewProcedureReady(mockHighPriorityRequestArray[i]);
        }
        for (LocalRequest r : mMockRequestArray) {
            inOrder.verify(mMockConsumer).onNewProcedureReady(r);
        }
    }
}
