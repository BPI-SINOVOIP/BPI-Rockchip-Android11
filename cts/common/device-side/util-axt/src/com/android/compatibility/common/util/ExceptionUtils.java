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

package com.android.compatibility.common.util;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.function.Function;

/**
 * Utilities to deal with exceptions
 */
public class ExceptionUtils {
    private ExceptionUtils() {}

    /**
     * Rethrow a given exception, optionally wrapping it in a {@link RuntimeException}
     */
    public static RuntimeException propagate(@NonNull Throwable t) {
        if (t == null) {
            throw new NullPointerException();
        }
        propagateIfInstanceOf(t, Error.class);
        propagateIfInstanceOf(t, RuntimeException.class);
        throw new RuntimeException(t);
    }

    /**
     * Rethrow a given exception, if it's of type {@code E}
     */
    public static <E extends Throwable> void propagateIfInstanceOf(
            @Nullable Throwable t, Class<E> c) throws E {
        if (t != null && c.isInstance(t)) {
            throw c.cast(t);
        }
    }

    /**
     * Gets the root {@link Throwable#getCause() cause} of {@code t}
     */
    public static @NonNull Throwable getRootCause(@NonNull Throwable t) {
        while (t.getCause() != null) t = t.getCause();
        return t;
    }

    /**
     * Appends {@code cause} at the end of the causal chain of {@code t}
     *
     * @return {@code t} for convenience
     */
    public static @NonNull <E extends Throwable> E appendCause(@NonNull E t, @Nullable Throwable cause) {
        if (cause != null) {
            getRootCause(t).initCause(cause);
        }
        return t;
    }

    /**
     * Runs the given {@code action}, and if any exceptions are thrown in the process, applies
     * given {@code exceptionTransformer}, rethrowing the result.
     */
    public static <R> R wrappingExceptions(
            Function<Throwable, Throwable> exceptionTransformer, ThrowingSupplier<R> action) {
        try {
            return action.get();
        } catch (Throwable t) {
            Throwable transformed;
            try {
                transformed = exceptionTransformer.apply(t);
            } catch (Throwable t2) {
                transformed = new RuntimeException("Failed to apply exception transformation",
                        ExceptionUtils.appendCause(t2, t));
            }
            throw ExceptionUtils.propagate(transformed);
        }
    }

    /**
     * @see #wrappingExceptions(Function, ThrowingSupplier)
     */
    public static void wrappingExceptions(
            Function<Throwable, Throwable> exceptionTransformer, ThrowingRunnable action) {
        wrappingExceptions(exceptionTransformer, () -> {
            action.run();
            return null;
        });
    }
}
