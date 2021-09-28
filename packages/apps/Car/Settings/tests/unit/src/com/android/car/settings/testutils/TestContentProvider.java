/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.settings.testutils;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import java.util.List;

/**
 * A simple content provider for tests.  This provider runs in the same process as the test.
 */
public class TestContentProvider extends ContentProvider {

    public static final String TEST_TEXT_CONTENT = "TestContentProviderText";

    private static final String SCHEME = "content";
    private static final String AUTHORITY =
            "com.android.car.settings.testutils.TestContentProvider";
    private static final String METHOD_GET_TEXT = "getText";
    private static final String METHOD_GET_ICON = "getIcon";
    private static final String KEY_PREFERENCE_TITLE = "com.android.settings.title";
    private static final String KEY_PREFERENCE_SUMMARY = "com.android.settings.summary";
    private static final String KEY_PREFERENCE_ICON = "com.android.settings.icon";
    private static final String KEY_PREFERENCE_ICON_PACKAGE = "com.android.settings.icon_package";
    private static final String TEXT_KEY = "textKey";
    private static final String ICON_KEY = "iconKey";


    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public Bundle call(String methodName, String uriString, Bundle extras) {
        if (TextUtils.isEmpty(methodName)) {
            return null;
        }
        if (TextUtils.isEmpty(uriString)) {
            return null;
        }
        Uri uri = Uri.parse(uriString);
        if (uri == null) {
            return null;
        }
        // Only process URIs with valid scheme, authority, and no assigned port.
        if (!(SCHEME.equals(uri.getScheme())
                && AUTHORITY.equals(uri.getAuthority())
                && (uri.getPort() == -1))) {
            return null;
        }
        List<String> pathSegments = uri.getPathSegments();
        // Path segments should consist of the method name and the argument. If the number of path
        // segments is not exactly two, the content provider is not being called as intended.
        if ((pathSegments == null) || (pathSegments.size() != 2)) {
            return null;
        }
        // The first path segment needs to match the methodName.
        if (!methodName.equals(pathSegments.get(0))) {
            return null;
        }
        String key = pathSegments.get(1);
        if (METHOD_GET_TEXT.equals(methodName)) {
            if (TEXT_KEY.equals(key)) {
                Bundle bundle = new Bundle();
                bundle.putString(KEY_PREFERENCE_TITLE, TEST_TEXT_CONTENT);
                bundle.putString(KEY_PREFERENCE_SUMMARY, TEST_TEXT_CONTENT);
                return bundle;
            }
        } else if (METHOD_GET_ICON.equals(methodName)) {
            if (ICON_KEY.equals(key)) {
                try {
                    String packageName = getContext().getPackageName();
                    PackageManager manager = getContext().getPackageManager();
                    Resources resources = manager.getResourcesForApplication(packageName);
                    int iconRes = resources.getIdentifier("test_icon", "drawable",
                            packageName);
                    if (iconRes == 0) {
                        return null;
                    }

                    Bundle bundle = new Bundle();
                    bundle.putInt(KEY_PREFERENCE_ICON, iconRes);
                    bundle.putString(KEY_PREFERENCE_ICON_PACKAGE, packageName);
                    return bundle;

                } catch (PackageManager.NameNotFoundException e) {
                    return null;
                }
            }
        }
        return null;
    }
}
