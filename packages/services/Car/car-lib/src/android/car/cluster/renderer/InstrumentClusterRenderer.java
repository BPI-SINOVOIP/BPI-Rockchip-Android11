/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.car.cluster.renderer;

import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.annotation.UiThread;
import android.content.Context;

import com.android.internal.annotations.GuardedBy;

/**
 * @deprecated This class is unused. Refer to {@link InstrumentClusterRenderingService} for
 * documentation on how to build a instrument cluster renderer.
 *
 * @hide
 */
@Deprecated
@SystemApi
public abstract class InstrumentClusterRenderer {

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    @Nullable private NavigationRenderer mNavigationRenderer;

    /**
     * Called when instrument cluster renderer is created.
     */
    public abstract void onCreate(Context context);

    /**
     * Called when instrument cluster renderer is started.
     */
    public abstract void onStart();

    /**
     * Called when instrument cluster renderer is stopped.
     */
    public abstract void onStop();

    protected abstract NavigationRenderer createNavigationRenderer();

    /** The method is thread-safe, callers should cache returned object. */
    @Nullable
    public NavigationRenderer getNavigationRenderer() {
        synchronized (mLock) {
            return mNavigationRenderer;
        }
    }

    /**
     * This method is called by car service after onCreateView to initialize private members. The
     * method should not be overridden by subclasses.
     */
    @UiThread
    public final void initialize() {
        synchronized (mLock) {
            mNavigationRenderer = createNavigationRenderer();
        }
    }
}

