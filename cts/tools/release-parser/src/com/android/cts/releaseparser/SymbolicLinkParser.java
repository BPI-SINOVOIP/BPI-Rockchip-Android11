/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.releaseparser;

import com.android.cts.releaseparser.ReleaseProto.*;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class SymbolicLinkParser extends FileParser {
    public SymbolicLinkParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.SYMBOLIC_LINK;
    }

    @Override
    public List<String> getDependencies() {
        ArrayList results = new ArrayList<String>();
        results.add(getFileName());
        return results;
    }

    @Override
    public String getFileContentId() {
        // NO_ID for Symbolic Link
        return mContentId;
    }
}
