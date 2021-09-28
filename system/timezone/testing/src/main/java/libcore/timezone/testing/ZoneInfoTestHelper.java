/*
 * Copyright (C) 2016 The Android Open Source Project
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
package libcore.timezone.testing;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Helps with creating valid and invalid test data.
 */
public class ZoneInfoTestHelper {

    private ZoneInfoTestHelper() {}

    /**
     * Constructs valid and invalid zic data for tests.
     */
    public static class ZicDataBuilder {

        private int magic = 0x545a6966; // Default, valid magic.
        private long[] transitionTimes64Bit; // Time of each transition, one per transition.
        private byte[] transitionTypes64Bit; // Type of each transition, one per transition.
        private int[] isDsts; // Whether a type uses DST, one per type.
        private int[] offsetsSeconds; // The UTC offset, one per type.

        public ZicDataBuilder() {}

        public ZicDataBuilder setMagic(int magic) {
            this.magic = magic;
            return this;
        }

        /**
         * See {@link #setTransitions(long[][])} and {@link #setTypes(int[][])}.
         */
        public ZicDataBuilder setTransitionsAndTypes(
                long[][] transitionPairs, int[][] typePairs) {
            setTransitions(transitionPairs);
            setTypes(typePairs);
            return this;
        }
        /**
         * Sets transition information using an array of pairs of longs. e.g.
         *
         * new long[][] {
         *   { transitionTimeSeconds1, typeIndex1 },
         *   { transitionTimeSeconds2, typeIndex1 },
         * }
         */
        public ZicDataBuilder setTransitions(long[][] transitionPairs) {
            long[] transitions = new long[transitionPairs.length];
            byte[] types = new byte[transitionPairs.length];
            for (int i = 0; i < transitionPairs.length; i++) {
                transitions[i] = transitionPairs[i][0];
                types[i] = (byte) transitionPairs[i][1];
            }
            this.transitionTimes64Bit = transitions;
            this.transitionTypes64Bit = types;
            return this;
        }

        /**
         * Sets transition information using an array of pairs of ints. e.g.
         *
         * new int[][] {
         *   { offsetSeconds1, typeIsDst1 },
         *   { offsetSeconds2, typeIsDst2 },
         * }
         */
        public ZicDataBuilder setTypes(int[][] typePairs) {
            int[] isDsts = new int[typePairs.length];
            int[] offsetSeconds = new int[typePairs.length];
            for (int i = 0; i < typePairs.length; i++) {
                offsetSeconds[i] = typePairs[i][0];
                isDsts[i] = typePairs[i][1];
            }
            this.isDsts = isDsts;
            this.offsetsSeconds = offsetSeconds;
            return this;
        }

        /** Initializes to a minimum viable ZoneInfo data. */
        public ZicDataBuilder initializeToValid() {
            setTransitions(new long[0][0]);
            setTypes(new int[][] {
                    { 3600, 0}
            });
            return this;
        }

        public byte[] build() {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();

            // Compute the 32-bit transitions / types.
            List<Integer> transitionTimes32Bit = new ArrayList<>();
            List<Byte> transitionTypes32Bit = new ArrayList<>();
            if (transitionTimes64Bit.length > 0) {
                // Skip transitions < Integer.MIN_VALUE.
                int i = 0;
                while (i < transitionTimes64Bit.length
                        && transitionTimes64Bit[i] < Integer.MIN_VALUE) {
                    i++;
                }
                // If we skipped any, add a transition at Integer.MIN_VALUE like zic does.
                if (i > 0) {
                    transitionTimes32Bit.add(Integer.MIN_VALUE);
                    transitionTypes32Bit.add(transitionTypes64Bit[i - 1]);
                }
                // Copy remaining transitions / types that fit in the 32-bit range.
                while (i < transitionTimes64Bit.length
                        && transitionTimes64Bit[i] <= Integer.MAX_VALUE) {
                    transitionTimes32Bit.add((int) transitionTimes64Bit[i]);
                    transitionTypes32Bit.add(transitionTypes64Bit[i]);
                    i++;
                }
            }

            int tzh_timecnt_32 = transitionTimes32Bit.size();
            int tzh_timecnt_64 = transitionTimes64Bit.length;
            int tzh_typecnt = offsetsSeconds.length;
            int tzh_charcnt = 0;
            int tzh_leapcnt = 0;
            int tzh_ttisstdcnt = 0;
            int tzh_ttisgmtcnt = 0;

            // Write 32-bit data.
            writeHeader(baos, magic, tzh_timecnt_32, tzh_typecnt, tzh_charcnt, tzh_leapcnt,
                    tzh_ttisstdcnt, tzh_ttisgmtcnt);
            writeIntList(baos, transitionTimes32Bit);
            writeByteList(baos, transitionTypes32Bit);
            writeTypes(baos, offsetsSeconds, isDsts);
            writeTrailingUnusued32BitData(baos, tzh_charcnt, tzh_leapcnt, tzh_ttisstdcnt,
                    tzh_ttisgmtcnt);

            // 64-bit data.
            writeHeader(baos, magic, tzh_timecnt_64, tzh_typecnt, tzh_charcnt, tzh_leapcnt,
                    tzh_ttisstdcnt, tzh_ttisgmtcnt);
            writeLongArray(baos, transitionTimes64Bit);
            writeByteArray(baos, transitionTypes64Bit);
            writeTypes(baos, offsetsSeconds, isDsts);

            return baos.toByteArray();
        }

