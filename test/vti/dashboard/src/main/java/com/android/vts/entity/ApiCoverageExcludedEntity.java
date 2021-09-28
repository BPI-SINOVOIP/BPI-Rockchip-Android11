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
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.googlecode.objectify.Key;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Entity;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Index;
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

import java.util.Date;
import java.util.List;

import static com.googlecode.objectify.ObjectifyService.ofy;

/**
 * This entity class contain the excluded API information. And this information will be used to
 * calculate more precise the ratio of API coverage.
 */
@Cache
@Entity(name = "ApiCoverageExcluded")
@EqualsAndHashCode(of = "id")
@NoArgsConstructor
@JsonAutoDetect(fieldVisibility = JsonAutoDetect.Visibility.ANY)
@JsonIgnoreProperties({"id", "parent"})
public class ApiCoverageExcludedEntity implements DashboardEntity {

    /** ApiCoverageEntity id field */
    @Id @Getter @Setter private String id;

    /** Package name. e.g. android.hardware.foo. */
    @Index @Getter @Setter private String packageName;

    /** Major Version. e.g. 1, 2. */
    @Getter @Setter private int majorVersion;

    /** Minor Version. e.g. 0. */
    @Getter @Setter private int minorVersion;

    /** Interface name. e.g. IFoo. */
    @Index @Getter @Setter private String interfaceName;

    /** API Name */
    @Index @Getter @Setter private String apiName;

    /** The reason comment for the excluded API */
    @Getter @Setter private String comment;

    /** When this record was created or updated */
    @Index @Getter @Setter private Date updated;

    /** Constructor function for ApiCoverageExcludedEntity Class */
    public ApiCoverageExcludedEntity(
            String packageName,
            String version,
            String interfaceName,
            String apiName,
            String comment) {
        this.id = this.getObjectifyId();
        this.packageName = packageName;
        this.interfaceName = interfaceName;
        this.apiName = apiName;
        this.comment = comment;
        this.updated = new Date();

        this.setVersions(version);
    }

    /** Setting major and minor version from version string */
    private void setVersions(String version) {
        String[] versionArray = version.split("[.]");
        if (versionArray.length == 0) {
            this.majorVersion = 0;
            this.minorVersion = 0;
        } else if (versionArray.length == 1) {
            this.majorVersion = Integer.parseInt(versionArray[0]);
            this.minorVersion = 0;
        } else {
            this.majorVersion = Integer.parseInt(versionArray[0]);
            this.minorVersion = Integer.parseInt(versionArray[1]);
        }
    }

    /** Getting objectify ID from the entity information */
    private String getObjectifyId() {
        return this.packageName
                + "."
                + this.majorVersion
                + "."
                + this.minorVersion
                + "."
                + this.interfaceName
                + "."
                + this.apiName;
    }

    /** Getting key from the entity */
    public Key<ApiCoverageExcludedEntity> getKey() {
        return Key.create(ApiCoverageExcludedEntity.class, this.getObjectifyId());
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<ApiCoverageExcludedEntity> save() {
        return ofy().save().entity(this).now();
    }

    /** Get All Key List of ApiCoverageExcludedEntity */
    public static List<Key<ApiCoverageExcludedEntity>> getAllKeyList() {
        return ofy().load().type(ApiCoverageExcludedEntity.class).keys().list();
    }

}
