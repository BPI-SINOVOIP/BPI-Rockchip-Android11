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

package com.android.compatibility.common.util;

import android.app.UiAutomation;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Custom JUnit4 rule that runs a test adopting Shell's permissions, revoking them at the end.
 *
 * <p>NOTE: should only be used in the cases where *every* test in a class requires the permission.
 * For a more fine-grained access, use {@link SystemUtil#runWithShellPermissionIdentity(Runnable)}
 * or {@link SystemUtil#callWithShellPermissionIdentity(java.util.concurrent.Callable)} instead.
 */
public class AdoptShellPermissionsRule implements TestRule {

    private final UiAutomation mUiAutomation;

    private final String[] mPermissions;

    public AdoptShellPermissionsRule() {
        this(InstrumentationRegistry.getInstrumentation().getUiAutomation());
    }

    public AdoptShellPermissionsRule(@NonNull UiAutomation uiAutomation) {
        this(uiAutomation, (String[]) null);
    }

    public AdoptShellPermissionsRule(@NonNull UiAutomation uiAutomation,
            @Nullable String... permissions) {
        mUiAutomation = uiAutomation;
        mPermissions = permissions;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {

            @Override
            public void evaluate() throws Throwable {
                if (mPermissions != null) {
                    mUiAutomation.adoptShellPermissionIdentity(mPermissions);
                } else {
                    mUiAutomation.adoptShellPermissionIdentity();
                }
                try {
                    base.evaluate();
                } finally {
                    mUiAutomation.dropShellPermissionIdentity();
                }
            }
        };
    }
}
