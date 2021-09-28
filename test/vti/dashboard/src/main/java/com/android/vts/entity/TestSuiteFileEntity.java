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
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Date;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Entity Class for Suite Test File Info */
@Cache
@Entity
@EqualsAndHashCode(of = "id")
@NoArgsConstructor
public class TestSuiteFileEntity implements DashboardEntity {

    /** Test Suite full file path field */
    @Id @Getter @Setter String filePath;

    /** Test Suite year field in the filePath  */
    @Index @Getter @Setter int year;

    /** Test Suite month field in the filePath  */
    @Index @Getter @Setter int month;

    /** Test Suite day field in the filePath */
    @Index @Getter @Setter int day;

    /** Test Suite file name field */
    @Index @Getter @Setter String fileName;

    /** When this record was created or updated */
    @Index @Getter Date updated;

    /** Construction function for TestSuiteResultEntity Class */
    public TestSuiteFileEntity(String filePath) {
        this.filePath = filePath;
        Path pathInfo = Paths.get(filePath);
        if (pathInfo.getNameCount() > 3) {
            this.year = Integer.valueOf(pathInfo.getName(1).toString());
            this.month = Integer.valueOf(pathInfo.getName(2).toString());
            this.day = Integer.valueOf(pathInfo.getName(3).toString());
            this.fileName = pathInfo.getFileName().toString();
        }
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<TestSuiteFileEntity> save() {
        this.updated = new Date();
        return ofy().save().entity(this).now();
    }
}
