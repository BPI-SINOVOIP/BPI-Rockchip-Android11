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

import com.android.tv.common.compat.api.SessionCompatCommands;
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
 * Tests sending {@link Commands.PrivateCommand}s to a {@link SessionCompatCommands} from {@link
 * TvViewCompatProcessor} via {@link TifSessionCompatProcessor}.
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class PrivateCommandTest {
    @Mock SessionCompatCommands mCallback;

    private TvViewCompatProcessor mTvViewCompatProcessor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TifSessionCompatProcessor sessionCompatProcessor =
                new TifSessionCompatProcessor(null, mCallback);
        mTvViewCompatProcessor =
                new TvViewCompatProcessor(sessionCompatProcessor::handleAppPrivateCommand);
    }

    @Test
    public void notifyDevToast() throws InvalidProtocolBufferException {
        mTvViewCompatProcessor.devMessage("Hello Developers");
        verify(mCallback, only()).onDevMessage("Hello Developers");
    }
}