        private static void writeTypes(
                ByteArrayOutputStream baos, int[] offsetsSeconds, int[] isDsts) {
            // Offset / DST
            for (int i = 0; i < offsetsSeconds.length; i++) {
                writeInt(baos, offsetsSeconds[i]);
                writeByte(baos, isDsts[i]);
                // Unused data on Android (abbreviation list index).
                writeByte(baos, 0);
            }
        }

        /**
         * Writes the data after types information Android doesn't currently use but is needed so
         * that the start of the 64-bit data can be found.
         */
        private static void writeTrailingUnusued32BitData(
                ByteArrayOutputStream baos, int tzh_charcnt, int tzh_leapcnt,
                int tzh_ttisstdcnt, int tzh_ttisgmtcnt) {

            // Time zone abbreviations text.
            writeByteArray(baos, new byte[tzh_charcnt]);
            // tzh_leapcnt repetitions of leap second transition time + correction.
            int leapInfoSize = 4 + 4;
            writeByteArray(baos, new byte[tzh_leapcnt * leapInfoSize]);
            // tzh_ttisstdcnt bytes
            writeByteArray(baos, new byte[tzh_ttisstdcnt]);
            // tzh_ttisgmtcnt bytes
            writeByteArray(baos, new byte[tzh_ttisgmtcnt]);
        }

        private static void writeHeader(ByteArrayOutputStream baos, int magic, int tzh_timecnt,
                int tzh_typecnt, int tzh_charcnt, int tzh_leapcnt, int tzh_ttisstdcnt,
                int tzh_ttisgmtcnt) {
            // Magic number.
            writeInt(baos, magic);
            // tzh_version
            writeByte(baos, '2');

            // Write out the unused part of the header.
            for (int i = 0; i < 15; ++i) {
                baos.write(i);
            }
            // Write out the known header fields.
            writeInt(baos, tzh_ttisgmtcnt);
            writeInt(baos, tzh_ttisstdcnt);
            writeInt(baos, tzh_leapcnt);
            writeInt(baos, tzh_timecnt);
            writeInt(baos, tzh_typecnt);
            writeInt(baos, tzh_charcnt);
        }
    }

    /**
     * Constructs valid and invalid tzdata files for tests. See also ZoneCompactor class in
     * system/timezone/zone_compactor which is the real thing.
     */
    public static class TzDataBuilder {

        private String headerMagic;
        // A list is used in preference to a Map to allow simulation of badly ordered / duplicate
        // IDs.
        private List<ZicDatum> zicData = new ArrayList<>();
        private String zoneTab;
        private Integer indexOffsetOverride;
        private Integer dataOffsetOverride;
        private Integer zoneTabOffsetOverride;

        public TzDataBuilder() {}

        /** Sets the header. A valid header is in the form "tzdata2016g". */
        public TzDataBuilder setHeaderMagic(String headerMagic) {
            this.headerMagic = headerMagic;
            return this;
        }

        public TzDataBuilder setIndexOffsetOverride(int indexOffset) {
            this.indexOffsetOverride = indexOffset;
            return this;
        }

        public TzDataBuilder setDataOffsetOverride(int dataOffset) {
            this.dataOffsetOverride = dataOffset;
            return this;
        }

        public TzDataBuilder setZoneTabOffsetOverride(int zoneTabOffset) {
            this.zoneTabOffsetOverride = zoneTabOffset;
            return this;
        }

        /**
         * Adds data for a new zone. These must be added in ID string order to generate
         * a valid file.
         */
        public TzDataBuilder addZicData(String id, byte[] data) {
            zicData.add(new ZicDatum(id, data));
            return this;
        }

