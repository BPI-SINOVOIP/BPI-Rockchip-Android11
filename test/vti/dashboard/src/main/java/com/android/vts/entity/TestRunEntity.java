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

import com.android.vts.util.TimeUtil;
import com.android.vts.util.UrlUtil;
import com.android.vts.util.UrlUtil.LinkDisplay;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Ignore;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.OnLoad;
import com.googlecode.objectify.annotation.Parent;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.function.Supplier;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Stream;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;
import org.apache.commons.lang3.math.NumberUtils;

@com.googlecode.objectify.annotation.Entity(name = "TestRun")
@Cache
@NoArgsConstructor
/** Entity describing test run information. */
public class TestRunEntity implements DashboardEntity {
    protected static final Logger logger = Logger.getLogger(TestRunEntity.class.getName());

    /** Enum for classifying test run types. */
    public enum TestRunType {
        OTHER(0),
        PRESUBMIT(1),
        POSTSUBMIT(2);

        private final int value;

        private TestRunType(int value) {
            this.value = value;
        }

        /**
         * Get the ordinal representation of the type.
         *
         * @return The value associated with the test run type.
         */
        public int getNumber() {
            return value;
        }

        /**
         * Convert an ordinal value to a TestRunType.
         *
         * @param value The orginal value to parse.
         * @return a TestRunType value.
         */
        public static TestRunType fromNumber(int value) {
            if (value == 1) {
                return TestRunType.PRESUBMIT;
            } else if (value == 2) {
                return TestRunType.POSTSUBMIT;
            } else {
                return TestRunType.OTHER;
            }
        }

        /**
         * Determine the test run type based on the build ID.
         *
         * <p>Postsubmit runs are expected to have integer build IDs, while presubmit runs are
         * integers prefixed by the character P. All other runs (e.g. local builds) are classified
         * as OTHER.
         *
         * @param buildId The build ID.
         * @return the TestRunType.
         */
        public static TestRunType fromBuildId(String buildId) {
            if (buildId.toLowerCase().startsWith("p")) {
                if (NumberUtils.isParsable(buildId.substring(1))) {
                    return TestRunType.PRESUBMIT;
                } else {
                    return TestRunType.OTHER;
                }
            } else if (NumberUtils.isParsable(buildId)) {
                return TestRunType.POSTSUBMIT;
            } else {
                return TestRunType.OTHER;
            }
        }
    }

    public static final String KIND = "TestRun";

    // Property keys
    public static final String TEST_NAME = "testName";
    public static final String TYPE = "type";
    public static final String START_TIMESTAMP = "startTimestamp";
    public static final String END_TIMESTAMP = "endTimestamp";
    public static final String TEST_BUILD_ID = "testBuildId";
    public static final String HOST_NAME = "hostName";
    public static final String PASS_COUNT = "passCount";
    public static final String FAIL_COUNT = "failCount";
    public static final String HAS_CODE_COVERAGE = "hasCodeCoverage";
    public static final String HAS_COVERAGE = "hasCoverage";
    public static final String TEST_CASE_IDS = "testCaseIds";
    public static final String LOG_LINKS = "logLinks";
    public static final String API_COVERAGE_KEY_LIST = "apiCoverageKeyList";
    public static final String TOTAL_API_COUNT = "totalApiCount";
    public static final String COVERED_API_COUNT = "coveredApiCount";

    @Ignore private Key key;

    @Id @Getter @Setter private Long id;

    @Parent @Getter @Setter private com.googlecode.objectify.Key<?> testRunParent;

    @Index @Getter @Setter private long type;

    @Index @Getter @Setter private long startTimestamp;

    @Index @Getter @Setter private long endTimestamp;

    @Index @Getter @Setter private String testBuildId;

    @Index @Getter @Setter private String testName;

    @Index @Getter @Setter private String hostName;

    @Index @Getter @Setter private long passCount;

    @Index @Getter @Setter private long failCount;

    @Index private boolean hasCoverage;

    @Index @Getter @Setter private boolean hasCodeCoverage;

    @Ignore private com.googlecode.objectify.Key<CodeCoverageEntity> codeCoverageEntityKey;

    @Index @Getter @Setter private long coveredLineCount;

    @Index @Getter @Setter private long totalLineCount;

    @Getter @Setter private List<Long> testCaseIds;

    @Getter @Setter private List<String> logLinks;

