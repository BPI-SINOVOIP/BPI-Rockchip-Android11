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

package com.android.cts.tagging;

import android.compat.cts.CompatChangeGatingTestCase;

import java.io.FileWriter;

public class TaggingBaseTest extends CompatChangeGatingTestCase {

    protected static final long NATIVE_HEAP_POINTER_TAGGING_CHANGE_ID = 135754954;

    protected boolean supportsTaggedPointers = false;

    private boolean supportsTaggedPointers() throws Exception {
        String kernelVersion = runCommand("uname -rm");
        if (!kernelVersion.contains("aarch64")) {
            return false;
        }

        String kernelVersionSplit[] = kernelVersion.split("\\.");
        if (kernelVersionSplit.length < 2) {
            return false;
        }

        int major = Integer.parseInt(kernelVersionSplit[0]);
        if (major > 4) {
            return true;
        } else if (major < 4) {
            return false;
        }

        // Major version is 4. Check that the minor is >= 14.
        int minor = Integer.parseInt(kernelVersionSplit[1]);
        if (minor < 14) {
            return false;
        }
        return true;
    }

    @Override
    protected void setUp() throws Exception {
        supportsTaggedPointers = supportsTaggedPointers();
    }
}
