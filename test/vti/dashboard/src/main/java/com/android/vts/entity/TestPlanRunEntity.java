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
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.taskqueue.Queue;
import com.google.appengine.api.taskqueue.QueueFactory;
import com.google.appengine.api.taskqueue.TaskOptions;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Ignore;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Parent;
import java.util.Date;
import java.util.List;
import java.util.Objects;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;
import lombok.Data;
import lombok.NoArgsConstructor;

@com.googlecode.objectify.annotation.Entity(name = "TestPlanRun")
@Cache
@Data
@NoArgsConstructor
/** Entity describing test plan run information. */
public class TestPlanRunEntity implements DashboardEntity {

    protected static final Logger logger = Logger.getLogger(TestPlanRunEntity.class.getName());

    private final String QUEUE_NAME = "coverageApiQueue";

    public static final String KIND = "TestPlanRun";

    private static final String COVERAGE_API_URL = "/api/coverage/api/sum";

    // Property keys
    public static final String TEST_PLAN_NAME = "testPlanName";
    public static final String TYPE = "type";
    public static final String START_TIMESTAMP = "startTimestamp";
    public static final String END_TIMESTAMP = "endTimestamp";
    public static final String TEST_BUILD_ID = "testBuildId";
    public static final String PASS_COUNT = "passCount";
    public static final String FAIL_COUNT = "failCount";
    public static final String TOTAL_API_COUNT = "totalApiCount";
    public static final String TOTAL_COVERED_API_COUNT = "coveredApiCount";
    public static final String TEST_RUNS = "testRuns";

    @Ignore public Key key;

    @Id private Long id;

    @Parent private com.googlecode.objectify.Key<TestPlanEntity> parent;

    @Index private String testPlanName;

    @Index private long type;

    @Index private long startTimestamp;

    @Index private long endTimestamp;

    @Index private String testBuildId;

    @Index private long passCount;

    @Index private long failCount;

    @Index private long totalApiCount;

    @Index private long coveredApiCount;

    @Ignore private List<Key> oldTestRuns;

    private List<com.googlecode.objectify.Key<TestRunEntity>> testRuns;

    /** When this record was created or updated */
    @Index Date updated;

    /**
     * Create a TestPlanRunEntity object describing a test plan run.
     *
     * @param testPlanKey The key for the parent entity in the database.
     * @param type The test run type (e.g. presubmit, postsubmit, other)
     * @param startTimestamp The time in microseconds when the test plan run started.
     * @param endTimestamp The time in microseconds when the test plan run ended.
     * @param testBuildId The build ID of the VTS test build.
     * @param passCount The number of passing test cases in the run.
     * @param failCount The number of failing test cases in the run.
     * @param testRuns A list of keys to the TestRunEntity objects for the plan run run.
     */
    public TestPlanRunEntity(
            Key testPlanKey,
            String testPlanName,
            long type,
            long startTimestamp,
            long endTimestamp,
            String testBuildId,
            long passCount,
            long failCount,
            long totalApiCount,
            long coveredApiCount,
            List<Key> testRuns) {
        this.id = startTimestamp;
        this.key = KeyFactory.createKey(testPlanKey, KIND, startTimestamp);
        this.testPlanName = testPlanName;
        this.type = type;
        this.startTimestamp = startTimestamp;
        this.endTimestamp = endTimestamp;
        this.testBuildId = testBuildId;
        this.passCount = passCount;
        this.failCount = failCount;
        this.totalApiCount = totalApiCount;
        this.coveredApiCount = coveredApiCount;
        this.oldTestRuns = testRuns;
        this.testRuns =
                testRuns.stream()
                        .map(
                                testRun -> {
                                    com.googlecode.objectify.Key testParentKey =
                                            com.googlecode.objectify.Key.create(
                                                    TestEntity.class,
                                                    testRun.getParent().getName());
                                    return com.googlecode.objectify.Key.create(
                                            testParentKey, TestRunEntity.class, testRun.getId());
                                })
                        .collect(Collectors.toList());
    }

    /**
     * Create a TestPlanRunEntity object describing a test plan run.
     *
     * @param testPlanKey The key for the parent entity in the database.
     * @param type The test run type (e.g. presubmit, postsubmit, other)
     * @param startTimestamp The time in microseconds when the test plan run started.
     * @param endTimestamp The time in microseconds when the test plan run ended.
     * @param testBuildId The build ID of the VTS test build.
     * @param passCount The number of passing test cases in the run.
     * @param failCount The number of failing test cases in the run.
     * @param testRuns A list of keys to the TestRunEntity objects for the plan run run.
     */
    public TestPlanRunEntity(
            com.googlecode.objectify.Key<TestPlanEntity> testPlanKey,
            String testPlanName,
            long type,
            long startTimestamp,
            long endTimestamp,
            String testBuildId,
            long passCount,
            long failCount,
            long totalApiCount,
            long coveredApiCount,
            List<com.googlecode.objectify.Key<TestRunEntity>> testRuns) {
        this.id = startTimestamp;
        this.parent = testPlanKey;
        this.testPlanName = testPlanName;
        this.type = type;
        this.startTimestamp = startTimestamp;
        this.endTimestamp = endTimestamp;
        this.testBuildId = testBuildId;
        this.passCount = passCount;
        this.failCount = failCount;
        this.totalApiCount = totalApiCount;
        this.coveredApiCount = coveredApiCount;
        this.testRuns = testRuns;
    }

