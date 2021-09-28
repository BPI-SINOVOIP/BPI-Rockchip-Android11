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

import com.google.common.base.Joiner;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.MappedByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;

/**
 * Dumps out the contents of a tzfile in a CSV form.
 *
 * <p>This class contains a near copy of logic found in Android's ZoneInfo class.
 */
public class TzFileDumper {

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("usage: java TzFileDumper <tzfile|dir> <output file|output dir>");
            System.exit(0);
        }

        File input = new File(args[0]);
        File output = new File(args[1]);
        if (input.isDirectory()) {
            if (!output.isDirectory()) {
                System.err.println("If first args is a directory, second arg must be a directory");
                System.exit(1);
            }

            for (File inputFile : input.listFiles()) {
                if (inputFile.isFile()) {
                    File outputFile = new File(output, inputFile.getName() + ".csv");
                    try {
                        new TzFileDumper(inputFile, outputFile).execute();
                    } catch (IOException e) {
                        System.err.println("Error processing:" + inputFile);
                    }
                }
            }
        } else {
            new TzFileDumper(input, output).execute();
        }
    }

    private final File inputFile;
    private final File outputFile;

    private TzFileDumper(File inputFile, File outputFile) {
        this.inputFile = inputFile;
        this.outputFile = outputFile;
    }

    private void execute() throws IOException {
        System.out.println("Dumping " + inputFile + " to " + outputFile);
        MappedByteBuffer mappedTzFile = ZoneSplitter.createMappedByteBuffer(inputFile);

        try (Writer fileWriter = new OutputStreamWriter(
                new FileOutputStream(outputFile), StandardCharsets.UTF_8)) {
            Header header32Bit = readHeader(mappedTzFile);
            List<Transition> transitions32Bit = read32BitTransitions(mappedTzFile, header32Bit);
            List<Type> types32Bit = readTypes(mappedTzFile, header32Bit);
            skipUninteresting32BitData(mappedTzFile, header32Bit);
            types32Bit = mergeTodInfo(mappedTzFile, header32Bit, types32Bit);

            writeCsvRow(fileWriter, "File format version: " + (char) header32Bit.tzh_version);
            writeCsvRow(fileWriter);
            writeCsvRow(fileWriter, "32-bit data");
            writeCsvRow(fileWriter);
            writeTypes(types32Bit, fileWriter);
            writeCsvRow(fileWriter);
            writeTransitions(transitions32Bit, types32Bit, fileWriter);
            writeCsvRow(fileWriter);

            if (header32Bit.tzh_version >= '2') {
                Header header64Bit = readHeader(mappedTzFile);
                List<Transition> transitions64Bit = read64BitTransitions(mappedTzFile, header64Bit);
                List<Type> types64Bit = readTypes(mappedTzFile, header64Bit);
                skipUninteresting64BitData(mappedTzFile, header64Bit);
                types64Bit = mergeTodInfo(mappedTzFile, header64Bit, types64Bit);

                writeCsvRow(fileWriter, "64-bit data");
                writeCsvRow(fileWriter);
                writeTypes(types64Bit, fileWriter);
                writeCsvRow(fileWriter);
                writeTransitions(transitions64Bit, types64Bit, fileWriter);
            }
        }
    }

    private Header readHeader(MappedByteBuffer mappedTzFile) throws IOException {
        // Variable names beginning tzh_ correspond to those in "tzfile.h".
        // Check tzh_magic.
        int tzh_magic = mappedTzFile.getInt();
        if (tzh_magic != 0x545a6966) { // "TZif"
            throw new IOException("File=" + inputFile + " has an invalid header=" + tzh_magic);
        }

        byte tzh_version = mappedTzFile.get();

        // Skip the uninteresting part of the header.
        mappedTzFile.position(mappedTzFile.position() + 15);
        int tzh_ttisgmtcnt = mappedTzFile.getInt();
        int tzh_ttisstdcnt = mappedTzFile.getInt();
        int tzh_leapcnt = mappedTzFile.getInt();

        // Read the sizes of the arrays we're about to read.
        int tzh_timecnt = mappedTzFile.getInt();
        // Arbitrary ceiling to prevent allocating memory for corrupt data.
        // 2 per year with 2^32 seconds would give ~272 transitions.
        final int MAX_TRANSITIONS = 2000;
        if (tzh_timecnt < 0 || tzh_timecnt > MAX_TRANSITIONS) {
            throw new IOException(
                    "File=" + inputFile + " has an invalid number of transitions=" + tzh_timecnt);
        }

        int tzh_typecnt = mappedTzFile.getInt();
        final int MAX_TYPES = 256;
        if (tzh_typecnt < 1) {
            throw new IOException("ZoneInfo requires at least one type to be provided for each"
                    + " timezone but could not find one for '" + inputFile + "'");
        } else if (tzh_typecnt > MAX_TYPES) {
            throw new IOException(
                    "File=" + inputFile + " has too many types=" + tzh_typecnt);
        }

        int tzh_charcnt = mappedTzFile.getInt();

        return new Header(
                tzh_version, tzh_ttisgmtcnt, tzh_ttisstdcnt, tzh_leapcnt, tzh_timecnt, tzh_typecnt,
                tzh_charcnt);
    }

    private List<Transition> read32BitTransitions(MappedByteBuffer mappedTzFile, Header header)
            throws IOException {

        // Read the data.
        int[] transitionTimes = new int[header.tzh_timecnt];
        fillIntArray(mappedTzFile, transitionTimes);

        byte[] typeIndexes = new byte[header.tzh_timecnt];
        mappedTzFile.get(typeIndexes);

        // Convert int times to longs
        long[] transitionTimesLong = new long[header.tzh_timecnt];
        for (int i = 0; i < header.tzh_timecnt; ++i) {
            transitionTimesLong[i] = transitionTimes[i];
        }

        return createTransitions(header, transitionTimesLong, typeIndexes);
    }

    private List<Transition> createTransitions(Header header,
            long[] transitionTimes, byte[] typeIndexes) throws IOException {
        List<Transition> transitions = new ArrayList<>();
        for (int i = 0; i < header.tzh_timecnt; ++i) {
            if (i > 0 && transitionTimes[i] <= transitionTimes[i - 1]) {
                throw new IOException(
                        inputFile + " transition at " + i + " is not sorted correctly, is "
                                + transitionTimes[i] + ", previous is " + transitionTimes[i - 1]);
            }

            int typeIndex = typeIndexes[i] & 0xff;
            if (typeIndex >= header.tzh_typecnt) {
                throw new IOException(inputFile + " type at " + i + " is not < "
                        + header.tzh_typecnt + ", is " + typeIndex);
            }

            Transition transition = new Transition(transitionTimes[i], typeIndex);
            transitions.add(transition);
        }
        return transitions;
    }

    private List<Transition> read64BitTransitions(MappedByteBuffer mappedTzFile, Header header)
            throws IOException {
        long[] transitionTimes = new long[header.tzh_timecnt];
        fillLongArray(mappedTzFile, transitionTimes);

        byte[] typeIndexes = new byte[header.tzh_timecnt];
        mappedTzFile.get(typeIndexes);

        return createTransitions(header, transitionTimes, typeIndexes);
    }

    private void writeTransitions(List<Transition> transitions, List<Type> types, Writer fileWriter)
            throws IOException {

        List<Object[]> rows = new ArrayList<>();
        for (Transition transition : transitions) {
            Type type = types.get(transition.typeIndex);
            Object[] row = new Object[] {
                    transition.transitionTimeSeconds,
                    transition.typeIndex,
                    formatTimeSeconds(transition.transitionTimeSeconds),
                    formatDurationSeconds(type.gmtOffsetSeconds),
                    formatIsDst(type.isDst),
            };
            rows.add(row);
        }

        writeCsvRow(fileWriter, "Transitions");
        writeTuplesCsv(fileWriter, rows, "transition", "type", "[UTC time]", "[Type offset]",
                "[Type isDST]");
    }

    private List<Type> readTypes(MappedByteBuffer mappedTzFile, Header header) throws IOException {
        List<Type> types = new ArrayList<>();
        for (int i = 0; i < header.tzh_typecnt; ++i) {
            int gmtOffsetSeconds = mappedTzFile.getInt();
            byte isDst = mappedTzFile.get();
            if (isDst != 0 && isDst != 1) {
                throw new IOException(inputFile + " dst at " + i + " is not 0 or 1, is " + isDst);
            }

            // We skip the abbreviation index.
            mappedTzFile.get();

            types.add(new Type(gmtOffsetSeconds, isDst));
        }
        return types;
    }

    private static void skipUninteresting32BitData(MappedByteBuffer mappedTzFile, Header header) {
        mappedTzFile.get(new byte[header.tzh_charcnt]);
        int leapInfoSize = 4 + 4;
        mappedTzFile.get(new byte[header.tzh_leapcnt * leapInfoSize]);
    }


    private void skipUninteresting64BitData(MappedByteBuffer mappedTzFile, Header header) {
        mappedTzFile.get(new byte[header.tzh_charcnt]);
        int leapInfoSize = 8 + 4;
        mappedTzFile.get(new byte[header.tzh_leapcnt * leapInfoSize]);
    }

    /**
     * Populate ttisstd and ttisgmt information by copying {@code types} and populating those fields
     * in the copies.
     */
    private static List<Type> mergeTodInfo(
            MappedByteBuffer mappedTzFile, Header header, List<Type> types) {

        byte[] ttisstds = new byte[header.tzh_ttisstdcnt];
        mappedTzFile.get(ttisstds);
        byte[] ttisgmts = new byte[header.tzh_ttisgmtcnt];
        mappedTzFile.get(ttisgmts);

        List<Type> outputTypes = new ArrayList<>();
        for (int i = 0; i < types.size(); i++) {
            Type inputType = types.get(i);
            Byte ttisstd = ttisstds.length == 0 ? null : ttisstds[i];
            Byte ttisgmt = ttisgmts.length == 0 ? null : ttisgmts[i];
            Type outputType =
                    new Type(inputType.gmtOffsetSeconds, inputType.isDst, ttisstd, ttisgmt);
            outputTypes.add(outputType);
        }
        return outputTypes;
    }

    private void writeTypes(List<Type> types, Writer fileWriter) throws IOException {
        List<Object[]> rows = new ArrayList<>();
        for (Type type : types) {
            Object[] row = new Object[] {
                    type.gmtOffsetSeconds,
                    type.isDst,
                    type.ttisgmt,
                    type.ttisstd,
                    formatDurationSeconds(type.gmtOffsetSeconds),
                    formatIsDst(type.isDst),
            };
            rows.add(row);
        }

        writeCsvRow(fileWriter, "Types");
        writeTuplesCsv(
                fileWriter, rows, "gmtOffset (seconds)", "isDst", "ttisgmt", "ttisstd",
                "[gmtOffset ISO]", "[DST?]");
    }

    private static void fillIntArray(MappedByteBuffer mappedByteBuffer, int[] toFill) {
        for (int i = 0; i < toFill.length; i++) {
            toFill[i] = mappedByteBuffer.getInt();
        }
    }

    private static void fillLongArray(MappedByteBuffer mappedByteBuffer, long[] toFill) {
        for (int i = 0; i < toFill.length; i++) {
            toFill[i] = mappedByteBuffer.getLong();
        }
    }

    private static String formatTimeSeconds(long timeInSeconds) {
        long timeInMillis = timeInSeconds * 1000L;
        return Instant.ofEpochMilli(timeInMillis).toString();
    }

    private static String formatDurationSeconds(int duration) {
        return Duration.ofSeconds(duration).toString();
    }

    private String formatIsDst(byte isDst) {
        return isDst == 0 ? "STD" : "DST";
    }

    private static void writeCsvRow(Writer writer, Object... values) throws IOException {
        writer.append(Joiner.on(',').join(values));
        writer.append('\n');
    }

    private static void writeTuplesCsv(Writer writer, List<Object[]> lines, String... headings)
            throws IOException {

        writeCsvRow(writer, (Object[]) headings);
        for (Object[] line : lines) {
            writeCsvRow(writer, line);
        }
    }

    private static class Header {

        /** The version. Known values are 0 (ASCII NUL), 50 (ASCII '2'), 51 (ASCII '3'). */
        final byte tzh_version;
        final int tzh_timecnt;
        final int tzh_typecnt;
        final int tzh_charcnt;
        final int tzh_leapcnt;
        final int tzh_ttisstdcnt;
        final int tzh_ttisgmtcnt;

        Header(byte tzh_version, int tzh_ttisgmtcnt, int tzh_ttisstdcnt, int tzh_leapcnt,
                int tzh_timecnt, int tzh_typecnt, int tzh_charcnt) {
            this.tzh_version = tzh_version;
            this.tzh_timecnt = tzh_timecnt;
            this.tzh_typecnt = tzh_typecnt;
            this.tzh_charcnt = tzh_charcnt;
            this.tzh_leapcnt = tzh_leapcnt;
            this.tzh_ttisstdcnt = tzh_ttisstdcnt;
            this.tzh_ttisgmtcnt = tzh_ttisgmtcnt;
        }
    }

    private static class Type {

        final int gmtOffsetSeconds;
        final byte isDst;
        final Byte ttisstd;
        final Byte ttisgmt;

        Type(int gmtOffsetSeconds, byte isDst) {
            this(gmtOffsetSeconds, isDst, null, null);
        }

        Type(int gmtOffsetSeconds, byte isDst, Byte ttisstd, Byte ttisgmt) {
            this.gmtOffsetSeconds = gmtOffsetSeconds;
            this.isDst = isDst;
            this.ttisstd = ttisstd;
            this.ttisgmt = ttisgmt;
        }
    }

    private static class Transition {

        final long transitionTimeSeconds;
        final int typeIndex;

        Transition(long transitionTimeSeconds, int typeIndex) {
            this.transitionTimeSeconds = transitionTimeSeconds;
            this.typeIndex = typeIndex;
        }
    }
}
