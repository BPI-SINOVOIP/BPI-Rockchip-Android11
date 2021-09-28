/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.task.interactacrossprofiles;


import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.UserManager;
import android.util.Xml;

import com.android.internal.util.FastXmlSerializer;
import com.android.managedprovisioning.common.ProvisionLogger;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlSerializer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/**
 * Stores and retrieves the cross-profile apps whitelist during provisioning and on
 * subsequent OTAs.
 */
public class CrossProfileAppsSnapshot {
    private static final String TAG_CROSS_PROFILE_APPS = "cross-profile-apps";
    private static final String TAG_PACKAGE_LIST_ITEM = "item";
    private static final String ATTR_VALUE = "value";
    private static final String FOLDER_NAME = "cross_profile_apps";

    private final Context mContext;

    public CrossProfileAppsSnapshot(Context context) {
        if (context == null) {
            throw new NullPointerException();
        }
        mContext = context;
    }

    /**
     * Returns whether currently a snapshot exists for the given user.
     *
     * @param userId the user id for which the snapshot is requested.
     */
    public boolean hasSnapshot(int userId) {
        return getCrossProfileAppsFile(mContext, userId).exists();
    }

    /**
     * Returns the last stored snapshot for the given user.
     *
     * @param userId the user id for which the snapshot is requested.
     */
    public Set<String> getSnapshot(int userId) {
        return readCrossProfileApps(getCrossProfileAppsFile(mContext, userId));
    }

    /**
     * Call this method to take a snapshot of the current cross profile apps whitelist.
     *
     * @param userId the user id for which the snapshot should be taken.
     */
    public void takeNewSnapshot(int userId) {
        final File systemAppsFile = getCrossProfileAppsFile(mContext, userId);
        systemAppsFile.getParentFile().mkdirs(); // Creating the folder if it does not exist
        writeCrossProfileApps(getCurrentCrossProfileAppsWhitelist(), systemAppsFile);
    }

    private Set<String> getCurrentCrossProfileAppsWhitelist() {
        DevicePolicyManager devicePolicyManager =
                mContext.getSystemService(DevicePolicyManager.class);
        return devicePolicyManager.getDefaultCrossProfilePackages();
    }

    private void writeCrossProfileApps(Set<String> packageNames, File crossProfileAppsFile) {
        try {
            FileOutputStream stream = new FileOutputStream(crossProfileAppsFile, false);
            XmlSerializer serializer = new FastXmlSerializer();
            serializer.setOutput(stream, "utf-8");
            serializer.startDocument(null, true);
            serializer.startTag(null, TAG_CROSS_PROFILE_APPS);
            for (String packageName : packageNames) {
                serializer.startTag(null, TAG_PACKAGE_LIST_ITEM);
                serializer.attribute(null, ATTR_VALUE, packageName);
                serializer.endTag(null, TAG_PACKAGE_LIST_ITEM);
            }
            serializer.endTag(null, TAG_CROSS_PROFILE_APPS);
            serializer.endDocument();
            stream.close();
        } catch (IOException e) {
            ProvisionLogger.loge("IOException trying to write the cross profile apps", e);
        }
    }

    private Set<String> readCrossProfileApps(File systemAppsFile) {
        Set<String> result = new HashSet<>();
        if (!systemAppsFile.exists()) {
            return result;
        }
        try {
            FileInputStream stream = new FileInputStream(systemAppsFile);

            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(stream, null);
            parser.next();

            int type;
            int outerDepth = parser.getDepth();
            while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                    && (type != XmlPullParser.END_TAG || parser.getDepth() > outerDepth)) {
                if (type == XmlPullParser.END_TAG || type == XmlPullParser.TEXT) {
                    continue;
                }
                String tag = parser.getName();
                if (tag.equals(TAG_PACKAGE_LIST_ITEM)) {
                    result.add(parser.getAttributeValue(null, ATTR_VALUE));
                } else {
                    ProvisionLogger.loge("Unknown tag: " + tag);
                }
            }
            stream.close();
        } catch (IOException e) {
            ProvisionLogger.loge("IOException trying to read the cross profile apps", e);
        } catch (XmlPullParserException e) {
            ProvisionLogger.loge("XmlPullParserException trying to read the cross profile apps", e);
        }
        return result;
    }

    private static File getCrossProfileAppsFile(Context context, int userId) {
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        int userSerialNumber = userManager.getUserSerialNumber(userId);
        if (userSerialNumber == -1 ) {
            throw new IllegalArgumentException("Invalid userId : " + userId);
        }
        return new File(getFolder(context), userSerialNumber + ".xml");
    }

    private static File getFolder(Context context) {
        return new File(context.getFilesDir(), FOLDER_NAME);
    }
}
