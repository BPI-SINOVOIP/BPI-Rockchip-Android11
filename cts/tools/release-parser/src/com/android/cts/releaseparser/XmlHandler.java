/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.cts.releaseparser;

import com.android.cts.releaseparser.ReleaseProto.*;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;
import java.util.HashMap;

/** {@link DefaultHandler} */
class XmlHandler extends DefaultHandler {
    // Root Element Tag
    public static final String PERMISSIONS_TAG = "permissions";

    // Element Tag
    public static final String PERMISSION_TAG = "permission";
    public static final String ASSIGN_PERMISSION_TAG = "assign-permission";
    public static final String LIBRARY_TAG = "library";
    public static final String ALLOW_IN_POWER_SAVE_TAG = "allow-in-power-save";
    public static final String SYSTEM_USER_WHITELISTED_TAG = "system-user-whitelisted-app";
    public static final String PRIVAPP_PERMISSIONS_TAG = "privapp-permissions";
    public static final String FEATURE_TAG = "feature";

    // Attribue Tag
    private static final String NAME_TAG = "name";
    private static final String GID_TAG = "gid";
    private static final String UID_TAG = "uid";
    private static final String GROUP_TAG = "group";
    private static final String FILE_TAG = "file";
    private static final String PACKAGE_TAG = "package";
    private static final String VERSION_TAG = "version";

    private HashMap<String, PermissionList> mPermissions;
    private Permission.Builder mPermissionsBuilder;
    private PermissionList.Builder mPermissionListBuilder;
    private PermissionList.Builder mAssignPermissionsListBuilder;
    private PermissionList.Builder mLibraryListBuilder;
    private PermissionList.Builder mAllowInPowerSaveListBuilder;
    private PermissionList.Builder mSystemUserWhitelistedListBuilder;
    private PermissionList.Builder mPrivappPermissionsListBuilder;
    private PermissionList.Builder mFeatureListBuilder;

    XmlHandler(String fileName) {
        mPermissions = new HashMap<String, PermissionList>();
        mPermissionListBuilder = PermissionList.newBuilder();
        mPermissionListBuilder.setName(PERMISSION_TAG);
        mAssignPermissionsListBuilder = PermissionList.newBuilder();
        mAssignPermissionsListBuilder.setName(ASSIGN_PERMISSION_TAG);
        mLibraryListBuilder = PermissionList.newBuilder();
        mLibraryListBuilder.setName(LIBRARY_TAG);
        mAllowInPowerSaveListBuilder = PermissionList.newBuilder();
        mAllowInPowerSaveListBuilder.setName(ALLOW_IN_POWER_SAVE_TAG);
        mSystemUserWhitelistedListBuilder = PermissionList.newBuilder();
        mSystemUserWhitelistedListBuilder.setName(SYSTEM_USER_WHITELISTED_TAG);
        mPrivappPermissionsListBuilder = PermissionList.newBuilder();
        mPrivappPermissionsListBuilder.setName(PRIVAPP_PERMISSIONS_TAG);
        mFeatureListBuilder = PermissionList.newBuilder();
        mFeatureListBuilder.setName(FEATURE_TAG);
    }

    public HashMap<String, PermissionList> getPermissions() {
        return mPermissions;
    }

    @Override
    public void startElement(String uri, String localName, String name, Attributes attributes)
            throws SAXException {
        super.startElement(uri, localName, name, attributes);

        switch (localName) {
            case PERMISSIONS_TAG:
                // start permissions tree
                break;
            case PERMISSION_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(NAME_TAG));
                break;
            case GROUP_TAG:
                if (mPermissionsBuilder != null) {
                    Element.Builder eleBuilder = Element.newBuilder();
                    eleBuilder.setName(GID_TAG);
                    eleBuilder.setValue(attributes.getValue(GID_TAG));
                    mPermissionsBuilder.addElements(eleBuilder.build());
                }
                break;
            case ASSIGN_PERMISSION_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(NAME_TAG));
                String uid = attributes.getValue(UID_TAG);
                if (uid != null) {
                    Element.Builder eleBuilder = Element.newBuilder();
                    eleBuilder.setName(UID_TAG);
                    eleBuilder.setValue(uid);
                    mPermissionsBuilder.addElements(eleBuilder.build());
                }
                break;
            case LIBRARY_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(NAME_TAG));
                String file = attributes.getValue(FILE_TAG);
                if (file != null) {
                    Element.Builder eleBuilder = Element.newBuilder();
                    eleBuilder.setName(FILE_TAG);
                    eleBuilder.setValue(file);
                    mPermissionsBuilder.addElements(eleBuilder.build());
                }
                break;
            case ALLOW_IN_POWER_SAVE_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(PACKAGE_TAG));
                break;
            case SYSTEM_USER_WHITELISTED_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(PACKAGE_TAG));
                break;
            case PRIVAPP_PERMISSIONS_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(PACKAGE_TAG));
                break;
            case FEATURE_TAG:
                mPermissionsBuilder = Permission.newBuilder();
                mPermissionsBuilder.setName(attributes.getValue(NAME_TAG));
                String version = attributes.getValue(VERSION_TAG);
                if (version != null) {
                    Element.Builder eleBuilder = Element.newBuilder();
                    eleBuilder.setName(VERSION_TAG);
                    eleBuilder.setValue(version);
                    mPermissionsBuilder.addElements(eleBuilder.build());
                }
                break;
        }
    }

    @Override
    public void endElement(String uri, String localName, String name) throws SAXException {
        super.endElement(uri, localName, name);
        switch (localName) {
            case PERMISSIONS_TAG:
                if (mPermissionListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(PERMISSION_TAG, mPermissionListBuilder.build());
                }
                if (mAssignPermissionsListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(ASSIGN_PERMISSION_TAG, mAssignPermissionsListBuilder.build());
                }
                if (mLibraryListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(LIBRARY_TAG, mLibraryListBuilder.build());
                }
                if (mAllowInPowerSaveListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(ALLOW_IN_POWER_SAVE_TAG, mAllowInPowerSaveListBuilder.build());
                }
                if (mSystemUserWhitelistedListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(
                            SYSTEM_USER_WHITELISTED_TAG, mSystemUserWhitelistedListBuilder.build());
                }
                if (mPrivappPermissionsListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(
                            PRIVAPP_PERMISSIONS_TAG, mPrivappPermissionsListBuilder.build());
                }
                if (mFeatureListBuilder.getPermissionsList().size() > 0) {
                    mPermissions.put(FEATURE_TAG, mFeatureListBuilder.build());
                }
                break;
            case PERMISSION_TAG:
                mPermissionListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
            case ASSIGN_PERMISSION_TAG:
                mAssignPermissionsListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
            case LIBRARY_TAG:
                mLibraryListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
            case ALLOW_IN_POWER_SAVE_TAG:
                mAllowInPowerSaveListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
            case SYSTEM_USER_WHITELISTED_TAG:
                mSystemUserWhitelistedListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
            case PRIVAPP_PERMISSIONS_TAG:
                mPrivappPermissionsListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
            case FEATURE_TAG:
                mFeatureListBuilder.addPermissions(mPermissionsBuilder.build());
                mPermissionsBuilder = null;
                break;
        }
    }
}
