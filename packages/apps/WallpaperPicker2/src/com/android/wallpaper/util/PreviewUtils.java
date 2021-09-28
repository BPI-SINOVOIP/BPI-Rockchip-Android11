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

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

/** Util class for wallpaper preview. */
public class PreviewUtils {

    private static final String PREVIEW = "preview";
    private static final String METHOD_GET_PREVIEW = "get_preview";

    private final Context mContext;
    private final String mProviderAuthority;
    private final ProviderInfo mProviderInfo;

    public PreviewUtils(Context context, String authorityMetadataKey) {
        mContext = context;
        Intent homeIntent = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME);

        ResolveInfo info = context.getPackageManager().resolveActivity(homeIntent,
                PackageManager.MATCH_DEFAULT_ONLY | PackageManager.GET_META_DATA);
        if (info != null && info.activityInfo != null && info.activityInfo.metaData != null) {
            mProviderAuthority = info.activityInfo.metaData.getString(authorityMetadataKey);
        } else {
            mProviderAuthority = null;
        }
        // TODO: check permissions if needed
        mProviderInfo = TextUtils.isEmpty(mProviderAuthority) ? null
                : mContext.getPackageManager().resolveContentProvider(mProviderAuthority, 0);
    }

    /** Render preview under the current grid option. */
    public Bundle renderPreview(Bundle bundle) {
        return mContext.getContentResolver().call(getUri(PREVIEW), METHOD_GET_PREVIEW, null,
                bundle);
    }

    /** Easy way to generate a Uri with the provider info from this class. */
    public Uri getUri(String path) {
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(mProviderInfo.authority)
                .appendPath(path)
                .build();
    }

    /** Return whether preview is supported. */
    public boolean supportsPreview() {
        return mProviderInfo != null;
    }
}
