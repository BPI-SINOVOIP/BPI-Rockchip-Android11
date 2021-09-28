/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.cts.readexternalstorageapp;

import static com.android.cts.externalstorageapp.CommonExternalStorageTest.PACKAGE_NONE;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.PACKAGE_READ;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.assertFileNoAccess;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.assertFileReadWriteAccess;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.getAllPackageSpecificNoGiftPaths;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.readInt;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.writeInt;

import android.test.AndroidTestCase;

import java.io.File;
import java.util.List;

public class ReadGiftTest extends AndroidTestCase {
    public void testStageNonGifts() throws Exception {
        final List<File> readList = getAllPackageSpecificNoGiftPaths(getContext(), PACKAGE_READ);
        for (File read : readList) {
            read.getParentFile().mkdirs();
            assertTrue(read.createNewFile());
            writeInt(read, 101);
        }
    }

    /**
     * Verify we can read & write only our gifts.
     */
    public void testNoGifts() throws Exception {
        final List<File> noneList = getAllPackageSpecificNoGiftPaths(getContext(), PACKAGE_NONE);
        for (File none : noneList) {
            assertFileNoAccess(none);
        }

        final List<File> readList = getAllPackageSpecificNoGiftPaths(getContext(), PACKAGE_READ);
        for (File read : readList) {
            assertFileReadWriteAccess(read);
            assertEquals(101, readInt(read));
        }
    }
}
