/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.platform.test.rule;

import androidx.annotation.VisibleForTesting;

import com.android.helpers.GarbageCollectionHelper;

import org.junit.runner.Description;
import org.junit.runners.model.InitializationError;

/**
 * This rule will gc the provided apps before running each test method.
 */
public class GarbageCollectRule extends TestWatcher {
    private final GarbageCollectionHelper mGcHelper;

    public GarbageCollectRule() throws InitializationError {
        throw new InitializationError("Must supply an application for garbage collection.");
    }

    public GarbageCollectRule(String... applications) {
        mGcHelper = initGcHelper();
        mGcHelper.setUp(applications);
    }

    @VisibleForTesting
    GarbageCollectionHelper initGcHelper() {
        return new GarbageCollectionHelper();
    }

    @Override
    protected void starting(Description description) {
        mGcHelper.garbageCollect();
    }
}
