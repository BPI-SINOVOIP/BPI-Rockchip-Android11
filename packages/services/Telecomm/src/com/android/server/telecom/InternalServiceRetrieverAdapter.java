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

package com.android.server.telecom;

import android.os.RemoteException;

import com.android.internal.telecom.IInternalServiceRetriever;

/**
 * Contains all services that Telecom must access that are only accessible as a local service as
 * part of the SYSTEM process.
 */
public class InternalServiceRetrieverAdapter {

    private final IInternalServiceRetriever mRetriever;

    public InternalServiceRetrieverAdapter(IInternalServiceRetriever retriever) {
        mRetriever = retriever;
    }

    public DeviceIdleControllerAdapter getDeviceIdleController() {
        try {
            return new DeviceIdleControllerAdapter(mRetriever.getDeviceIdleController());
        } catch (RemoteException e) {
            // This should not happen - if it does, there is a bad configuration as this should
            // all be local.
            throw new IllegalStateException(e);
        }
    }
}
