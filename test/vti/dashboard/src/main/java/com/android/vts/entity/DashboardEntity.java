/*
 * Copyright (c) 2017 Google Inc. All Rights Reserved.
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

package com.android.vts.entity;

import com.google.common.collect.Lists;
import com.googlecode.objectify.Key;

import java.io.Serializable;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Interface for interacting with VTS Dashboard entities in Cloud Datastore. */
public interface DashboardEntity extends Serializable {
    /**
     * Save the Entity to the datastore.
     *
     * @return The saved entity's key value.
     */
    <T> Key<T> save();

    /** Save List of entity through objectify entities method. */
    static <T> Map<Key<T>, T> saveAll(List<T> entityList, int maxEntitySize) {
        return ofy().transact(
                        () -> {
                            List<List<T>> partitionedList =
                                    Lists.partition(entityList, maxEntitySize);
                            return partitionedList
                                    .stream()
                                    .map(
                                            subEntityList ->
                                                    ofy().save().entities(subEntityList).now())
                                    .flatMap(m -> m.entrySet().stream())
                                    .collect(
                                            Collectors.toMap(
                                                    entry -> entry.getKey(),
                                                    entry -> entry.getValue()));
                        });
    }
}
