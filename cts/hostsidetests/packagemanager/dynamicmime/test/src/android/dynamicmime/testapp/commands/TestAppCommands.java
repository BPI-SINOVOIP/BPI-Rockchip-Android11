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

package android.dynamicmime.testapp.commands;

import android.content.Context;
import android.content.pm.PackageManager;

import androidx.annotation.Nullable;

import java.util.Set;

public class TestAppCommands implements MimeGroupCommands {
    private PackageManager mPM;

    TestAppCommands(Context context) {
        mPM = context.getPackageManager();
    }

    @Override
    public void setMimeGroup(String mimeGroup, Set<String> mimeTypes) {
        mPM.setMimeGroup(mimeGroup, mimeTypes);
    }

    @Nullable
    @Override
    public Set<String> getMimeGroupInternal(String mimeGroup) {
        return mPM.getMimeGroup(mimeGroup);
    }
}
