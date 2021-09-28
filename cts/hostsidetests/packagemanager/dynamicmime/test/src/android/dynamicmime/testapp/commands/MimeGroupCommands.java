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

import static android.dynamicmime.common.Constants.GROUP_FIRST;
import static android.dynamicmime.common.Constants.GROUP_SECOND;
import static android.dynamicmime.common.Constants.GROUP_THIRD;
import static android.dynamicmime.common.Constants.PACKAGE_HELPER_APP;
import static android.dynamicmime.common.Constants.PACKAGE_PREFERRED_APP;
import static android.dynamicmime.common.Constants.PACKAGE_UPDATE_APP;

import android.content.Context;
import android.util.ArraySet;

import androidx.annotation.Nullable;

import java.util.Collections;
import java.util.Set;

public interface MimeGroupCommands {
    void setMimeGroup(String mimeGroup, Set<String> mimeTypes);
    @Nullable
    Set<String> getMimeGroupInternal(String mimeGroup);

    default void addMimeTypeToGroup(String mimeGroup, String... mimeTypes) {
        ArraySet<String> typesToAdd = new ArraySet<>(mimeTypes);

        Set<String> newMimeTypes = new ArraySet<>(getMimeGroupInternal(mimeGroup));
        newMimeTypes.addAll(typesToAdd);

        setMimeGroup(mimeGroup, newMimeTypes);
    }

    default void removeMimeTypeFromGroup(String mimeGroup, String... mimeTypes) {
        ArraySet<String> typesToRemove = new ArraySet<>(mimeTypes);

        Set<String> newMimeTypes = new ArraySet<>(getMimeGroupInternal(mimeGroup));
        newMimeTypes.removeAll(typesToRemove);

        setMimeGroup(mimeGroup, newMimeTypes);
    }

    default void setMimeGroup(String mimeGroup, String... mimeTypes) {
        setMimeGroup(mimeGroup, new ArraySet<>(mimeTypes));
    }

    default void clearMimeGroup(String mimeGroup) {
        setMimeGroup(mimeGroup, Collections.emptySet());
    }

    default void clearGroups() {
        safeClearMimeGroup(GROUP_FIRST);
        safeClearMimeGroup(GROUP_SECOND);
        safeClearMimeGroup(GROUP_THIRD);
    }

    default void safeClearMimeGroup(String mimeGroup) {
        try {
            clearMimeGroup(mimeGroup);
        } catch (Throwable ignored) {
        }
    }

    static MimeGroupCommands testApp(Context context) {
        return new TestAppCommands(context);
    }

    static MimeGroupCommands helperApp(Context context) {
        return new AppCommands(context, PACKAGE_HELPER_APP);
    }

    static MimeGroupCommands appWithUpdates(Context context) {
        return new AppCommands(context, PACKAGE_UPDATE_APP);
    }

    static MimeGroupCommands preferredApp(Context context) {
        return new AppCommands(context, PACKAGE_PREFERRED_APP);
    }

    static MimeGroupCommands noOp() {
        return new MimeGroupCommands() {
            @Override
            public void setMimeGroup(String mimeGroup, Set<String> mimeTypes) {
            }

            @Override
            public Set<String> getMimeGroupInternal(String mimeGroup) {
                return null;
            }
        };
    }
}
