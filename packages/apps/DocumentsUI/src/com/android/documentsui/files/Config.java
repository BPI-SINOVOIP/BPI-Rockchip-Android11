/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.documentsui.files;

import com.android.documentsui.ActivityConfig;
import com.android.documentsui.base.DocumentStack;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.util.VersionUtils;

/**
 * Provides support for Files activity specific specializations.
 */
public final class Config extends ActivityConfig {

    @Override
    public boolean managedModeEnabled(DocumentStack stack) {
        // This method helps us understand when to kick in special manage mode behaviors.
        final RootInfo root = stack.getRoot();

        // When in Files activity, allow External Storage provider to view
        // Android/[data|obb|sandbox] directories which are otherwise hidden for privacy reasons.
        if (VersionUtils.isAtLeastR() && root != null && root.isExternalStorage()) {
            return true;
        }
        // When in downloads top level directory, we also show active downloads.
        // And while we don't allow folders in Downloads, we do allow Zip files in
        // downloads that themselves can be opened and viewed like directories.
        return root != null
                && root.isDownloads()
                && stack.size() == 1;
    }

    @Override
    public boolean dragAndDropEnabled() {
        return true;
    }
}
