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

import java.util.List;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Entity Class for CodeCoverageFile */
@Cache
@Entity(name = "CodeCoverageFile")
@EqualsAndHashCode(of = "id")
@NoArgsConstructor
public class CodeCoverageFileEntity implements DashboardEntity {

    /** CodeCoverageFileEntity testName field */
    @Id @Getter @Setter Long id;

    @Parent
    @Getter @Setter private Key<?> coverageParent;

    /** CodeCoverageFileEntity filePath field */
    @Getter @Setter String filePath;

    /** CodeCoverageFileEntity group field */
    @Getter @Setter String group;

    /** CodeCoverageFileEntity lineCoverage field */
    @Getter @Setter List<Long> lineCoverage;

    /** CodeCoverageFileEntity coveredCount field */
    @Getter @Setter long coveredCount;

    /** CodeCoverageFileEntity totalCount field */
    @Getter @Setter long totalCount;

    /** CodeCoverageFileEntity projectName field */
    @Getter @Setter String projectName;

    /** CodeCoverageFileEntity projectVersion field */
    @Getter @Setter String projectVersion;

    /** CodeCoverageFileEntity isIgnored field */
    @Index
    @Getter @Setter Boolean isIgnored;

    /** Constructor function for CodeCoverageFileEntity Class */
    public CodeCoverageFileEntity(
            long id,
            Key<?> coverageParent,
            String filePath,
            String group,
            List<Long> lineCoverage,
            long coveredCount,
            long totalCount,
            String projectName,
            String projectVersion) {
        this.id = id;
        this.coverageParent = coverageParent;
        this.filePath = filePath;
        this.group = group;
        this.lineCoverage = lineCoverage;
        this.coveredCount = coveredCount;
        this.totalCount = totalCount;
        this.projectName = projectName;
        this.projectVersion = projectVersion;
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<CodeCoverageFileEntity> save() {
        this.isIgnored = false;
        return ofy().save().entity(this).now();
    }
}
