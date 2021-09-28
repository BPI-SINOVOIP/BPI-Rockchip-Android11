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

import com.android.vts.proto.VtsReportMessage.AndroidDeviceInfoMessage;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.memcache.MemcacheService;
import com.google.appengine.api.memcache.MemcacheServiceFactory;
import com.google.apphosting.api.ApiProxy;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Ignore;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Parent;
import java.util.List;
import java.util.Objects;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import lombok.Data;
import lombok.NoArgsConstructor;

import static com.googlecode.objectify.ObjectifyService.ofy;

@com.googlecode.objectify.annotation.Entity(name = "DeviceInfo")
@Cache
@Data
@NoArgsConstructor
/** Class describing a device used for a test run. */
public class DeviceInfoEntity implements DashboardEntity {
    protected static final Logger logger = Logger.getLogger(DeviceInfoEntity.class.getName());

    /** This is the instance of App Engine memcache service java library */
    private static MemcacheService syncCache = MemcacheServiceFactory.getMemcacheService();

    public static final String KIND = "DeviceInfo";

    // Property keys
    public static final String BRANCH = "branch";
    public static final String PRODUCT = "product";
    public static final String BUILD_FLAVOR = "buildFlavor";
    public static final String BUILD_ID = "buildId";
    public static final String ABI_BITNESS = "abiBitness";
    public static final String ABI_NAME = "abiName";

    @Ignore
    private Key parentKey;

    /** ID field using start timestamp */
    @Id private Long id;

    /** parent field based on Test and TestRun key */
    @Parent
    private com.googlecode.objectify.Key<?> parent;

    @Index
    private String branch;

    @Index
    private String product;

    @Index
    private String buildFlavor;

    @Index
    private String buildId;

    private String abiBitness;

    private String abiName;

    /**
     * Create a DeviceInfoEntity object.
     *
     * @param parentKey The key for the parent entity in the database.
     * @param branch The build branch.
     * @param product The device product.
     * @param buildFlavor The device build flavor.
     * @param buildID The device build ID.
     * @param abiBitness The abi bitness of the device.
     * @param abiName The name of the abi.
     */
    public DeviceInfoEntity(
            Key parentKey,
            String branch,
            String product,
            String buildFlavor,
            String buildID,
            String abiBitness,
            String abiName) {
        this.parentKey = parentKey;
        this.branch = branch;
        this.product = product;
        this.buildFlavor = buildFlavor;
        this.buildId = buildID;
        this.abiBitness = abiBitness;
        this.abiName = abiName;
    }

    /**
     * Create a DeviceInfoEntity object with objectify Key
     *
     * @param parent The objectify key for the parent entity in the database.
     * @param branch The build branch.
     * @param product The device product.
     * @param buildFlavor The device build flavor.
     * @param buildID The device build ID.
     * @param abiBitness The abi bitness of the device.
     * @param abiName The name of the abi.
     */
    public DeviceInfoEntity(
            com.googlecode.objectify.Key parent,
            String branch,
            String product,
            String buildFlavor,
            String buildID,
            String abiBitness,
            String abiName) {
        this.parent = parent;
        this.branch = branch;
        this.product = product;
        this.buildFlavor = buildFlavor;
        this.buildId = buildID;
        this.abiBitness = abiBitness;
        this.abiName = abiName;
    }

    /**
     * Get All Branch List from DeviceInfoEntity
     */
    public static List<String> getAllBranches() {
        try {
            List<String> branchList = (List<String>) syncCache.get("branchList");
            if (Objects.isNull(branchList)) {
                branchList =
                        ofy().load()
                                .type(DeviceInfoEntity.class)
                                .project("branch")
                                .distinct(true)
                                .list()
                                .stream()
                                .map(device -> device.branch)
                                .collect(Collectors.toList());
                syncCache.put("branchList", branchList);
            }
            return branchList;
        } catch (ApiProxy.CallNotFoundException e) {
            return ofy().load()
                    .type(DeviceInfoEntity.class)
                    .project("branch")
                    .distinct(true)
                    .list()
                    .stream()
                    .map(device -> device.branch)
                    .collect(Collectors.toList());
        }
    }

    /**
     * Get All BuildFlavors List from DeviceInfoEntity
     */
    public static List<String> getAllBuildFlavors() {
        try {
            List<String> buildFlavorList = (List<String>) syncCache.get("buildFlavorList");
            if (Objects.isNull(buildFlavorList)) {
                buildFlavorList =
                        ofy().load()
                                .type(DeviceInfoEntity.class)
                                .project("buildFlavor")
                                .distinct(true)
                                .list()
                                .stream()
                                .map(device -> device.buildFlavor)
                                .collect(Collectors.toList());
                syncCache.put("buildFlavorList", buildFlavorList);
            }
            return buildFlavorList;
        } catch (ApiProxy.CallNotFoundException e) {
            return ofy().load()
                    .type(DeviceInfoEntity.class)
                    .project("buildFlavor")
                    .distinct(true)
                    .list()
                    .stream()
                    .map(device -> device.buildFlavor)
                    .collect(Collectors.toList());
        }
    }