    /**
     * Create a TestRunEntity object describing a test run.
     *
     * @param parentKey The key to the parent TestEntity.
     * @param type The test run type (e.g. presubmit, postsubmit, other)
     * @param startTimestamp The time in microseconds when the test run started.
     * @param endTimestamp The time in microseconds when the test run ended.
     * @param testBuildId The build ID of the VTS test build.
     * @param hostName The name of host machine.
     * @param passCount The number of passing test cases in the run.
     * @param failCount The number of failing test cases in the run.
     */
    public TestRunEntity(
            Key parentKey,
            long type,
            long startTimestamp,
            long endTimestamp,
            String testBuildId,
            String hostName,
            long passCount,
            long failCount,
            boolean hasCodeCoverage,
            List<Long> testCaseIds,
            List<String> logLinks) {
        this.id = startTimestamp;
        this.key = KeyFactory.createKey(parentKey, KIND, startTimestamp);
        this.type = type;
        this.startTimestamp = startTimestamp;
        this.endTimestamp = endTimestamp;
        this.testBuildId = testBuildId;
        this.hostName = hostName;
        this.passCount = passCount;
        this.failCount = failCount;
        this.hasCodeCoverage = hasCodeCoverage;
        this.testName = parentKey.getName();
        this.testCaseIds = testCaseIds;
        this.logLinks = logLinks;

        this.testRunParent = com.googlecode.objectify.Key.create(TestEntity.class, testName);
        this.codeCoverageEntityKey = getCodeCoverageEntityKey();
    }

    /**
     * Called after the POJO is populated with data through objecitfy library
     */
    @OnLoad
    private void onLoad() {
        if (Objects.isNull(this.hasCodeCoverage)) {
            this.hasCodeCoverage = this.hasCoverage;
            this.save();
        }
    }

    public Entity toEntity() {
        Entity testRunEntity = new Entity(this.key);
        testRunEntity.setProperty(TEST_NAME, this.testName);
        testRunEntity.setProperty(TYPE, this.type);
        testRunEntity.setProperty(START_TIMESTAMP, this.startTimestamp);
        testRunEntity.setUnindexedProperty(END_TIMESTAMP, this.endTimestamp);
        testRunEntity.setProperty(TEST_BUILD_ID, this.testBuildId.toLowerCase());
        testRunEntity.setProperty(HOST_NAME, this.hostName.toLowerCase());
        testRunEntity.setProperty(PASS_COUNT, this.passCount);
        testRunEntity.setProperty(FAIL_COUNT, this.failCount);
        testRunEntity.setProperty(HAS_CODE_COVERAGE, this.hasCodeCoverage);
        testRunEntity.setUnindexedProperty(TEST_CASE_IDS, this.testCaseIds);
        if (this.logLinks != null && this.logLinks.size() > 0) {
            testRunEntity.setUnindexedProperty(LOG_LINKS, this.logLinks);
        }
        return testRunEntity;
    }

    /** Saving function for the instance of this class */
    @Override
    public com.googlecode.objectify.Key<TestRunEntity> save() {
        return ofy().save().entity(this).now();
    }

    /**
     * Get key info from appengine based library.
     */
    public Key getKey() {
        Key parentKey = KeyFactory.createKey(TestEntity.KIND, testName);
        return KeyFactory.createKey(parentKey, KIND, startTimestamp);
    }

    /** Getter hasCodeCoverage value */
    public boolean getHasCodeCoverage() {
        return this.hasCodeCoverage;
    }

    /** Getter DateTime string from startTimestamp */
    public String getStartDateTime() {
        return TimeUtil.getDateTimeString(this.startTimestamp);
    }

    /** Getter DateTime string from startTimestamp */
    public String getEndDateTime() {
        return TimeUtil.getDateTimeString(this.endTimestamp);
    }

    /** find TestRun entity by ID and test name */
    public static TestRunEntity getByTestNameId(String testName, long id) {
        com.googlecode.objectify.Key testKey =
                com.googlecode.objectify.Key.create(TestEntity.class, testName);
        return ofy().load().type(TestRunEntity.class).parent(testKey).id(id).now();
    }

    /** Get CodeCoverageEntity Key to generate Key by combining key info */
    private com.googlecode.objectify.Key getCodeCoverageEntityKey() {
        com.googlecode.objectify.Key testRunKey = this.getOfyKey();
        return com.googlecode.objectify.Key.create(
                        testRunKey, CodeCoverageEntity.class, this.startTimestamp);
    }