        public TzDataBuilder setZoneTab(String zoneTab) {
            this.zoneTab = zoneTab;
            return this;
        }

        public TzDataBuilder initializeToValid() {
            setHeaderMagic("tzdata9999a");
            addZicData("Europe/Elbonia", new ZicDataBuilder().initializeToValid().build());
            setZoneTab("ZoneTab data");
            return this;
        }

        public TzDataBuilder clearZicData() {
            zicData.clear();
            return this;
        }

        public byte[] build() {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();

            byte[] headerMagicBytes = headerMagic.getBytes(StandardCharsets.US_ASCII);
            baos.write(headerMagicBytes, 0, headerMagicBytes.length);
            baos.write(0);

            // Write out the offsets for later manipulation.
            int indexOffsetOffset = baos.size();
            writeInt(baos, 0);
            int dataOffsetOffset = baos.size();
            writeInt(baos, 0);
            int zoneTabOffsetOffset = baos.size();
            writeInt(baos, 0);

            // Construct the data section in advance, so we know the offsets.
            ByteArrayOutputStream dataBytes = new ByteArrayOutputStream();
            Map<String, Integer> offsets = new HashMap<>();
            for (ZicDatum datum : zicData) {
                int offset = dataBytes.size();
                offsets.put(datum.id, offset);
                writeByteArray(dataBytes, datum.data);
            }

            int indexOffset = baos.size();

            // Write the index section.
            for (ZicDatum zicDatum : zicData) {
                // Write the ID.
                String id = zicDatum.id;
                byte[] idBytes = id.getBytes(StandardCharsets.US_ASCII);
                byte[] paddedIdBytes = new byte[40];
                System.arraycopy(idBytes, 0, paddedIdBytes, 0, idBytes.length);
                writeByteArray(baos, paddedIdBytes);
                // Write offset of zic data in the data section.
                Integer offset = offsets.get(id);
                writeInt(baos, offset);
                // Write the length of the zic data.
                writeInt(baos, zicDatum.data.length);
                // Write a filler value (not used)
                writeInt(baos, 0);
            }

            // Write the data section.
            int dataOffset = baos.size();
            writeByteArray(baos, dataBytes.toByteArray());

            // Write the zoneTab section.
            int zoneTabOffset = baos.size();
            byte[] zoneTabBytes = zoneTab.getBytes(StandardCharsets.US_ASCII);
            writeByteArray(baos, zoneTabBytes);

            byte[] bytes = baos.toByteArray();
            setInt(bytes, indexOffsetOffset,
                    indexOffsetOverride != null ? indexOffsetOverride : indexOffset);
            setInt(bytes, dataOffsetOffset,
                    dataOffsetOverride != null ? dataOffsetOverride : dataOffset);
            setInt(bytes, zoneTabOffsetOffset,
                    zoneTabOffsetOverride != null ? zoneTabOffsetOverride : zoneTabOffset);
            return bytes;
        }

        private static class ZicDatum {
            public final String id;
            public final byte[] data;

            ZicDatum(String id, byte[] data) {
                this.id = id;
                this.data = data;
            }
        }
    }

    static void writeByteArray(ByteArrayOutputStream baos, byte[] array) {
        baos.write(array, 0, array.length);
    }

    static void writeByteList(ByteArrayOutputStream baos, List<Byte> list) {
        for (byte value : list) {
            baos.write(value);
        }
    }

    static void writeByte(ByteArrayOutputStream baos, int value) {
        baos.write(value);
    }

    static void writeLongArray(ByteArrayOutputStream baos, long[] array) {
        for (long value : array) {
            writeLong(baos, value);
        }
    }

    static void writeIntList(ByteArrayOutputStream baos, List<Integer> list) {
        for (int value : list) {
            writeInt(baos, value);
        }
    }

    static void writeInt(ByteArrayOutputStream os, int value) {
        byte[] bytes = ByteBuffer.allocate(4).putInt(value).array();
        writeByteArray(os, bytes);
    }

    static void writeLong(ByteArrayOutputStream os, long value) {
        byte[] bytes = ByteBuffer.allocate(8).putLong(value).array();
        writeByteArray(os, bytes);
    }

    static void setInt(byte[] bytes, int offset, int value) {
        bytes[offset] = (byte) (value >>> 24);
        bytes[offset + 1] = (byte) (value >>> 16);
        bytes[offset + 2] = (byte) (value >>> 8);
        bytes[offset + 3] = (byte) value;
    }
}
