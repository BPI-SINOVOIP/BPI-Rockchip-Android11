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

import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import static com.googlecode.objectify.ObjectifyService.ofy;

@com.googlecode.objectify.annotation.Entity(name = "Branch")
@Cache
@Data
@NoArgsConstructor
/** Entity describing a branch. */
public class BranchEntity implements DashboardEntity {
    protected static final Logger logger = Logger.getLogger(BranchEntity.class.getName());

    public static final String KIND = "Branch"; // The entity kind.

    @Id private String name;

    /**
     * Create a BranchEntity object.
     *
     * @param branchName The name of the branch.
     */
    public BranchEntity(String branchName) {
        this.name = branchName;
    }

    public Key getKey() {
        return KeyFactory.createKey(KIND, this.name);
    }

    /** find by branch name */
    public static List<String> getByBranch(String branchName) {
        if (branchName.equals("*")) {
            return ofy().load()
                    .type(BranchEntity.class)
                    .limit(100)
                    .list()
                    .stream()
                    .map(b -> b.name)
                    .collect(Collectors.toList());
        } else {
            com.googlecode.objectify.Key startKey =
                    com.googlecode.objectify.Key.create(BranchEntity.class, branchName);

            int lastPosition = branchName.length() - 1;
            int lastCharValue = branchName.charAt(lastPosition);
            String nextChar = String.valueOf((char) (lastCharValue + 1));

            String nextBranchName = branchName.substring(0, lastPosition) + nextChar;
            com.googlecode.objectify.Key endKey =
                    com.googlecode.objectify.Key.create(BranchEntity.class, nextBranchName);
            return ofy().load()
                    .type(BranchEntity.class)
                    .filterKey(">=", startKey)
                    .filterKey("<", endKey)
                    .list()
                    .stream()
                    .map(b -> b.name)
                    .collect(Collectors.toList());
        }
    }

    /** Saving function for the instance of this class */
    @Override
    public com.googlecode.objectify.Key<BranchEntity> save() {
        return ofy().save().entity(this).now();
    }

    /**
     * Convert an Entity object to a BranchEntity.
     *
     * @param e The entity to process.
     * @return BranchEntity object with the properties from e, or null if incompatible.
     */
    public static BranchEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND) || e.getKey().getName() == null) {
            logger.log(Level.WARNING, "Missing branch attributes in entity: " + e.toString());
            return null;
        }
        String branchName = e.getKey().getName();
        return new BranchEntity(branchName);
    }
}
