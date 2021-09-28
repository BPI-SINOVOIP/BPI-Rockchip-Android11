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
package com.android.wallpaper.util;

import android.content.Intent;
import android.net.Uri;

/** Util class for deep link. */
public class DeepLinkUtils {
    private static final String KEY_COLLECTION_ID = "collection_id";
    private static final String SCHEME = "https";
    private static final String SCHEME_SPECIFIC_PART_PREFIX = "//g.co/wallpaper";

    /**
     * Checks if it is the deep link case.
     */
    public static boolean isDeepLink(Intent intent) {
        Uri data = intent.getData();
        return data != null && SCHEME.equals(data.getScheme())
                && data.getSchemeSpecificPart().startsWith(SCHEME_SPECIFIC_PART_PREFIX);
    }

    /**
     * Gets the wallpaper collection which wants to deep link to.
     *
     * @return the wallpaper collection id
     */
    public static String getCollectionId(Intent intent) {
        return isDeepLink(intent) ? intent.getData().getQueryParameter(KEY_COLLECTION_ID) : null;
    }
}