    public Entity toEntity() {
        Entity planRun = new Entity(this.key);
        planRun.setProperty(TEST_PLAN_NAME, this.testPlanName);
        planRun.setProperty(TYPE, this.type);
        planRun.setProperty(START_TIMESTAMP, this.startTimestamp);
        planRun.setProperty(END_TIMESTAMP, this.endTimestamp);
        planRun.setProperty(TEST_BUILD_ID, this.testBuildId.toLowerCase());
        planRun.setProperty(PASS_COUNT, this.passCount);
        planRun.setProperty(FAIL_COUNT, this.failCount);
        if (this.oldTestRuns != null && this.oldTestRuns.size() > 0) {
            planRun.setUnindexedProperty(TEST_RUNS, this.oldTestRuns);
        }
        return planRun;
    }

    /** Saving function for the instance of this class */
    @Override
    public com.googlecode.objectify.Key<TestPlanRunEntity> save() {
        this.updated = new Date();
        return ofy().save().entity(this).now();
    }

    /** Get UrlSafeKey from this class */
    public String getUrlSafeKey() {
        return this.getOfyKey().toUrlSafe();
    }

    /** Add a task to calculate the total number of coverage API */
    public void addCoverageApiTask() {
        if (Objects.isNull(this.testRuns)) {
            logger.log(Level.WARNING, "testRuns is null so adding task to the queue is skipped!");
        } else {
            Queue queue = QueueFactory.getQueue(QUEUE_NAME);
            queue.add(
                    TaskOptions.Builder.withUrl(COVERAGE_API_URL)
                            .param("urlSafeKey", this.getUrlSafeKey())
                            .method(TaskOptions.Method.POST));
        }
    }

    /**
     * Get key info from appengine based library.
     *
     * @param parentKey parent key.
     */
    public Key getOldKey(Key parentKey) {
        return KeyFactory.createKey(parentKey, KIND, startTimestamp);
    }

    /** Get key info from objecitfy library. */
    public com.googlecode.objectify.Key getOfyKey() {
        return com.googlecode.objectify.Key.create(
                this.parent, TestPlanRunEntity.class, this.startTimestamp);
    }

    /**
     * Convert an Entity object to a TestPlanRunEntity.
     *
     * @param e The entity to process.
     * @return TestPlanRunEntity object with the properties from e processed, or null if
     *     incompatible.
     */
    @SuppressWarnings("unchecked")
    public static TestPlanRunEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND)
                || !e.hasProperty(TEST_PLAN_NAME)
                || !e.hasProperty(TYPE)
                || !e.hasProperty(START_TIMESTAMP)
                || !e.hasProperty(END_TIMESTAMP)
                || !e.hasProperty(TEST_BUILD_ID)
                || !e.hasProperty(PASS_COUNT)
                || !e.hasProperty(FAIL_COUNT)
                || !e.hasProperty(TEST_RUNS)) {
            logger.log(Level.WARNING, "Missing test run attributes in entity: " + e.toString());
            return null;
        }
        try {
            String testPlanName = (String) e.getProperty(TEST_PLAN_NAME);
            long type = (long) e.getProperty(TYPE);
            long startTimestamp = (long) e.getProperty(START_TIMESTAMP);
            long endTimestamp = (long) e.getProperty(END_TIMESTAMP);
            String testBuildId = (String) e.getProperty(TEST_BUILD_ID);
            long passCount = (long) e.getProperty(PASS_COUNT);
            long failCount = (long) e.getProperty(FAIL_COUNT);

            long totalApiCount =
                    e.hasProperty(TOTAL_API_COUNT) ? (long) e.getProperty(TOTAL_API_COUNT) : 0L;
            long coveredApiCount =
                    e.hasProperty(TOTAL_COVERED_API_COUNT)
                            ? (long) e.getProperty(TOTAL_COVERED_API_COUNT)
                            : 0L;
            List<Key> oldTestRuns = (List<Key>) e.getProperty(TEST_RUNS);
            return new TestPlanRunEntity(
                    e.getKey().getParent(),
                    testPlanName,
                    type,
                    startTimestamp,
                    endTimestamp,
                    testBuildId,
                    passCount,
                    failCount,
                    totalApiCount,
                    coveredApiCount,
                    oldTestRuns);
        } catch (ClassCastException exception) {
            // Invalid cast
            logger.log(Level.WARNING, "Error parsing test plan run entity.", exception);
        }
        return null;
    }

    public JsonObject toJson() {
        JsonObject json = new JsonObject();
        json.add(TEST_PLAN_NAME, new JsonPrimitive(this.testPlanName));
        json.add(TEST_BUILD_ID, new JsonPrimitive(this.testBuildId));
        json.add(PASS_COUNT, new JsonPrimitive(this.passCount));
        json.add(FAIL_COUNT, new JsonPrimitive(this.failCount));
        json.add(START_TIMESTAMP, new JsonPrimitive(this.startTimestamp));
        json.add(END_TIMESTAMP, new JsonPrimitive(this.endTimestamp));
        return json;
    }
}
