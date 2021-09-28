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

/** Entity Class for ApiCoverageEntity */
@Cache
@Entity(name = "ApiCoverage")
@EqualsAndHashCode(of = "id")
@NoArgsConstructor
@JsonAutoDetect(fieldVisibility = Visibility.ANY)
@JsonIgnoreProperties({"id", "parent"})
public class ApiCoverageEntity implements DashboardEntity {

    /** ApiCoverageEntity id field */
    @Id @Getter @Setter String id;

    @Parent @Getter Key<?> parent;

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

    /** Constructor function for ApiCoverageEntity Class */
    public ApiCoverageEntity(
            com.google.appengine.api.datastore.Key testRunKey,
            String halPackageName,
            int halVersionMajor,
            int halVersionMinor,
            String halInterfaceName,
            List<String> halApi,
            List<String> coveredHalApi) {
        this.id = UUID.randomUUID().toString();
        this.parent = getParentKey(testRunKey);

        this.halPackageName = halPackageName;
        this.halMajorVersion = halVersionMajor;
        this.halMinorVersion = halVersionMinor;
        this.halInterfaceName = halInterfaceName;
        this.halApi = halApi;
        this.coveredHalApi = coveredHalApi;
        this.updated = new Date();
    }

    /** Constructor function for ApiCoverageEntity Class with objectify Key. */
    public ApiCoverageEntity(
            Key testRunKey,
            String halPackageName,
            int halVersionMajor,
            int halVersionMinor,
            String halInterfaceName,
            List<String> halApi,
            List<String> coveredHalApi) {
        this.id = UUID.randomUUID().toString();
        this.parent = testRunKey;

        this.halPackageName = halPackageName;
        this.halMajorVersion = halVersionMajor;
        this.halMinorVersion = halVersionMinor;
        this.halInterfaceName = halInterfaceName;
        this.halApi = halApi;
        this.coveredHalApi = coveredHalApi;
        this.updated = new Date();
    }

    /** Get objectify Key from datastore Key type */
    private Key getParentKey(com.google.appengine.api.datastore.Key testRunKey) {
        Key testParentKey = Key.create(TestEntity.class, testRunKey.getParent().getName());
        return Key.create(testParentKey, TestRunEntity.class, testRunKey.getId());
    }

    /** Get UrlSafeKey from ApiCoverageEntity Information */
    public String getUrlSafeKey() {
        Key uuidKey = Key.create(this.parent, ApiCoverageEntity.class, this.id);
        return uuidKey.toUrlSafe();
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<ApiCoverageEntity> save() {
        this.id = UUID.randomUUID().toString();
        this.updated = new Date();
        return ofy().save().entity(this).now();
    }

    /** Get List of ApiCoverageEntity by HAL interface name */
    public static ApiCoverageEntity getByUrlSafeKey(String urlSafeKey) {
        return ofy().load()
                .type(ApiCoverageEntity.class)
                .filterKey(com.google.cloud.datastore.Key.fromUrlSafe(urlSafeKey))
                .first()
                .now();
    }

    /** Get List of ApiCoverageEntity by HAL interface name */
    public static List<ApiCoverageEntity> getByInterfaceNameList(String halInterfaceName) {
        return ofy().load()
                .type(ApiCoverageEntity.class)
                .filter("halInterfaceName", halInterfaceName)
                .list();
    }

    /** Get List of ApiCoverageEntity by HAL package name */
    public static List<ApiCoverageEntity> getByPackageNameList(String packageName) {
        return ofy().load()
                .type(ApiCoverageEntity.class)
                .filter("halPackageName", packageName)
                .list();
    }
}
