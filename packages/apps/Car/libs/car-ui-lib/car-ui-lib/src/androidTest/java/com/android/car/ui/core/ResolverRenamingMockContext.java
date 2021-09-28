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

package com.android.car.ui.core;

import android.content.ContentProvider;
import android.content.Context;
import android.test.IsolatedContext;
import android.test.mock.MockContentResolver;
import android.test.mock.MockContext;

public class ResolverRenamingMockContext extends IsolatedContext {

    /**
     * The renaming prefix.
     */
    private static final String PREFIX = "test.";


    /**
     * The resolver.
     */
    private static final MockContentResolver RESOLVER = new MockContentResolver();

    /**
     * Constructor.
     */
    public ResolverRenamingMockContext(Context context) {
        super(RESOLVER, new DelegatedMockContext(context));
    }

    public MockContentResolver getResolver() {
        return RESOLVER;
    }

    public void addProvider(String name, ContentProvider provider) {
        RESOLVER.addProvider(name, provider);
    }

    /**
     * The DelegatedMockContext.
     */
    private static class DelegatedMockContext extends MockContext {

        private Context mDelegatedContext;

        DelegatedMockContext(Context context) {
            mDelegatedContext = context;
        }

        @Override
        public String getPackageName() {
            return "com.android.car.ui.test";
        }
    }

}
