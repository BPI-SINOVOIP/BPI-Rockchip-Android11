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
package com.android.tradefed.invoker.logger;

import javax.annotation.Nullable;
import javax.annotation.concurrent.ThreadSafe;

/**
 * This class provides invocation-scope variables.
 *
 * <p>The mechanism operates similarly to {@link ThreadLocal}. These variables differ from their
 * normal counterparts in that code in an invocation that accesses one (via its {@code get} method)
 * has its own, independently initialized copy of the variable. {@code InvocationLocal} instances
 * are typically private static fields in classes that wish to associate state with an invocation.
 *
 * <p>Each invocation is associated with a copy of an invocation-scoped variable as long as the
 * invocation is in progress and the {@code InvocationLocal} instance is accessible. After an
 * invocation is complete, all of its copies of invocation-local instances are subject to garbage
 * collection (unless other references to these copies exist).
 *
 * <p>Note that unlike {@link ThreadLocal} instances that are no longer referenced while the
 * invocation is still in progress are not garbage collected. Creating local or non-static instances
 * is therefore not recommended as they could grow without bound.
 *
 * <p>Warning: Use this class sparingly as invocation-locals are glorified global variables with
 * many of the same pitfalls.
 */
@ThreadSafe
public class InvocationLocal<T> {
    /**
     * Returns the current invocation's "initial value" for this invocation-local variable. This
     * method will be invoked the first time code executing in the context of the invocation
     * accesses the variable with the {@link #get} method. This method is guaranteed to be invoked
     * at most once per invocation.
     *
     * <p>This implementation simply returns {@code null} but can be changed by sub-classing {@code
     * InvocationLocal} and overriding this method.
     *
     * @return the initial value for this invocation-scoped variable
     */
    protected @Nullable T initialValue() {
        return null;
    }

    /**
     * Returns the currently-executing invocation's copy of this invocation-local variable. If the
     * variable has no value for the current invocation, it is first initialized to the value
     * returned by a call to the {@link #initialValue} method.
     *
     * @return the currently executing invocation's copy of this invocation-local.
     */
    public final @Nullable T get() {
        return CurrentInvocation.getLocal(this);
    }
}
