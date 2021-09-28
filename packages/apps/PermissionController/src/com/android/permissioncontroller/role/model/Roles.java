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

package com.android.permissioncontroller.role.model;

import android.content.Context;
import android.util.ArrayMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Provides access to all the {@link Role} definitions.
 */
public class Roles {

    @NonNull
    private static final Object sLock = new Object();

    @Nullable
    private static ArrayMap<String, Role> sRoles;

    private Roles() {}

    /**
     * Get the roles defined in {@code roles.xml}.
     *
     * @param context the {@code Context} used to read the XML resource
     *
     * @return a map from role name to {@link Role} instances
     */
    @NonNull
    public static ArrayMap<String, Role> get(@NonNull Context context) {
        synchronized (sLock) {
            if (sRoles == null) {
                sRoles = new RoleParser(context).parse();
            }
            return sRoles;
        }
    }
}