    /** Saving function for the instance of this class */
    public com.googlecode.objectify.Key<DeviceInfoEntity> save() {
        return ofy().save().entity(this).now();
    }

    public Entity toEntity() {
        Entity deviceEntity = new Entity(KIND, this.parentKey);
        deviceEntity.setProperty(BRANCH, this.branch.toLowerCase());
        deviceEntity.setProperty(PRODUCT, this.product.toLowerCase());
        deviceEntity.setProperty(BUILD_FLAVOR, this.buildFlavor.toLowerCase());
        deviceEntity.setProperty(BUILD_ID, this.buildId.toLowerCase());
        if (this.abiBitness != null && this.abiName != null) {
            deviceEntity.setUnindexedProperty(ABI_BITNESS, this.abiBitness.toLowerCase());
            deviceEntity.setUnindexedProperty(ABI_NAME, this.abiName.toLowerCase());
        }

        return deviceEntity;
    }

    /**
     * Convert an Entity object to a DeviceInfoEntity.
     *
     * @param e The entity to process.
     * @return DeviceInfoEntity object with the properties from e, or null if incompatible.
     */
    public static DeviceInfoEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND) || !e.hasProperty(BRANCH) || !e.hasProperty(PRODUCT)
                || !e.hasProperty(BUILD_FLAVOR) || !e.hasProperty(BUILD_ID)
                || !e.hasProperty(ABI_BITNESS) || !e.hasProperty(ABI_NAME)) {
            logger.log(Level.WARNING, "Missing device info attributes in entity: " + e.toString());
            return null;
        }
        try {
            Key parentKey = e.getKey().getParent();
            String branch = (String) e.getProperty(BRANCH);
            String product = (String) e.getProperty(PRODUCT);
            String buildFlavor = (String) e.getProperty(BUILD_FLAVOR);
            String buildId = (String) e.getProperty(BUILD_ID);
            String abiBitness = null;
            String abiName = null;
            if (e.hasProperty(ABI_BITNESS) && e.hasProperty(ABI_NAME)) {
                abiBitness = (String) e.getProperty(ABI_BITNESS);
                abiName = (String) e.getProperty(ABI_NAME);
            }
            return new DeviceInfoEntity(
                    parentKey, branch, product, buildFlavor, buildId, abiBitness, abiName);
        } catch (ClassCastException exception) {
            // Invalid cast
            logger.log(Level.WARNING, "Error parsing device info entity.", exception);
        }
        return null;
    }

    /**
     * Convert a device info message to a DeviceInfoEntity.
     *
     * @param parent The ancestor key for the device entity.
     * @param device The device info report describing the target Android device.
     * @return The DeviceInfoEntity for the target device, or null if incompatible
     */
    public static DeviceInfoEntity fromDeviceInfoMessage(
            com.googlecode.objectify.Key parent, AndroidDeviceInfoMessage device) {
        if (!device.hasBuildAlias() || !device.hasBuildFlavor() || !device.hasProductVariant()
                || !device.hasBuildId()) {
            return null;
        }
        String branch = device.getBuildAlias().toStringUtf8();
        String buildFlavor = device.getBuildFlavor().toStringUtf8();
        String product = device.getProductVariant().toStringUtf8();
        String buildId = device.getBuildId().toStringUtf8();
        String abiBitness = device.getAbiBitness().toStringUtf8();
        String abiName = device.getAbiName().toStringUtf8();
        return new DeviceInfoEntity(
                parent, branch, product, buildFlavor, buildId, abiBitness, abiName);
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof DeviceInfoEntity)) {
            return false;
        }
        DeviceInfoEntity device2 = (DeviceInfoEntity) obj;
        if (!this.branch.equals(device2.branch) || !this.product.equals(device2.product)
                || !this.buildFlavor.equals(device2.buildFlavor)
                || !this.buildId.equals(device2.buildId)) {
            return false;
        }
        return true;
    }

    @Override
    public int hashCode() {
        String deviceId = this.branch + this.product + this.buildFlavor + this.buildId;
        return deviceId.hashCode();
    }

    /**
     * Create a copy of the device info under a near parent.
     *
     * @param parentKey The new parent key.
     * @return A copy of the DeviceInfoEntity with the specified parent.
     */
    public DeviceInfoEntity copyWithParent(com.googlecode.objectify.Key parentKey) {
        return new DeviceInfoEntity(parentKey, this.branch, this.product, this.buildFlavor,
                this.buildId, this.abiBitness, this.abiName);
    }

    /**
     * Create a string representation of the device build information.
     * @return A String fingerprint of the format: branch/buildFlavor (build ID)
     */
    public String getFingerprint() {
        return this.branch + "/" + this.buildFlavor + " (" + this.buildId + ")";
    }
}
