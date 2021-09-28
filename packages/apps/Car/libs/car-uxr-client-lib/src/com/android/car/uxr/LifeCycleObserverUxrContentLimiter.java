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

package com.android.car.uxr;

import androidx.annotation.NonNull;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import com.android.car.ui.recyclerview.ContentLimiting;

/**
 * An implementation of {@link UxrContentLimiter} interface that also provides the functionality
 * necessary for a {@link DefaultLifecycleObserver}.
 *
 * <p>Relies heavily on the {@link UxrContentLimiterImpl} implementation of the
 * {@link UxrContentLimiter} interface.
 *
 * <p>For example, you could do the following to get yourself a lifecycle aware {@link
 * UxrContentLimiter}:
 * <pre>{@code
 * new LifeCycleObserverUxrContentLimiter(new UxrContentLimiterImpl(context,xmlRes));
 * }</pre>
 */
public class LifeCycleObserverUxrContentLimiter
        implements UxrContentLimiter, DefaultLifecycleObserver {

    private final UxrContentLimiterImpl mDelegate;

    public LifeCycleObserverUxrContentLimiter(UxrContentLimiterImpl delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onStart(@NonNull LifecycleOwner owner) {
        mDelegate.start();
    }

    @Override
    public void onStop(@NonNull LifecycleOwner owner) {
        mDelegate.stop();
    }

    @Override
    public void setAdapter(ContentLimiting adapter) {
        mDelegate.setAdapter(adapter);
    }
}
