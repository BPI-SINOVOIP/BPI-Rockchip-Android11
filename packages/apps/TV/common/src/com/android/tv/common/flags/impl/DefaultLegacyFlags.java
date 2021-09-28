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
package com.android.tv.common.flags.impl;

import com.google.auto.value.AutoValue;
import com.android.tv.common.flags.LegacyFlags;

/** Default {@link LegacyFlags}. */
@AutoValue
public abstract class DefaultLegacyFlags implements LegacyFlags {
    public static final DefaultLegacyFlags DEFAULT = DefaultLegacyFlags.builder().build();

    public static Builder builder() {
        return new AutoValue_DefaultLegacyFlags.Builder()
                .compiled(true)
                .enableDeveloperFeatures(false)
                .enableQaFeatures(false)
                .enableUnratedContentSettings(false);
    }

    /** Builder for {@link LegacyFlags} */
    @AutoValue.Builder
    public abstract static class Builder {
        public abstract Builder compiled(boolean value);

        public abstract Builder enableDeveloperFeatures(boolean value);

        public abstract Builder enableQaFeatures(boolean value);

        public abstract Builder enableUnratedContentSettings(boolean value);

        public abstract DefaultLegacyFlags build();
    }
}
