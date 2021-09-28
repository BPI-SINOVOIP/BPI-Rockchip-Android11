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

import com.google.cloud.datastore.Datastore;
import com.googlecode.objectify.ObjectifyFactory;
import com.googlecode.objectify.ObjectifyService;
import com.googlecode.objectify.util.Closeable;
import org.junit.jupiter.api.extension.AfterEachCallback;
import org.junit.jupiter.api.extension.BeforeEachCallback;
import org.junit.jupiter.api.extension.ExtensionContext;
import org.junit.jupiter.api.extension.ExtensionContext.Namespace;

/** Sets up and tears down the GAE local unit test harness environment */
public class ObjectifyExtension implements BeforeEachCallback, AfterEachCallback {

    private static final Namespace NAMESPACE = Namespace.create(ObjectifyExtension.class);

    @Override
    public void beforeEach(final ExtensionContext context) throws Exception {
        final Datastore datastore =
                LocalDatastoreExtension.getHelper(context).getOptions().getService();

        ObjectifyService.init(new ObjectifyFactory(datastore));

        final Closeable rootService = ObjectifyService.begin();

        context.getStore(NAMESPACE).put(Closeable.class, rootService);
    }

    @Override
    public void afterEach(final ExtensionContext context) throws Exception {
        final Closeable rootService =
                context.getStore(NAMESPACE).get(Closeable.class, Closeable.class);

        rootService.close();
    }
}
