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

import com.android.vts.proto.VtsReportMessage.CoverageReportMessage;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Ignore;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Parent;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;
import lombok.Data;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

@com.googlecode.objectify.annotation.Entity(name = "Coverage")
@Cache
@Data
@NoArgsConstructor
/** Object describing coverage data gathered for a file. */
public class CoverageEntity implements DashboardEntity {

    protected static final Logger logger = Logger.getLogger(CoverageEntity.class.getName());

    public static final String KIND = "Coverage";

    public static String GERRIT_URI;

    // Property keys
    public static final String GROUP = "group";
    public static final String COVERED_LINE_COUNT = "coveredCount";
    public static final String TOTAL_LINE_COUNT = "totalCount";
    public static final String FILE_PATH = "filePath";
    public static final String PROJECT_NAME = "projectName";
    public static final String PROJECT_VERSION = "projectVersion";
    public static final String LINE_COVERAGE = "lineCoverage";

    @Ignore @Getter @Setter private Key parentKey;

    @Id @Getter @Setter private Long id;

    @Parent @Getter @Setter private com.googlecode.objectify.Key<?> testParent;

    @Index @Getter @Setter private String group;

    @Getter @Setter private long coveredCount;

    @Getter @Setter private long totalCount;

    @Index @Getter @Setter private String filePath;

    @Getter @Setter private String projectName;

    @Getter @Setter private String projectVersion;

    @Getter @Setter private List<Long> lineCoverage;

    /** CoverageEntity isIgnored field */
    @Index @Getter @Setter Boolean isIgnored;

    /**
     * Create a CoverageEntity object for a file.
     *
     * @param parentKey The key to the parent TestRunEntity object in the database.
     * @param group The group within the test run describing the coverage.
     * @param coveredLineCount The total number of covered lines in the file.
     * @param totalLineCount The total number of uncovered executable lines in the file.
     * @param filePath The path to the file.
     * @param projectName The name of the git project.
     * @param projectVersion The commit hash of the project at the time the test was executed.
     * @param lineCoverage List of coverage counts per executable line in the file.
     */
    public CoverageEntity(
            Key parentKey,
            String group,
            long coveredLineCount,
            long totalLineCount,
            String filePath,
            String projectName,
            String projectVersion,
            List<Long> lineCoverage) {
        this.parentKey = parentKey;
        this.group = group;
        this.coveredCount = coveredLineCount;
        this.totalCount = totalLineCount;
        this.filePath = filePath;
        this.projectName = projectName;
        this.projectVersion = projectVersion;
        this.lineCoverage = lineCoverage;
    }

    /**
     * Create a CoverageEntity object for a file.
     *
     * @param testParent The objectify key to the parent TestRunEntity object in the database.
     * @param group The group within the test run describing the coverage.
     * @param coveredLineCount The total number of covered lines in the file.
     * @param totalLineCount The total number of uncovered executable lines in the file.
     * @param filePath The path to the file.
     * @param projectName The name of the git project.
     * @param projectVersion The commit hash of the project at the time the test was executed.
     * @param lineCoverage List of coverage counts per executable line in the file.
     */
    public CoverageEntity(
            com.googlecode.objectify.Key testParent,
            String group,
            long coveredLineCount,
            long totalLineCount,
            String filePath,
            String projectName,
            String projectVersion,
            List<Long> lineCoverage) {
        this.testParent = testParent;
        this.group = group;
        this.coveredCount = coveredLineCount;
        this.totalCount = totalLineCount;
        this.filePath = filePath;
        this.projectName = projectName;
        this.projectVersion = projectVersion;
        this.lineCoverage = lineCoverage;
    }

    /** find coverage entity by ID */
    public static CoverageEntity findById(String testName, String testRunId, String id) {
        com.googlecode.objectify.Key testKey =
                com.googlecode.objectify.Key.create(TestEntity.class, testName);
        com.googlecode.objectify.Key testRunKey =
                com.googlecode.objectify.Key.create(
                        testKey, TestRunEntity.class, Long.parseLong(testRunId));
        return ofy().load()
                .type(CoverageEntity.class)
                .parent(testRunKey)
                .id(Long.parseLong(id))
                .now();
    }

    public static void setPropertyValues(Properties newSystemConfigProp) {
        GERRIT_URI = newSystemConfigProp.getProperty("gerrit.uri");
    }

    /** Saving function for the instance of this class */
    @Override
    public com.googlecode.objectify.Key<CoverageEntity> save() {
        return ofy().save().entity(this).now();
    }

    /** Get percentage from calculating coveredCount and totalCount values */
    public Double getPercentage() {
        return Math.round(coveredCount * 10000d / totalCount) / 100d;
    }

    /** Get Gerrit Url function from the attributes of this class */
    public String getGerritUrl() throws UnsupportedEncodingException {
        String gerritPath =
                GERRIT_URI
                        + "/projects/"
                        + URLEncoder.encode(projectName, "UTF-8")
                        + "/commits/"
                        + URLEncoder.encode(projectVersion, "UTF-8")
                        + "/files/"
                        + URLEncoder.encode(filePath, "UTF-8")
                        + "/content";
        return gerritPath;
    }

