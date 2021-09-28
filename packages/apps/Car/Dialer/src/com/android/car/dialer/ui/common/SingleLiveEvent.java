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

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.MediatorLiveData;
import androidx.lifecycle.Observer;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A mediator live data implementation that propagates value ONLY ONCE.
 */
public class SingleLiveEvent<T> extends MediatorLiveData<T> {

    private final AtomicBoolean mPendingUpdate = new AtomicBoolean(false);

    @Override
    @MainThread
    public void observe(@NonNull LifecycleOwner owner, @NonNull Observer<? super T> observer) {
        super.observe(owner, value -> {
            if (mPendingUpdate.compareAndSet(true, false)) {
                observer.onChanged(value);
            }
        });
    }

    @Override
    @MainThread
    public void setValue(@Nullable T value) {
        if (value != null) {
            mPendingUpdate.set(true);
            super.setValue(value);
        }
    }
}
