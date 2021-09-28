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

import com.google.appengine.api.datastore.Entity;
import com.googlecode.objectify.Key;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Index;
import java.util.Collection;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

@com.googlecode.objectify.annotation.Entity(name = "TestCoverageStatus")
@EqualsAndHashCode(of = "testName")
@Cache
@NoArgsConstructor
/** Entity describing test coverage status. */
public class TestCoverageStatusEntity implements DashboardEntity {

    protected static final Logger logger =
            Logger.getLogger(TestCoverageStatusEntity.class.getName());

    public static final String KIND = "TestCoverageStatus";

    // Property keys
    public static final String TOTAL_LINE_COUNT = "totalLineCount";
    public static final String COVERED_LINE_COUNT = "coveredLineCount";
    public static final String UPDATED_TIMESTAMP = "updatedTimestamp";
    public static final String DEVICE_INFO_ID = "deviceInfoId";

    /** TestCoverageStatusEntity name field */
    @Id @Getter @Setter String testName;

    /** TestCoverageStatusEntity coveredLineCount field */
    @Index @Getter @Setter long coveredLineCount;

    /** TestCoverageStatusEntity totalLineCount field */
    @Index @Getter @Setter long totalLineCount;

    /** TestCoverageStatusEntity updatedTimestamp field */
    @Index @Getter @Setter long updatedTimestamp;

    /** TestCoverageStatusEntity DeviceInfo Entity ID reference field */
    @Index @Getter @Setter long deviceInfoId;

    /** TestCoverageStatusEntity updatedCoveredLineCount field */
    @Index @Getter @Setter long updatedCoveredLineCount;

    /** TestCoverageStatusEntity updatedTotalLineCount field */
    @Index @Getter @Setter long updatedTotalLineCount;

    /** TestCoverageStatusEntity updatedDate field */
    @Index @Getter @Setter Date updatedDate;

    /**
     * Create a TestCoverageStatusEntity object with status metadata.
     *
     * @param testName The name of the test.
     * @param timestamp The timestamp indicating the most recent test run event in the test state.
     * @param coveredLineCount The number of lines covered.
     * @param totalLineCount The total number of lines.
     */
    public TestCoverageStatusEntity(
            String testName,
            long timestamp,
            long coveredLineCount,
            long totalLineCount,
            long deviceInfoId) {
        this.testName = testName;
        this.updatedTimestamp = timestamp;
        this.coveredLineCount = coveredLineCount;
        this.totalLineCount = totalLineCount;
        this.deviceInfoId = deviceInfoId;
    }

    /** find TestCoverageStatus entity by ID */
    public static TestCoverageStatusEntity findById(String testName) {
        return ofy().load().type(TestCoverageStatusEntity.class).id(testName).now();
    }

    /** Get all TestCoverageStatusEntity List */
    public static Map<String, TestCoverageStatusEntity> getTestCoverageStatusMap() {
        List<TestCoverageStatusEntity> testCoverageStatusEntityList = getAllTestCoverage();

        Map<String, TestCoverageStatusEntity> testCoverageStatusMap =
                testCoverageStatusEntityList.stream()
                        .collect(Collectors.toMap(t -> t.getTestName(), t -> t));
        return testCoverageStatusMap;
    }

    /** Get all DeviceInfoEntity List by TestCoverageStatusEntities' key list */
    public static List<Key<DeviceInfoEntity>> getDeviceInfoEntityKeyList(
            List<TestCoverageStatusEntity> testCoverageStatusEntityList) {
        return testCoverageStatusEntityList
                .stream()
                .map(
                        testCoverageStatusEntity -> {
                            com.googlecode.objectify.Key testKey =
                                    com.googlecode.objectify.Key.create(
                                            TestEntity.class,
                                            testCoverageStatusEntity.getTestName());
                            com.googlecode.objectify.Key testRunKey =
                                    com.googlecode.objectify.Key.create(
                                            testKey,
                                            TestRunEntity.class,
                                            testCoverageStatusEntity.getUpdatedTimestamp());
                            return com.googlecode.objectify.Key.create(
                                    testRunKey,
                                    DeviceInfoEntity.class,
                                    testCoverageStatusEntity.getDeviceInfoId());
                        })
                .collect(Collectors.toList());
    }