    /** Get ApiCoverageEntity Key from the parent key */
    public com.googlecode.objectify.Key getOfyKey() {
        com.googlecode.objectify.Key testKey =
                com.googlecode.objectify.Key.create(
                        TestEntity.class, this.testName);
        com.googlecode.objectify.Key testRunKey =
                com.googlecode.objectify.Key.create(
                        testKey, TestRunEntity.class, this.startTimestamp);
        return testRunKey;
    }

    /** Get ApiCoverageEntity from key info */
    public Optional<List<ApiCoverageEntity>> getApiCoverageEntityList() {
        com.googlecode.objectify.Key testRunKey = this.getOfyKey();
        List<ApiCoverageEntity> apiCoverageEntityList =
                ofy().load().type(ApiCoverageEntity.class).ancestor(testRunKey).list();
        return Optional.ofNullable(apiCoverageEntityList);
    }

    /**
     * Get CodeCoverageEntity instance from codeCoverageEntityKey value.
     */
    public CodeCoverageEntity getCodeCoverageEntity() {
        if (this.hasCodeCoverage) {
            CodeCoverageEntity codeCoverageEntity =
                    ofy().load()
                            .type(CodeCoverageEntity.class)
                            .filterKey(this.codeCoverageEntityKey)
                            .first()
                            .now();
            if (Objects.isNull(codeCoverageEntity)) {
                codeCoverageEntity =
                        new CodeCoverageEntity(
                                this.getKey(), coveredLineCount, totalLineCount);
                codeCoverageEntity.save();
                return codeCoverageEntity;
            } else {
                return codeCoverageEntity;
            }
        } else {
            logger.log(
                    Level.WARNING,
                    "The hasCodeCoverage value is false. Please check the code coverage entity key");
            return null;
        }
    }

