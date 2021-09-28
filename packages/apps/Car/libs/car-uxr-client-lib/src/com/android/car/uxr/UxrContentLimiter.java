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

import com.android.car.ui.recyclerview.ContentLimiting;

/**
 * An interface to facilitate the content limiting ability of {@link ContentLimiting}
 * {@link androidx.recyclerview.widget.RecyclerView.Adapter} objects based on changes to the state
 * of the car.
 */
public interface UxrContentLimiter {

    /**
     * Registers the given {@link ContentLimiting} with this {@code UxrContentLimiter}.
     *
     * <p>That means that when the car state changes, if necessary, this
     * {@code UxrContentLimiter} will limit the content in the given adapter.
     *
     * @param adapter - the adapter to associate with this content limiter.
     */
    void setAdapter(ContentLimiting adapter);
}
