/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.vts.util;

import com.google.cloud.datastore.testing.LocalDatastoreHelper;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.junit.jupiter.api.extension.BeforeAllCallback;
import org.junit.jupiter.api.extension.BeforeEachCallback;
import org.junit.jupiter.api.extension.ExtensionContext;
import org.junit.jupiter.api.extension.ExtensionContext.Namespace;

/** Sets up and tears down the Local Datastore emulator, defaults to strong consistency */
@RequiredArgsConstructor
@Slf4j
public class LocalDatastoreExtension implements BeforeAllCallback, BeforeEachCallback {

    private final double consistency;

    public LocalDatastoreExtension() {
        this(1.0);
    }

    @Override
    public void beforeAll(final ExtensionContext context) throws Exception {
        if (getHelper(context) == null) {
            log.info("Creating new LocalDatastoreHelper");

            final LocalDatastoreHelper helper = LocalDatastoreHelper.create(consistency);
            context.getRoot().getStore(Namespace.GLOBAL).put(LocalDatastoreHelper.class, helper);
            helper.start();
        }
    }

    @Override
    public void beforeEach(final ExtensionContext context) throws Exception {
        final LocalDatastoreHelper helper = getHelper(context);
        helper.reset();
    }

    /** Get the helper created in beforeAll; it should be global so there will one per test run */
    public static LocalDatastoreHelper getHelper(final ExtensionContext context) {
        return context.getRoot()
                .getStore(Namespace.GLOBAL)
                .get(LocalDatastoreHelper.class, LocalDatastoreHelper.class);
    }
}
