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
import com.google.cloud.datastore.EntityValue;
import com.google.cloud.datastore.FullEntity;
import com.google.cloud.datastore.IncompleteKey;
import com.google.cloud.datastore.Value;
import com.googlecode.objectify.Key;
import com.googlecode.objectify.cache.MemcacheService;
import com.googlecode.objectify.impl.AsyncDatastore;
import org.junit.jupiter.api.extension.ExtendWith;

import static com.googlecode.objectify.ObjectifyService.factory;
import static com.googlecode.objectify.ObjectifyService.ofy;

/** All tests should extend this class to set up the GAE environment. */
@ExtendWith({
    MockitoExtension.class,
    LocalDatastoreExtension.class,
    ObjectifyExtension.class,
})
public class ObjectifyTestBase {
    /** Set embedded entity with property name */
    protected Value<FullEntity<?>> makeEmbeddedEntityWithProperty(
            final String name, final Value<?> value) {
        return EntityValue.of(FullEntity.newBuilder().set(name, value).build());
    }

    /** Get datastore instance */
    protected Datastore datastore() {
        return factory().datastore();
    }

    /** Get memcache instance */
    protected MemcacheService memcache() {
        return factory().memcache();
    }

    /** Get asynchronous datastore instance */
    protected AsyncDatastore asyncDatastore() {
        return factory().asyncDatastore();
    }

    /** Save an entity and clear cache data and return entity by finding the key */
    protected <E> E saveClearLoad(final E thing) {
        final Key<E> key = ofy().save().entity(thing).now();
        ofy().clear();
        return ofy().load().key(key).now();
    }

    /** Get the entity instance from class type */
    protected FullEntity.Builder<?> makeEntity(final Class<?> kind) {
        return makeEntity(Key.getKind(kind));
    }

    /** Get the entity instance from class name */
    protected FullEntity.Builder<?> makeEntity(final String kind) {
        final IncompleteKey incompleteKey =
                factory().datastore().newKeyFactory().setKind(kind).newKey();
        return FullEntity.newBuilder(incompleteKey);
    }
}
