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
package android.car;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import android.annotation.NonNull;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.util.Log;

import com.android.car.CarLocalServices;

/**
 * Specialized {@link AbstractExtendedMockitoTestCase} implementation that provides
 * CarService-related helpers.
 */
public abstract class AbstractExtendedMockitoCarServiceTestCase
        extends AbstractExtendedMockitoTestCase {

    private static final String TAG = AbstractExtendedMockitoCarServiceTestCase.class
            .getSimpleName();

    private static final boolean VERBOSE = false;

    /**
     * Mocks a call to {@link CarLocalServices#getService(Class)}.
     *
     * @throws IllegalStateException if class didn't override {@link #newSessionBuilder()} and
     * called {@code spyStatic(CarLocalServices.class)} on the session passed to it.
     */
    protected final <T> void mockGetCarLocalService(@NonNull Class<T> type, @NonNull T service) {
        if (VERBOSE) Log.v(TAG, getLogPrefix() + "mockGetLocalService(" + type.getName() + ")");
        assertSpied(CarLocalServices.class);

        beginTrace("mockGetLocalService");
        doReturn(service).when(() -> CarLocalServices.getService(type));
        endTrace();
    }
}
