/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.tv.settings.util;

import android.content.ContentProviderClient;
import android.content.Context;
import android.net.Uri;

/** Utility class for slice **/
public final class SliceUtils {
    public static final String PATH_SLICE_FRAGMENT =
            "com.android.tv.twopanelsettings.slices.SliceFragment";
    /**
     * Check if slice provider exists.
     */
    public static boolean isSliceProviderValid(Context context, String uri) {
        if (uri == null) {
            return false;
        }
        ContentProviderClient client =
                context.getContentResolver().acquireContentProviderClient(Uri.parse(uri));
        if (client != null) {
            client.close();
            return true;
        } else {
            return false;
        }
    }
}