    /** Get all TestCoverageStatusEntity List */
    public static List<TestCoverageStatusEntity> getAllTestCoverage() {
        return ofy().load().type(TestCoverageStatusEntity.class).list();
    }

    /** Get all TestCoverageStatusEntities' Branch List */
    public static Set<String> getBranchSet(
            List<TestCoverageStatusEntity> testCoverageStatusEntityList) {
        List<com.googlecode.objectify.Key<DeviceInfoEntity>> deviceInfoEntityKeyList =
                getDeviceInfoEntityKeyList(testCoverageStatusEntityList);

        Collection<DeviceInfoEntity> deviceInfoEntityList =
                ofy().load().keys(() -> deviceInfoEntityKeyList.iterator()).values();

        Set<String> branchSet =
                deviceInfoEntityList
                        .stream()
                        .map(deviceInfoEntity -> deviceInfoEntity.getBranch())
                        .collect(Collectors.toSet());
        return branchSet;
    }

    /** Get all TestCoverageStatusEntities' Device List */
    public static Set<String> getDeviceSet(
            List<TestCoverageStatusEntity> testCoverageStatusEntityList) {
        List<com.googlecode.objectify.Key<DeviceInfoEntity>> deviceInfoEntityKeyList =
                getDeviceInfoEntityKeyList(testCoverageStatusEntityList);

        Collection<DeviceInfoEntity> deviceInfoEntityList =
                ofy().load().keys(() -> deviceInfoEntityKeyList.iterator()).values();

        Set<String> deviceSet =
                deviceInfoEntityList
                        .stream()
                        .map(deviceInfoEntity -> deviceInfoEntity.getBuildFlavor())
                        .collect(Collectors.toSet());
        return deviceSet;
    }

    /** TestRunEntity function to get the related TestRunEntity from id value */
    public TestRunEntity getTestRunEntity() {
        com.googlecode.objectify.Key testKey =
                com.googlecode.objectify.Key.create(TestEntity.class, this.testName);
        return ofy().load()
                .type(TestRunEntity.class)
                .parent(testKey)
                .id(this.updatedTimestamp)
                .now();
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<TestCoverageStatusEntity> save() {
        this.updatedDate = new Date();
        return ofy().save().entity(this).now();
    }

    public Entity toEntity() {
        Entity testEntity = new Entity(KIND, this.testName);
        testEntity.setProperty(UPDATED_TIMESTAMP, this.updatedTimestamp);
        testEntity.setProperty(COVERED_LINE_COUNT, this.coveredLineCount);
        testEntity.setProperty(TOTAL_LINE_COUNT, this.totalLineCount);
        return testEntity;
    }

    /**
     * Convert an Entity object to a TestCoverageStatusEntity.
     *
     * @param e The entity to process.
     * @return TestCoverageStatusEntity object with the properties from e processed, or null if
     *     incompatible.
     */
    @SuppressWarnings("unchecked")
    public static TestCoverageStatusEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND)
                || e.getKey().getName() == null
                || !e.hasProperty(UPDATED_TIMESTAMP)
                || !e.hasProperty(COVERED_LINE_COUNT)
                || !e.hasProperty(TOTAL_LINE_COUNT)
                || !e.hasProperty(DEVICE_INFO_ID)) {
            logger.log(Level.WARNING, "Missing test attributes in entity: " + e.toString());
            return null;
        }
        String testName = e.getKey().getName();
        long timestamp = 0;
        long coveredLineCount = -1;
        long totalLineCount = -1;
        long deviceInfoId = 0;
        try {
            timestamp = (long) e.getProperty(UPDATED_TIMESTAMP);
            coveredLineCount = (Long) e.getProperty(COVERED_LINE_COUNT);
            totalLineCount = (Long) e.getProperty(TOTAL_LINE_COUNT);
            deviceInfoId = (Long) e.getProperty(DEVICE_INFO_ID);
        } catch (ClassCastException exception) {
            // Invalid contents or null values
            logger.log(Level.WARNING, "Error parsing test entity.", exception);
            return null;
        }
        return new TestCoverageStatusEntity(
                testName, timestamp, coveredLineCount, totalLineCount, deviceInfoId);
    }
}
