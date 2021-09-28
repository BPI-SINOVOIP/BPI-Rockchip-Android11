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

package android.dynamicmime.testapp.assertions;

import android.content.Context;
import android.dynamicmime.testapp.util.AppMimeGroups;
import android.util.ArraySet;

import java.util.Set;

public class AppAssertionsByGroupData extends AssertionsByGroupData {
    private final AppMimeGroups mAppMimeGroups;

    AppAssertionsByGroupData(Context context, String targetPackage) {
        mAppMimeGroups = AppMimeGroups.with(context, targetPackage);
    }

    @Override
    protected Set<String> getMimeGroup(String mimeGroup) {
        try {
            String[] mimeTypes = mAppMimeGroups.get(mimeGroup);
            return mimeTypes != null ? new ArraySet<>(mimeTypes) : null;
        } catch (Throwable exception) {
            return null;
        }
    }
}
