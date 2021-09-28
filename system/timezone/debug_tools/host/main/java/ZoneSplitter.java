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

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.util.Arrays;

/**
 * Reverses the ZoneCompactor process to extract information and zic output files from Android's
 * tzdata file. This enables easier debugging / inspection of Android's tzdata file with standard
 * tools like zdump or Android tools like TzFileDumper.
 *
 * <p>This class contains a copy of logic found in Android's ZoneInfoDb.
 */
public class ZoneSplitter {

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("usage: java ZoneSplitter <tzdata file> <output directory>");
            System.exit(0);
        }
        new ZoneSplitter(args[0], args[1]).execute();
    }

    private final File tzData;
    private final File outputDir;

    private ZoneSplitter(String tzData, String outputDir) {
        this.tzData = new File(tzData);
        this.outputDir = new File(outputDir);
    }

    private void execute() throws IOException {
        if (!(tzData.exists() && tzData.isFile() && tzData.canRead())) {
            throw new IOException(tzData + " not found or is not readable");
        }
        if (!(outputDir.exists() && outputDir.isDirectory())) {
            throw new IOException(outputDir + " not found or is not a directory");
        }

        MappedByteBuffer mappedFile = createMappedByteBuffer(tzData);

        // byte[12] tzdata_version  -- "tzdata2012f\0"
        // int index_offset
        // int data_offset
        // int zonetab_offset
        writeVersionFile(mappedFile, outputDir);

        final int fileSize = (int) tzData.length();
        int index_offset = mappedFile.getInt();
        validateOffset(index_offset, fileSize);
        int data_offset = mappedFile.getInt();
        validateOffset(data_offset, fileSize);
        int zonetab_offset = mappedFile.getInt();
        validateOffset(zonetab_offset, fileSize);

        if (index_offset >= data_offset || data_offset >= zonetab_offset) {
            throw new IOException("Invalid offset: index_offset=" + index_offset
                    + ", data_offset=" + data_offset + ", zonetab_offset=" + zonetab_offset
                    + ", fileSize=" + fileSize);
        }

        File zicFilesDir = new File(outputDir, "zones");
        zicFilesDir.mkdir();
        extractZicFiles(mappedFile, index_offset, data_offset, zicFilesDir);

        writeZoneTabFile(mappedFile, zonetab_offset, fileSize - zonetab_offset, outputDir);
    }

    static MappedByteBuffer createMappedByteBuffer(File tzData) throws IOException {
        MappedByteBuffer mappedFile;
        RandomAccessFile file = new RandomAccessFile(tzData, "r");
        try (FileChannel fileChannel = file.getChannel()) {
            mappedFile = fileChannel.map(FileChannel.MapMode.READ_ONLY, 0, file.length());
        }
        mappedFile.load();
        return mappedFile;
    }

    private static void validateOffset(int offset, int size) throws IOException {
        if (offset < 0 || offset >= size) {
            throw new IOException("Invalid offset=" + offset + ", size=" + size);
        }
    }

    private static void writeVersionFile(MappedByteBuffer mappedFile, File targetDir)
            throws IOException {

        byte[] tzdata_version = new byte[12];
        mappedFile.get(tzdata_version);

        String magic = new String(tzdata_version, 0, 6, StandardCharsets.US_ASCII);
        if (!magic.startsWith("tzdata") || tzdata_version[11] != 0) {
            throw new IOException("bad tzdata magic: " + Arrays.toString(tzdata_version));
        }
        writeStringUtf8ToFile(new File(targetDir, "version"),
                new String(tzdata_version, 6, 5, StandardCharsets.US_ASCII));
    }

    private static void extractZicFiles(MappedByteBuffer mappedFile, int indexOffset,
            int dataOffset, File outputDir) throws IOException {

        mappedFile.position(indexOffset);

        // The index of the tzdata file is made up of entries for each time zone ID which describe
        // the location of the associated zic data in the data section of the file. The index
        // section has no padding so we can determine the number of entries from the size.
        //
        // Each index entry consists of:
        // byte[MAXNAME] idBytes - the id string, \0 terminated. e.g. "America/New_York\0"
        // int32 byteOffset      - the offset of the start of the zic data relative to the start of
        //                         the tzdata data section
        // int32 length          - the length of the of the zic data
        // int32 unused          - no longer used
        final int MAXNAME = 40;
        final int SIZEOF_OFFSET = 4;
        final int SIZEOF_INDEX_ENTRY = MAXNAME + 3 * SIZEOF_OFFSET;

        int indexSize = (dataOffset - indexOffset);
        if (indexSize % SIZEOF_INDEX_ENTRY != 0) {
            throw new IOException("Index size is not divisible by " + SIZEOF_INDEX_ENTRY
                    + ", indexSize=" + indexSize);
        }

        byte[] idBytes = new byte[MAXNAME];
        int entryCount = indexSize / SIZEOF_INDEX_ENTRY;
        int[] byteOffsets = new int[entryCount];
        int[] lengths = new int[entryCount];
        String[] ids = new String[entryCount];

        for (int i = 0; i < entryCount; i++) {
            // Read the fixed length timezone ID.
            mappedFile.get(idBytes, 0, idBytes.length);

            // Read the offset into the file where the data for ID can be found.
            byteOffsets[i] = mappedFile.getInt();
            byteOffsets[i] += dataOffset;

            lengths[i] = mappedFile.getInt();
            if (lengths[i] < 44) {
                throw new IOException("length in index file < sizeof(tzhead)");
            }
            mappedFile.getInt(); // Skip the unused 4 bytes that used to be the raw offset.

            // Calculate the true length of the ID.
            int len = 0;
            while (len < idBytes.length && idBytes[len] != 0) {
                len++;
            }
            if (len == 0) {
                throw new IOException("Invalid ID at index=" + i);
            }
            ids[i] = new String(idBytes, 0, len, StandardCharsets.US_ASCII);
            if (i > 0) {
                if (ids[i].compareTo(ids[i - 1]) <= 0) {
                    throw new IOException(
                            "Index not sorted or contains multiple entries with the same ID"
                            + ", index=" + i + ", ids[i]=" + ids[i] + ", ids[i - 1]=" + ids[i - 1]);
                }
            }
        }
        for (int i = 0; i < entryCount; i++) {
            String id = ids[i];
            int byteOffset = byteOffsets[i];
            int length = lengths[i];

            File subFile = new File(outputDir, id.replace('/', '_'));
            mappedFile.position(byteOffset);
            byte[] bytes = new byte[length];
            mappedFile.get(bytes, 0, length);

            writeBytesToFile(subFile, bytes);
        }
    }

    private static void writeZoneTabFile(MappedByteBuffer mappedFile,
            int zoneTabOffset, int zoneTabSize, File outputDir) throws IOException {
        byte[] bytes = new byte[zoneTabSize];
        mappedFile.position(zoneTabOffset);
        mappedFile.get(bytes, 0, bytes.length);
        writeBytesToFile(new File(outputDir, "zone.tab"), bytes);
    }

    private static void writeStringUtf8ToFile(File file, String string) throws IOException {
        writeBytesToFile(file, string.getBytes(StandardCharsets.UTF_8));
    }

    private static void writeBytesToFile(File file, byte[] bytes) throws IOException {
        System.out.println("Writing: " + file);
        Files.write(file.toPath(), bytes, StandardOpenOption.CREATE);
    }
}
