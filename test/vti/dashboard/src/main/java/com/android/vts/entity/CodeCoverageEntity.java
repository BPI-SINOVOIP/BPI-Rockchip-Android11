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

package com.android.vts.entity;

import com.fasterxml.jackson.annotation.JsonAutoDetect;
import com.fasterxml.jackson.annotation.JsonAutoDetect.Visibility;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.google.appengine.api.datastore.KeyFactory;
import com.googlecode.objectify.Key;

import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Entity;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Parent;
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

import java.util.Date;
import java.util.logging.Logger;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Entity Class for CodeCoverageEntity */
@Cache
@Entity(name = "CodeCoverage")
@EqualsAndHashCode(of = "id")
@NoArgsConstructor
@JsonAutoDetect(fieldVisibility = Visibility.ANY)
@JsonIgnoreProperties({"id", "parent"})
public class CodeCoverageEntity implements DashboardEntity {
    protected static final Logger logger = Logger.getLogger(CodeCoverageEntity.class.getName());

    public static final String KIND = "CodeCoverage";

    public static final String COVERED_LINE_COUNT = "coveredLineCount";
    public static final String TOTAL_LINE_COUNT = "totalLineCount";

    /** CodeCoverageEntity id field */
    @Id @Getter @Setter Long id;

    @Parent @Getter Key<?> parent;

    @Index @Getter @Setter private long coveredLineCount;

    @Index @Getter @Setter private long totalLineCount;

    /** When this record was created or updated */
    @Index Date updated;

    /** Constructor function for ApiCoverageEntity Class */
    public CodeCoverageEntity(
            com.google.appengine.api.datastore.Key testRunKey,
            long coveredLineCount,
            long totalLineCount) {

        this.parent = getParentKey(testRunKey);

        this.coveredLineCount = coveredLineCount;
        this.totalLineCount = totalLineCount;
    }

    /** Constructor function for ApiCoverageEntity Class */
    public CodeCoverageEntity(
            long id,
            com.google.appengine.api.datastore.Key testRunKey,
            long coveredLineCount,
            long totalLineCount) {
        this.id = id;

        this.parent = getParentKey(testRunKey);

        this.coveredLineCount = coveredLineCount;
        this.totalLineCount = totalLineCount;
    }

    /** Constructor function for ApiCoverageEntity Class with objectify key*/
    public CodeCoverageEntity(Key testRunKey, long coveredLineCount, long totalLineCount) {
        this.parent = testRunKey;
        this.coveredLineCount = coveredLineCount;
        this.totalLineCount = totalLineCount;
    }

    /** Get objectify Key from datastore Key type */
    private Key getParentKey(com.google.appengine.api.datastore.Key testRunKey) {
        Key testParentKey = Key.create(TestEntity.class, testRunKey.getParent().getName());
        return Key.create(testParentKey, TestRunEntity.class, testRunKey.getId());
    }

    /** Get UrlSafeKey from ApiCoverageEntity Information */
    public String getUrlSafeKey() {
        Key uuidKey = Key.create(this.parent, CodeCoverageEntity.class, this.id);
        return uuidKey.toUrlSafe();
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<CodeCoverageEntity> save() {
        this.id = this.getParent().getId();
        this.updated = new Date();
        return ofy().save().entity(this).now();
    }

    public com.google.appengine.api.datastore.Entity toEntity() {
        com.google.appengine.api.datastore.Key testKey =
                KeyFactory.createKey(TestEntity.KIND, this.getParent().getParent().getName());
        com.google.appengine.api.datastore.Key testRunKey =
                KeyFactory.createKey(testKey, TestRunEntity.KIND, this.getParent().getId());
        com.google.appengine.api.datastore.Key codeCoverageKey =
                KeyFactory.createKey(testRunKey, KIND, this.getParent().getId());

        com.google.appengine.api.datastore.Entity codeCoverageEntity =
                new com.google.appengine.api.datastore.Entity(codeCoverageKey);
        codeCoverageEntity.setProperty(COVERED_LINE_COUNT, this.coveredLineCount);
        codeCoverageEntity.setProperty(TOTAL_LINE_COUNT, this.totalLineCount);
        codeCoverageEntity.setIndexedProperty("updated", new Date());
        return codeCoverageEntity;
    }
}
