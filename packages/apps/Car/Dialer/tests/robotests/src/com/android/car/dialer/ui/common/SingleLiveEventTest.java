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

package com.android.car.dialer.ui.common;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;

import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.LiveDataObserver;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(CarDialerRobolectricTestRunner.class)
public class SingleLiveEventTest {
    private static final String VALUE = "setValue";

    private SingleLiveEvent<String> mSingleLiveEvent;

    private LifecycleRegistry mLifecycleRegistry;
    @Mock
    private LifecycleOwner mMockLifecycleOwner;
    @Mock
    private LiveDataObserver<SingleLiveEvent> mMockObserver1;
    @Mock
    private LiveDataObserver<SingleLiveEvent> mMockObserver2;
    @Captor
    private ArgumentCaptor<String> mCaptor;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mSingleLiveEvent = new SingleLiveEvent();
        mLifecycleRegistry = new LifecycleRegistry(mMockLifecycleOwner);
        when(mMockLifecycleOwner.getLifecycle()).thenReturn(mLifecycleRegistry);
    }

    @Test
    public void test_onChangeCalledAfterSetValue() {
        mSingleLiveEvent.observe(mMockLifecycleOwner, mMockObserver1);
        verify(mMockObserver1, never()).onChanged(any());

        mLifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START);
        verify(mMockObserver1, never()).onChanged(any());

        mSingleLiveEvent.setValue(VALUE);
        verify(mMockObserver1, times(1)).onChanged(mCaptor.capture());
        assertThat(mCaptor.getValue()).isEqualTo(VALUE);
    }

    @Test
    public void test_onChangeOnlyHandledOnce() {
        mLifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START);

        mSingleLiveEvent.observe(mMockLifecycleOwner, mMockObserver1);
        mSingleLiveEvent.observe(mMockLifecycleOwner, mMockObserver2);

        mSingleLiveEvent.setValue(VALUE);
        verify(mMockObserver1, times(1)).onChanged(mCaptor.capture());
        assertThat(mCaptor.getValue()).isEqualTo(VALUE);
        verify(mMockObserver2, never()).onChanged(any());
    }
}