    /**
     * Convert an Entity object to a TestRunEntity.
     *
     * @param e The entity to process.
     * @return TestRunEntity object with the properties from e processed, or null if incompatible.
     */
    @SuppressWarnings("unchecked")
    public static TestRunEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND)
                || !e.hasProperty(TYPE)
                || !e.hasProperty(START_TIMESTAMP)
                || !e.hasProperty(END_TIMESTAMP)
                || !e.hasProperty(TEST_BUILD_ID)
                || !e.hasProperty(HOST_NAME)
                || !e.hasProperty(PASS_COUNT)
                || !e.hasProperty(FAIL_COUNT)) {
            logger.log(Level.WARNING, "Missing test run attributes in entity: " + e.toString());
            return null;
        }
        try {
            long type = (long) e.getProperty(TYPE);
            long startTimestamp = (long) e.getProperty(START_TIMESTAMP);
            long endTimestamp = (long) e.getProperty(END_TIMESTAMP);
            String testBuildId = (String) e.getProperty(TEST_BUILD_ID);
            String hostName = (String) e.getProperty(HOST_NAME);
            long passCount = (long) e.getProperty(PASS_COUNT);
            long failCount = (long) e.getProperty(FAIL_COUNT);
            boolean hasCodeCoverage = false;
            if (e.hasProperty(HAS_CODE_COVERAGE)) {
                hasCodeCoverage = (boolean) e.getProperty(HAS_CODE_COVERAGE);
            } else {
                hasCodeCoverage = (boolean) e.getProperty(HAS_COVERAGE);
            }
            List<Long> testCaseIds = (List<Long>) e.getProperty(TEST_CASE_IDS);
            if (Objects.isNull(testCaseIds)) {
                testCaseIds = new ArrayList<>();
            }
            List<String> links = new ArrayList<>();
            if (e.hasProperty(LOG_LINKS)) {
                links = (List<String>) e.getProperty(LOG_LINKS);
            }
            return new TestRunEntity(
                    e.getKey().getParent(),
                    type,
                    startTimestamp,
                    endTimestamp,
                    testBuildId,
                    hostName,
                    passCount,
                    failCount,
                    hasCodeCoverage,
                    testCaseIds,
                    links);
        } catch (ClassCastException exception) {
            // Invalid cast
            logger.log(Level.WARNING, "Error parsing test run entity.", exception);
        }
        return null;
    }

    /** Get JsonFormat logLinks */
    public JsonElement getJsonLogLinks() {
        List<String> logLinks = this.getLogLinks();
        List<JsonElement> links = new ArrayList<>();
        if (logLinks != null && logLinks.size() > 0) {
            for (String rawUrl : logLinks) {
                UrlUtil.LinkDisplay validatedLink = UrlUtil.processUrl(rawUrl);
                if (validatedLink == null) {
                    logger.log(Level.WARNING, "Invalid logging URL : " + rawUrl);
                    continue;
                }
                String[] logInfo = new String[] {validatedLink.name, validatedLink.url};
                links.add(new Gson().toJsonTree(logInfo));
            }
        }
        return new Gson().toJsonTree(links);
    }

    public JsonObject toJson() {
        Map<String, TestCoverageStatusEntity> testCoverageStatusMap = TestCoverageStatusEntity
                .getTestCoverageStatusMap();

        JsonObject json = new JsonObject();
        json.add(TEST_NAME, new JsonPrimitive(this.testName));
        json.add(TEST_BUILD_ID, new JsonPrimitive(this.testBuildId));
        json.add(HOST_NAME, new JsonPrimitive(this.hostName));
        json.add(PASS_COUNT, new JsonPrimitive(this.passCount));
        json.add(FAIL_COUNT, new JsonPrimitive(this.failCount));
        json.add(START_TIMESTAMP, new JsonPrimitive(this.startTimestamp));
        json.add(END_TIMESTAMP, new JsonPrimitive(this.endTimestamp));

        // Overwrite the coverage value with newly update value from user decision
        if (this.hasCodeCoverage) {
            CodeCoverageEntity codeCoverageEntity = this.getCodeCoverageEntity();
            if (testCoverageStatusMap.containsKey(this.testName)) {
                TestCoverageStatusEntity testCoverageStatusEntity =
                        testCoverageStatusMap.get(this.testName);

                if (testCoverageStatusEntity.getUpdatedCoveredLineCount() > 0) {
                    codeCoverageEntity.setCoveredLineCount(
                            testCoverageStatusEntity.getUpdatedCoveredLineCount());
                }
                if (testCoverageStatusEntity.getUpdatedTotalLineCount() > 0) {
                    codeCoverageEntity.setTotalLineCount(
                            testCoverageStatusEntity.getUpdatedTotalLineCount());
                }
            }

            long totalLineCount = codeCoverageEntity.getTotalLineCount();
            long coveredLineCount = codeCoverageEntity.getCoveredLineCount();
            if (totalLineCount > 0 && coveredLineCount >= 0) {
                json.add(CodeCoverageEntity.COVERED_LINE_COUNT, new JsonPrimitive(coveredLineCount));
                json.add(CodeCoverageEntity.TOTAL_LINE_COUNT, new JsonPrimitive(totalLineCount));
            }
        }

        Optional<List<ApiCoverageEntity>> apiCoverageEntityOptionList =
                this.getApiCoverageEntityList();
        if (apiCoverageEntityOptionList.isPresent()) {
            List<ApiCoverageEntity> apiCoverageEntityList = apiCoverageEntityOptionList.get();
            Supplier<Stream<ApiCoverageEntity>> apiCoverageStreamSupplier =
                    () -> apiCoverageEntityList.stream();
            int totalHalApi =
                    apiCoverageStreamSupplier.get().mapToInt(data -> data.getHalApi().size()).sum();
            if (totalHalApi > 0) {
                int coveredHalApi =
                        apiCoverageStreamSupplier
                                .get()
                                .mapToInt(data -> data.getCoveredHalApi().size())
                                .sum();
                JsonArray apiCoverageKeyArray =
                        apiCoverageStreamSupplier
                                .get()
                                .map(data -> new JsonPrimitive(data.getUrlSafeKey()))
                                .collect(JsonArray::new, JsonArray::add, JsonArray::addAll);

                json.add(API_COVERAGE_KEY_LIST, apiCoverageKeyArray);
                json.add(COVERED_API_COUNT, new JsonPrimitive(coveredHalApi));
                json.add(TOTAL_API_COUNT, new JsonPrimitive(totalHalApi));
            }
        }

        List<String> logLinks = this.getLogLinks();
        if (logLinks != null && logLinks.size() > 0) {
            List<JsonElement> links = new ArrayList<>();
            for (String rawUrl : logLinks) {
                LinkDisplay validatedLink = UrlUtil.processUrl(rawUrl);
                if (validatedLink == null) {
                    logger.log(Level.WARNING, "Invalid logging URL : " + rawUrl);
                    continue;
                }
                String[] logInfo = new String[] {validatedLink.name, validatedLink.url};
                links.add(new Gson().toJsonTree(logInfo));
            }
            if (links.size() > 0) {
                json.add(this.LOG_LINKS, new Gson().toJsonTree(links));
            }
        }
        return json;
    }
}
