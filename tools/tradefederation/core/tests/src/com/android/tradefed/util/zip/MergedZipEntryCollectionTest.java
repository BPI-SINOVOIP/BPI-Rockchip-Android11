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
package com.android.tradefed.util.zip;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link MergedZipEntryCollection} */
@RunWith(JUnit4.class)
public class MergedZipEntryCollectionTest {

    private static final int COMPRESSED_SIZE = 2 * MergedZipEntryCollection.MAX_GAP;

    /** Test that zip entries are merged into sections as expected due to small gap in size. */
    @Test
    public void testMergeZipEntries_smallGap() throws Exception {
        List<CentralDirectoryInfo> entries = new ArrayList<>();
        long startOffset = 10;
        CentralDirectoryInfo info = createZipEntry(startOffset);
        entries.add(info);
        startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;

        // Create a gap smaller than MAX_GAP.
        startOffset += MergedZipEntryCollection.MAX_GAP / 2;
        info = createZipEntry(startOffset);
        entries.add(info);
        startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;

        List<MergedZipEntryCollection> collections =
                MergedZipEntryCollection.createCollections(entries);
        assertEquals(1, collections.size());
        assertEquals(2, collections.get(0).getZipEntries().size());
        assertEquals(10, collections.get(0).getStartOffset());
        assertEquals(22598, collections.get(0).getEndOffset());
    }

    /** Test that zip entries are not merged due to large gap in size. */
    @Test
    public void testMergeZipEntries_largeGap() throws Exception {
        List<CentralDirectoryInfo> entries = new ArrayList<>();
        long startOffset = 10;
        CentralDirectoryInfo info = createZipEntry(startOffset);
        entries.add(info);
        startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;

        // Create a gap larger than MAX_GAP.
        startOffset += MergedZipEntryCollection.MAX_GAP * 2;
        info = createZipEntry(startOffset);
        entries.add(info);
        startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;

        List<MergedZipEntryCollection> collections =
                MergedZipEntryCollection.createCollections(entries);
        assertEquals(2, collections.size());
        assertEquals(1, collections.get(0).getZipEntries().size());
        assertEquals(10, collections.get(0).getStartOffset());
        assertEquals(10280, collections.get(0).getEndOffset());
        assertEquals(18472, collections.get(1).getStartOffset());
        assertEquals(28742, collections.get(1).getEndOffset());
    }

    /** Test that zip entries are merged into a single section due to a small gap in percentage. */
    @Test
    public void testMergeZipEntries_smallGapPercent() throws Exception {
        List<CentralDirectoryInfo> entries = new ArrayList<>();
        long startOffset = 10;
        // Create enough entries so the gap size is greater than MAX_GAP.
        for (int i = 0; i < 10; i++) {
            CentralDirectoryInfo info = createZipEntry(startOffset);
            entries.add(info);
            startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;
        }

        // Create a gap smaller than MAX_GAP_PERCENTAGE
        startOffset += (long) (startOffset * MergedZipEntryCollection.MAX_GAP_PERCENTAGE) - 1024;
        CentralDirectoryInfo info = createZipEntry(startOffset);
        entries.add(info);
        startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;

        List<MergedZipEntryCollection> collections =
                MergedZipEntryCollection.createCollections(entries);
        assertEquals(1, collections.size());
        assertEquals(11, collections.get(0).getZipEntries().size());
        assertEquals(10, collections.get(0).getStartOffset());
        assertEquals(127362, collections.get(0).getEndOffset());
    }

    /** Test that zip entries are not merged due to a large gap in percentage. */
    @Test
    public void testMergeZipEntries_largeGapPercent() throws Exception {
        List<CentralDirectoryInfo> entries = new ArrayList<>();
        long startOffset = 10;
        // Create enough entries so the gap size is greater than MAX_GAP.
        for (int i = 0; i < 10; i++) {
            CentralDirectoryInfo info = createZipEntry(startOffset);
            entries.add(info);
            startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;
        }

        // Create a gap larger than MAX_GAP_PERCENTAGE
        startOffset += (long) (startOffset * MergedZipEntryCollection.MAX_GAP_PERCENTAGE * 3);
        CentralDirectoryInfo info = createZipEntry(startOffset);
        entries.add(info);
        startOffset += info.getCompressedSize() + MergedZipEntryCollection.HEADER_SIZE;

        List<MergedZipEntryCollection> collections =
                MergedZipEntryCollection.createCollections(entries);
        assertEquals(2, collections.size());
        assertEquals(10, collections.get(0).getZipEntries().size());
        assertEquals(10, collections.get(0).getStartOffset());
        assertEquals(102710, collections.get(0).getEndOffset());
        assertEquals(1, collections.get(1).getZipEntries().size());
        assertEquals(148929, collections.get(1).getStartOffset());
        assertEquals(159199, collections.get(1).getEndOffset());
    }

    /** Create a {@link CentralDirectoryInfo} object for testing. */
    private CentralDirectoryInfo createZipEntry(long startOffset) {
        CentralDirectoryInfo info = new CentralDirectoryInfo();
        info.setLocalHeaderOffset(startOffset);
        info.setCompressedSize(COMPRESSED_SIZE);
        return info;
    }
}
