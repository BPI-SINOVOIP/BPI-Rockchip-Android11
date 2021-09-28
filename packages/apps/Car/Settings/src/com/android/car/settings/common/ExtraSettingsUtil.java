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

package com.android.car.settings.common;

import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON_TINTABLE;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_KEYHINT;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import java.util.List;

/** Contains utility functions for injected settings. */
public class ExtraSettingsUtil {
    private static final Logger LOG = new Logger(ExtraSettingsUtil.class);

    /**
     * Returns whether or not an icon is tintable given the injected setting metadata.
     */
    public static boolean isIconTintable(Bundle metaData) {
        if (metaData.containsKey(META_DATA_PREFERENCE_ICON_TINTABLE)) {
            return metaData.getBoolean(META_DATA_PREFERENCE_ICON_TINTABLE);
        }
        return false;
    }

    /**
     * Returns a drawable from an resource's package context.
     */
    public static Drawable loadDrawableFromPackage(Context context, String resPackage, int resId) {
        try {
            return context.createPackageContext(resPackage, /* flags= */ 0)
                    .getDrawable(resId);
        } catch (PackageManager.NameNotFoundException ex) {
            LOG.e("loadDrawableFromPackage: package name not found: ", ex);
        } catch (Resources.NotFoundException ex) {
            LOG.w("loadDrawableFromPackage: resource not found with id " + resId, ex);
        }
        return null;
    }

    /**
     * Returns the complete uri from the meta data key of the injected setting metadata.
     *
     * A complete uri should contain at least one path segment and be one of the following types:
     *      content://authority/method
     *      content://authority/method/key
     *
     * If the uri from the tile is not complete, build a uri by the default method and the
     * preference key.
     */
    public static Uri getCompleteUri(Bundle metaData, String metaDataKey, String defaultMethod) {
        String uriString = metaData.getString(metaDataKey);
        if (TextUtils.isEmpty(uriString)) {
            return null;
        }

        Uri uri = Uri.parse(uriString);
        List<String> pathSegments = uri.getPathSegments();
        if (pathSegments != null && !pathSegments.isEmpty()) {
            return uri;
        }

        String key = metaData.getString(META_DATA_PREFERENCE_KEYHINT);
        if (TextUtils.isEmpty(key)) {
            LOG.w("Please specify the meta-data " + META_DATA_PREFERENCE_KEYHINT
                    + " in AndroidManifest.xml for " + uriString);
            return buildUri(uri.getAuthority(), defaultMethod);
        }
        return buildUri(uri.getAuthority(), defaultMethod, key);
    }

    private static Uri buildUri(String authority, String method, String key) {
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(authority)
                .appendPath(method)
                .appendPath(key)
                .build();
    }

    private static Uri buildUri(String authority, String method) {
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(authority)
                .appendPath(method)
                .build();
    }
}
