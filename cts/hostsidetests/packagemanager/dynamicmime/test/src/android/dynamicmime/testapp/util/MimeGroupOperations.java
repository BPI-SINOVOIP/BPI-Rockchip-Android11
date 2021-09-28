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

package android.dynamicmime.testapp.util;

import android.dynamicmime.testapp.assertions.MimeGroupAssertions;
import android.dynamicmime.testapp.commands.MimeGroupCommands;

import androidx.annotation.Nullable;

import java.util.Set;

/**
 * Base class for commands/queries related to MIME groups
 * Allows independent implementations for commands and assertions
 */
public class MimeGroupOperations extends MimeGroupAssertions implements MimeGroupCommands {
    private MimeGroupCommands mCommands;
    private MimeGroupAssertions mAssertions;

    public MimeGroupOperations(MimeGroupCommands commands, MimeGroupAssertions assertions) {
        mCommands = commands;
        mAssertions = assertions;
    }

    @Override
    public final void assertMatchedByMimeGroup(String mimeGroup, String mimeType) {
        mAssertions.assertMatchedByMimeGroup(mimeGroup, mimeType);
    }

    @Override
    public final void assertNotMatchedByMimeGroup(String mimeGroup, String mimeType) {
        mAssertions.assertNotMatchedByMimeGroup(mimeGroup, mimeType);
    }

    @Override
    public final void assertMimeGroupInternal(String mimeGroup, Set<String> mimeTypes) {
        mAssertions.assertMimeGroupInternal(mimeGroup, mimeTypes);
    }

    @Override
    public void setMimeGroup(String mimeGroup, Set<String> mimeTypes) {
        mCommands.setMimeGroup(mimeGroup, mimeTypes);
    }

    @Nullable
    @Override
    public Set<String> getMimeGroupInternal(String mimeGroup) {
        return mCommands.getMimeGroupInternal(mimeGroup);
    }

    public MimeGroupCommands getCommands() {
        return mCommands;
    }

    public MimeGroupAssertions getAssertions() {
        return mAssertions;
    }

    public void setCommands(MimeGroupCommands commands) {
        mCommands = commands;
    }

    public void setAssertions(MimeGroupAssertions assertions) {
        mAssertions = assertions;
    }
}
