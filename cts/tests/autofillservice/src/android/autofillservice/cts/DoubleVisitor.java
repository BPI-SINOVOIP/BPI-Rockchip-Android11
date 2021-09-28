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
package android.autofillservice.cts;

import androidx.annotation.NonNull;

/**
 * Implements the Visitor design pattern to visit 2 related objects (like a view and the activity
 * hosting it).
 *
 * @param <V1> 1st visited object
 * @param <V2> 2nd visited object
 */
// TODO: move to common
public interface DoubleVisitor<V1, V2> {

    /**
     * Visit those objects.
     */
    void visit(@NonNull V1 visited1, @NonNull V2 visited2);
}
