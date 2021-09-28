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

import static com.googlecode.objectify.ObjectifyService.ofy;

import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Entity;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Index;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;
import lombok.Data;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

@Entity(name = "Test")
@Cache
@Data
@NoArgsConstructor
/** Entity describing test metadata. */
public class TestEntity implements DashboardEntity {
    protected static final Logger logger = Logger.getLogger(TestEntity.class.getName());

    public static final String KIND = "Test";
    public static final String HAS_PROFILING_DATA = "hasProfilingData";

    @Id
    @Getter
    @Setter
    private String testName;

    @Index
    @Getter
    @Setter
    private boolean hasProfilingData;

    /**
     * Create a TestEntity object.
     *
     * @param testName The name of the test.
     * @param hasProfilingData True if the test includes profiling data.
     */
    public TestEntity(String testName, boolean hasProfilingData) {
        this.testName = testName;
        this.hasProfilingData = hasProfilingData;
    }

    /**
     * Create a TestEntity object.
     *
     * @param testName The name of the test.
     */
    public TestEntity(String testName) {
        this(testName, false);
    }

    /** Saving function for the instance of this class */
    @Override
    public com.googlecode.objectify.Key<TestEntity> save() {
        return ofy().save().entity(this).now();
    }

    public com.google.appengine.api.datastore.Entity toEntity() {
        com.google.appengine.api.datastore.Entity testEntity = new com.google.appengine.api.datastore.Entity(this.getOldKey());
        testEntity.setProperty(HAS_PROFILING_DATA, this.hasProfilingData);
        return testEntity;
    }

    /**
     * Get objectify TestRun Entity's key.
     *
     * @param startTimestamp test start timestamp
     */
    public com.googlecode.objectify.Key getTestRunKey(long startTimestamp) {
        com.googlecode.objectify.Key testKey =
                com.googlecode.objectify.Key.create(TestEntity.class, this.getTestName());
        com.googlecode.objectify.Key testRunKey =
                com.googlecode.objectify.Key.create(testKey, TestRunEntity.class, startTimestamp);
        return testRunKey;
    }

    /**
     * Get key info from appengine based library.
     */
    public Key getOldKey() {
        return KeyFactory.createKey(KIND, testName);
    }

    public static List<String> getAllTestNames() {
        List<TestEntity> testEntityList = getAllTest();

        List<String> allTestNames = testEntityList.stream()
            .map(te -> te.getTestName()).collect(Collectors.toList());
        return allTestNames;
    }

    public static List<TestEntity> getAllTest() {
        return ofy().load().type(TestEntity.class).list();
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null || !TestEntity.class.isAssignableFrom(obj.getClass())) {
            return false;
        }
        TestEntity test2 = (TestEntity) obj;
        return (
            this.testName.equals(test2.testName) &&
                this.hasProfilingData == test2.hasProfilingData);
    }

    /**
     * Convert an Entity object to a TestEntity.
     *
     * @param e The entity to process.
     * @return TestEntity object with the properties from e processed, or null if incompatible.
     */
    @SuppressWarnings("unchecked")
    public static TestEntity fromEntity(com.google.appengine.api.datastore.Entity e) {
        if (!e.getKind().equals(KIND) || e.getKey().getName() == null) {
            logger.log(Level.WARNING, "Missing test attributes in entity: " + e.toString());
            return null;
        }
        String testName = e.getKey().getName();
        boolean hasProfilingData = false;
        if (e.hasProperty(HAS_PROFILING_DATA)) {
            hasProfilingData = (boolean) e.getProperty(HAS_PROFILING_DATA);
        }
        return new TestEntity(testName, hasProfilingData);
    }
}
