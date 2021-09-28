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

package android.car.media;

import android.car.media.ICarMediaSourceListener;
import android.content.ComponentName;

/**
 * Binder interface for {@link android.car.media.CarMediaManager}.
 * Check {@link android.car.media.CarMediaManager} APIs for expected behavior of each call.
 *
 * @hide
 */
interface ICarMedia {
    /** Gets the currently active media source for the provided mode */
    ComponentName getMediaSource(int mode);
    /** Sets the currently active media source for the provided mode */
    void setMediaSource(in ComponentName mediaSource, int mode);
    /** Register a callback that receives updates to the active media source */
    void registerMediaSourceListener(in ICarMediaSourceListener callback, int mode);
    /** Unregister a callback that receives updates to the active media source */
    void unregisterMediaSourceListener(in ICarMediaSourceListener callback, int mode);
    /** Retrieve a list of media sources, ordered by most recently used */
    List<ComponentName> getLastMediaSources(int mode);
    /** Returns whether the browse and playback sources can be changed independently. */
    boolean isIndependentPlaybackConfig();
    /** Sets whether the browse and playback sources can be changed independently. */
    void setIndependentPlaybackConfig(boolean independent);
}
