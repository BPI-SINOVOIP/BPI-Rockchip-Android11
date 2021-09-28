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

import java.util.function.Supplier;

/**
 * Similar to {@link Supplier} but has {@code throws Exception}.
 *
 * @param <T> type of the value produced
 */
public interface ThrowingSupplier<T> {
    /**
     * Similar to {@link Supplier#get} but has {@code throws Exception}.
     */
    T get() throws Exception;
}
