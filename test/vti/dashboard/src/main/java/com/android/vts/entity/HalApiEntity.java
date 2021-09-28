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

import static com.googlecode.objectify.ObjectifyService.ofy;

import com.fasterxml.jackson.annotation.JsonAutoDetect;
import com.fasterxml.jackson.annotation.JsonAutoDetect.Visibility;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.googlecode.objectify.Key;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Entity;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Parent;
import java.util.Date;
import java.util.List;
import java.util.UUID;
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;

/** Entity Class for HalApiEntity */
@Cache
@Entity(name = "HalApiEntity")
@EqualsAndHashCode(of = "id")
@NoArgsConstructor
@JsonAutoDetect(fieldVisibility = Visibility.ANY)
@JsonIgnoreProperties({"id", "parent"})
public class HalApiEntity implements DashboardEntity {

    /** HalApiEntity id field */
    @Id @Getter @Setter String id;

    @Parent @Getter Key<?> parent;

    /** HAL Api Release Level. e.g. */
    @Index @Getter @Setter String halApiReleaseLevel;

    /** HAL package name. e.g. android.hardware.foo. */
    @Index @Getter @Setter String halPackageName;

    /** HAL (major) version. e.g. 1. */
    @Index @Getter @Setter int halMajorVersion;

    /** HAL (minor) version. e.g. 0. */
    @Index @Getter @Setter int halMinorVersion;

    /** HAL interface name. e.g. IFoo. */
    @Index @Getter @Setter String halInterfaceName;

    /** List of HAL API */
    @Getter @Setter List<String> halApi;

    /** List of HAL covered API */
    @Getter @Setter List<String> coveredHalApi;

    /** When this record was created or updated */
    @Index Date updated;

    /** Constructor function for HalApiEntity Class */
    public HalApiEntity(
            com.googlecode.objectify.Key testRunKey,
            String halApiReleaseLevel,
            String halPackageName,
            int halMajorVersion,
            int halMinorVersion,
            String halInterfaceName,
            List<String> halApi,
            List<String> coveredHalApi) {

        this.id = UUID.randomUUID().toString();
        this.parent = testRunKey;

        this.halApiReleaseLevel = halApiReleaseLevel;
        this.halPackageName = halPackageName;
        this.halMajorVersion = halMajorVersion;
        this.halMinorVersion = halMinorVersion;
        this.halInterfaceName = halInterfaceName;
        this.halApi = halApi;
        this.coveredHalApi = coveredHalApi;
        this.updated = new Date();
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<HalApiEntity> save() {
        return ofy().save().entity(this).now();
    }
}
