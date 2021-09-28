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
package com.android.tradefed.dependency;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.invoker.IInvocationContext;

import java.io.File;

/** Helper to resolve dependencies if needed. */
public class TestDependencyResolver {

    /**
     * Resolve a single dependency based on some context.
     *
     * @param dependency The dependency to be resolved.
     * @param build The current build information being created.
     * @param context The invocation context.
     * @return The resolved dependency or null if unresolved.
     * @throws BuildRetrievalError
     */
    public static File resolveDependencyFromContext(
            File dependency, IBuildInfo build, IInvocationContext context)
            throws BuildRetrievalError {
        if (dependency.exists()) {
            return dependency;
        }
        // TODO: Implement the core logic
        return null;
    }
}
