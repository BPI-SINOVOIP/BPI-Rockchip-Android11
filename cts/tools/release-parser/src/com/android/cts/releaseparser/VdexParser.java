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
import java.io.RandomAccessFile;
import java.util.Arrays;
import java.util.logging.Logger;

// art/runtime/vdex_file.h & vdex_file.cc
public class VdexParser extends FileParser {
    // The magic values for the VDEX identification.
    private static final byte[] VDEX_MAGIC = {(byte) 'v', (byte) 'd', (byte) 'e', (byte) 'x'};
    private static final int HEADER_SIZE = 64;
    private VdexInfo.Builder mVdexInfoBuilder;

    public VdexParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.VDEX;
    }

    @Override
    public void setAdditionalInfo() {
        getFileEntryBuilder().setVdexInfo(getVdexInfo());
    }

    @Override
    public String getCodeId() {
        if (mVdexInfoBuilder == null) {
            parse();
        }
        return mCodeId;
    }

    public VdexInfo getVdexInfo() {
        if (mVdexInfoBuilder == null) {
            parse();
        }
        return mVdexInfoBuilder.build();
    }

    private void parse() {
        byte[] buffer = new byte[HEADER_SIZE];
        mVdexInfoBuilder = VdexInfo.newBuilder();

        try {
            RandomAccessFile raFile = new RandomAccessFile(getFile(), "r");
            raFile.seek(0);
            raFile.readFully(buffer, 0, HEADER_SIZE);
            raFile.close();

            // ToDo: this is specific for 019 VerifierDepsVersion. Need to handle changes for older
            // versions
            if (buffer[0] != VDEX_MAGIC[0]
                    || buffer[1] != VDEX_MAGIC[1]
                    || buffer[2] != VDEX_MAGIC[2]
                    || buffer[3] != VDEX_MAGIC[3]) {
                String content = new String(buffer);
                System.err.println("Invalid VDEX file:" + getFileName() + " " + content);
                throw new IllegalArgumentException("Invalid VDEX MAGIC");
            }
            int offset = 4;
            String version = new String(Arrays.copyOfRange(buffer, offset, offset + 4));
            mVdexInfoBuilder.setVerifierDepsVersion(version);
            offset += 4;
            String dex_section_version = new String(Arrays.copyOfRange(buffer, offset, offset + 4));
            mVdexInfoBuilder.setDexSectionVersion(dex_section_version);
            offset += 4;
            int numberOfDexFiles = getIntLittleEndian(buffer, offset);
            mVdexInfoBuilder.setNumberOfDexFiles(numberOfDexFiles);
            offset += 4;
            mVdexInfoBuilder.setVerifierDepsSize(getIntLittleEndian(buffer, offset));
            offset += 4;

            // Code Id format: [0xchecksum1],...
            StringBuilder codeIdSB = new StringBuilder();
            for (int i = 0; i < numberOfDexFiles; i++) {
                int checksums = getIntLittleEndian(buffer, offset);
                offset += 4;
                mVdexInfoBuilder.addChecksums(checksums);
                codeIdSB.append(String.format(CODE_ID_FORMAT, checksums));
            }
            mCodeId = codeIdSB.toString();

            for (int i = 0; i < numberOfDexFiles; i++) {
                DexSectionHeader.Builder dshBuilder = DexSectionHeader.newBuilder();
                dshBuilder.setDexSize(getIntLittleEndian(buffer, offset));
                offset += 4;
                dshBuilder.setDexSharedDataSize(getIntLittleEndian(buffer, offset));
                offset += 4;
                dshBuilder.setQuickeningInfoSize(getIntLittleEndian(buffer, offset));
                offset += 4;
                mVdexInfoBuilder.addDexSectionHeaders(dshBuilder.build());
                offset += 4;
            }

            for (int i = 0; i < numberOfDexFiles; i++) {
                int quicken_table_off = getIntLittleEndian(buffer, offset);
                offset += 4;
                // Todo processing Dex
            }

        } catch (Exception ex) {
            System.err.println("Invalid VDEX file:" + getFileName());
            mVdexInfoBuilder.setValid(false);
        }
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + VdexParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to parse VDEX file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);

            File aFile = new File(fileName);
            VdexParser aParser = new VdexParser(aFile);
            Entry fileEntry = aParser.getFileEntryBuilder().build();
            writeTextFormatMessage(outputFileName, fileEntry);
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(VdexParser.class.getSimpleName());
    }
}
