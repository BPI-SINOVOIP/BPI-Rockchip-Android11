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

package com.android.permissioncontroller.permission.service;

import android.app.Service;
import android.content.Intent;
import android.os.Handler;

import androidx.annotation.NonNull;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;

/**
 * A copy of the AndroidX ServiceLifecycleDispatcher.
 */
public class ServiceLifecycleDispatcher {
    private final LifecycleRegistry mRegistry;
    private final Handler mHandler;
    private DispatchRunnable mLastDispatchRunnable;

    /**
     * @param provider {@link LifecycleOwner} for a service, usually it is a service itself
     */
    public ServiceLifecycleDispatcher(@NonNull LifecycleOwner provider) {
        mRegistry = new CheckLifecycleRegistry(provider);
        mHandler = new Handler();
    }

    private void postDispatchRunnable(Lifecycle.Event event) {
        if (mLastDispatchRunnable != null) {
            mLastDispatchRunnable.run();
        }
        mLastDispatchRunnable = new DispatchRunnable(mRegistry, event);
        mHandler.postAtFrontOfQueue(mLastDispatchRunnable);
    }

    /**
     * Must be the first call in {@link Service#onCreate()} method, even before super.onCreate call.
     */
    public void onServicePreSuperOnCreate() {
        postDispatchRunnable(Lifecycle.Event.ON_CREATE);
    }

    /**
     * Must be the first call in {@link Service#onBind(Intent)} method, even before super.onBind
     * call.
     */
    public void onServicePreSuperOnBind() {
        postDispatchRunnable(Lifecycle.Event.ON_START);
        postDispatchRunnable(Lifecycle.Event.ON_RESUME);
    }

    /**
     * Must be a first call in {@link Service#onStart(Intent, int)} or
     * {@link Service#onStartCommand(Intent, int, int)} methods, even before
     * a corresponding super call.
     */
    public void onServicePreSuperOnStart() {
        postDispatchRunnable(Lifecycle.Event.ON_START);
    }

    /**
     * Must be the first call in {@link Service#onDestroy()} method, even before super.OnDestroy
     * call.
     */
    public void onServicePreSuperOnUnbind() {
        postDispatchRunnable(Lifecycle.Event.ON_STOP);
    }

    /**
     * Must be the first call in {@link Service#onDestroy()} method, even before super.OnDestroy
     * call.
     */
    public void onServicePreSuperOnDestroy() {
        postDispatchRunnable(Lifecycle.Event.ON_STOP);
        postDispatchRunnable(Lifecycle.Event.ON_DESTROY);
    }

    /**
     * @return {@link Lifecycle} for the given {@link LifecycleOwner}
     */
    @NonNull
    public Lifecycle getLifecycle() {
        return mRegistry;
    }

    static class DispatchRunnable implements Runnable {
        private final LifecycleRegistry mRegistry;
        final Lifecycle.Event mEvent;
        private boolean mWasExecuted = false;

        DispatchRunnable(@NonNull LifecycleRegistry registry, Lifecycle.Event event) {
            mRegistry = registry;
            mEvent = event;
        }

        @Override
        public void run() {
            if (!mWasExecuted) {
                mRegistry.handleLifecycleEvent(mEvent);
                mWasExecuted = true;
            }
        }
    }
}