    /* Comparator for sorting the list by isIgnored field */
    public static Comparator<CoverageEntity> isIgnoredComparator =
            new Comparator<CoverageEntity>() {

                public int compare(CoverageEntity coverageEntity1, CoverageEntity coverageEntity2) {
                    Boolean isIgnored1 =
                            Objects.isNull(coverageEntity1.getIsIgnored())
                                    ? false
                                    : coverageEntity1.getIsIgnored();
                    Boolean isIgnored2 =
                            Objects.isNull(coverageEntity2.getIsIgnored())
                                    ? false
                                    : coverageEntity2.getIsIgnored();

                    // ascending order
                    return isIgnored1.compareTo(isIgnored2);
                }
            };

    public Entity toEntity() {
        Entity coverageEntity = new Entity(KIND, parentKey);
        coverageEntity.setProperty(GROUP, group);
        coverageEntity.setUnindexedProperty(COVERED_LINE_COUNT, coveredCount);
        coverageEntity.setUnindexedProperty(TOTAL_LINE_COUNT, totalCount);
        coverageEntity.setProperty(FILE_PATH, filePath);
        coverageEntity.setUnindexedProperty(PROJECT_NAME, projectName);
        coverageEntity.setUnindexedProperty(PROJECT_VERSION, projectVersion);
        if (lineCoverage != null && lineCoverage.size() > 0) {
            coverageEntity.setUnindexedProperty(LINE_COVERAGE, lineCoverage);
        }
        return coverageEntity;
    }

    /**
     * Convert an Entity object to a CoverageEntity.
     *
     * @param e The entity to process.
     * @return CoverageEntity object with the properties from e, or null if incompatible.
     */
    @SuppressWarnings("unchecked")
    public static CoverageEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND)
                || !e.hasProperty(GROUP)
                || !e.hasProperty(COVERED_LINE_COUNT)
                || !e.hasProperty(TOTAL_LINE_COUNT)
                || !e.hasProperty(FILE_PATH)
                || !e.hasProperty(PROJECT_NAME)
                || !e.hasProperty(PROJECT_VERSION)) {
            logger.log(Level.WARNING, "Missing coverage attributes in entity: " + e.toString());
            return null;
        }
        try {
            String group = (String) e.getProperty(GROUP);
            long coveredLineCount = (long) e.getProperty(COVERED_LINE_COUNT);
            long totalLineCount = (long) e.getProperty(TOTAL_LINE_COUNT);
            String filePath = (String) e.getProperty(FILE_PATH);
            String projectName = (String) e.getProperty(PROJECT_NAME);
            String projectVersion = (String) e.getProperty(PROJECT_VERSION);
            List<Long> lineCoverage;
            if (e.hasProperty(LINE_COVERAGE)) {
                lineCoverage = (List<Long>) e.getProperty(LINE_COVERAGE);
            } else {
                lineCoverage = new ArrayList<>();
            }
            return new CoverageEntity(
                    e.getKey().getParent(),
                    group,
                    coveredLineCount,
                    totalLineCount,
                    filePath,
                    projectName,
                    projectVersion,
                    lineCoverage);
        } catch (ClassCastException exception) {
            // Invalid contents or null values
            logger.log(Level.WARNING, "Error parsing coverage entity.", exception);
        }
        return null;
    }

    /**
     * Convert a coverage report to a CoverageEntity.
     *
     * @param parentKey The ancestor key for the coverage entity.
     * @param group The group to display the coverage report with.
     * @param coverage The coverage report containing coverage data.
     * @return The CoverageEntity for the coverage report message, or null if not compatible.
     */
    public static CoverageEntity fromCoverageReport(
            com.googlecode.objectify.Key parentKey, String group, CoverageReportMessage coverage) {
        if (!coverage.hasFilePath()
                || !coverage.hasProjectName()
                || !coverage.hasRevision()
                || !coverage.hasTotalLineCount()
                || !coverage.hasCoveredLineCount()) {
            return null; // invalid coverage report;
        }
        long coveredLineCount = coverage.getCoveredLineCount();
        long totalLineCount = coverage.getTotalLineCount();
        String filePath = coverage.getFilePath().toStringUtf8();
        String projectName = coverage.getProjectName().toStringUtf8();
        String projectVersion = coverage.getRevision().toStringUtf8();
        List<Long> lineCoverage = null;
        if (coverage.getLineCoverageVectorCount() > 0) {
            lineCoverage = new ArrayList<>();
            for (long count : coverage.getLineCoverageVectorList()) {
                lineCoverage.add(count);
            }
        }
        return new CoverageEntity(
                parentKey,
                group,
                coveredLineCount,
                totalLineCount,
                filePath,
                projectName,
                projectVersion,
                lineCoverage);
    }
}
