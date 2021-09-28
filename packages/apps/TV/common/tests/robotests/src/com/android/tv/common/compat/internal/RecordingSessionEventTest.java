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
 * limitations under the License
 */
package com.android.tv.common.compat.internal;

import static org.mockito.Mockito.only;
import static org.mockito.Mockito.verify;

import com.android.tv.common.compat.api.RecordingClientCallbackCompatEvents;
import com.android.tv.testing.constants.ConfigConstants;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/**
 * Tests sending {@link RecordingEvents.RecordingSessionEvent}s to a {@link
 * RecordingClientCallbackCompatEvents} from {@link RecordingSessionCompatProcessor} via {@link
 * RecordingClientCompatProcessor}.
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class RecordingSessionEventTest {
    @Mock RecordingClientCallbackCompatEvents mCallback;

    private RecordingSessionCompatProcessor mCompatProcess;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        RecordingClientCompatProcessor compatProcessor =
                new RecordingClientCompatProcessor(null, mCallback);
        mCompatProcess =
                new RecordingSessionCompatProcessor(
                        (event, data) -> compatProcessor.handleEvent("testinput", event, data),
                        null);
    }

    @Test
    public void notifyDevToast() throws InvalidProtocolBufferException {
        mCompatProcess.notifyDevToast("Recording");
        verify(mCallback, only()).onDevToast("testinput", "Recording");
    }

    @Test
    public void notifyRecordingStarted() throws InvalidProtocolBufferException {
        mCompatProcess.notifyRecordingStarted("file:example");
        verify(mCallback, only()).onRecordingStarted("testinput", "file:example");
    }
}
