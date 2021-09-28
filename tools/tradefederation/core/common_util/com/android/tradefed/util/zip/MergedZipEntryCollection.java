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

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Merge individual zip entries in a large zip file into blocks to minimize the download attempts.
 */
public class MergedZipEntryCollection {

    // The maximum number of bytes between each section of merged zip entries.
    public static final int MAX_GAP = 4096;

    // The maximum percentage of gaps between zip entries over the total size of the download block.
    public static final double MAX_GAP_PERCENTAGE = 0.15;

    // Best guess of header size. 2k Should be more than enough for file path and extra attributes.
    public static final int HEADER_SIZE = LocalFileHeader.LOCAL_FILE_HEADER_SIZE + 2048;

    private List<CentralDirectoryInfo> mZipEntries;

    public MergedZipEntryCollection(List<CentralDirectoryInfo> zipEntries) {
        mZipEntries = zipEntries;
    }

    public long getStartOffset() {
        return mZipEntries.get(0).getLocalHeaderOffset();
    }

    public long getEndOffset() {
        CentralDirectoryInfo lastEntry = mZipEntries.get(mZipEntries.size() - 1);
        return lastEntry.getLocalHeaderOffset() + lastEntry.getCompressedSize() + HEADER_SIZE;
    }

    public List<CentralDirectoryInfo> getZipEntries() {
        return mZipEntries;
    }

    /*
     * Merge a list of zip entries into groups to minimize download attempts.
     *
     *  @param zipEntries a list of {@link CentralDirectoryInfo} for a zip file.
     *
     *  @return a list of {@link MergedZipEntryCollection}, each contains a list of
     *    {@link CentralDirectoryInfo} that are stored closely inside the zip file.
     */
    public static List<MergedZipEntryCollection> createCollections(
            List<CentralDirectoryInfo> zipEntries) {
        if (zipEntries.size() == 0) {
            return new ArrayList<MergedZipEntryCollection>();
        }

        // Sort the entries by start offset.
        List<CentralDirectoryInfo> entries =
                zipEntries
                        .stream()
                        .sorted(Comparator.comparing(CentralDirectoryInfo::getLocalHeaderOffset))
                        .collect(Collectors.toList());
        long endOffset = -1;
        long totalGap = 0;
        List<MergedZipEntryCollection> collections = new ArrayList<>();
        List<CentralDirectoryInfo> group = new ArrayList<>();
        for (CentralDirectoryInfo entry : entries) {
            if (endOffset >= 0) {
                long newGap = entry.getLocalHeaderOffset() - endOffset + totalGap;
                long totalSize =
                        entry.getLocalHeaderOffset()
                                + HEADER_SIZE
                                + entry.getCompressedSize()
                                - group.get(0).getLocalHeaderOffset();
                double gapPercentage = (double) newGap / totalSize;
                if (endOffset < entry.getLocalHeaderOffset() - MAX_GAP
                        && MAX_GAP_PERCENTAGE < gapPercentage) {
                    collections.add(new MergedZipEntryCollection(group));
                    group = new ArrayList<>();
                    totalGap = 0;
                }
            }
            group.add(entry);
            if (group.size() > 1 && entry.getLocalHeaderOffset() > endOffset) {
                totalGap += entry.getLocalHeaderOffset() - endOffset;
            }
            endOffset = entry.getLocalHeaderOffset() + HEADER_SIZE + entry.getCompressedSize();
        }
        collections.add(new MergedZipEntryCollection(group));

        return collections;
    }
}
