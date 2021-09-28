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

// ART file format at art/runtime/image.h
public class ArtParser extends FileParser {
    // The magic values for the ART identification.
    private static final byte[] ART_MAGIC = {(byte) 'a', (byte) 'r', (byte) 't', (byte) 0x0A};
    private static final int HEADER_SIZE = 512;
    private ArtInfo.Builder mArtInfoBuilder;

    public ArtParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.ART;
    }

    @Override
    public void setAdditionalInfo() {
        getFileEntryBuilder().setArtInfo(getArtInfo());
    }

    @Override
    public String getCodeId() {
        if (mArtInfoBuilder == null) {
            parse();
        }
        return mCodeId;
    }

    public ArtInfo getArtInfo() {
        if (mArtInfoBuilder == null) {
            parse();
        }
        return mArtInfoBuilder.build();
    }

    private void parse() {
        byte[] buffer = new byte[HEADER_SIZE];
        mArtInfoBuilder = ArtInfo.newBuilder();
        // ToDo check this
        int kSectionCount = 11;
        int kImageMethodsCount = 20;
        try {
            RandomAccessFile raFile = new RandomAccessFile(getFile(), "r");
            raFile.seek(0);
            raFile.readFully(buffer, 0, HEADER_SIZE);
            raFile.close();

            if (buffer[0] != ART_MAGIC[0]
                    || buffer[1] != ART_MAGIC[1]
                    || buffer[2] != ART_MAGIC[2]
                    || buffer[3] != ART_MAGIC[3]) {
                String content = new String(buffer);
                System.err.println("Invalid ART file:" + getFileName() + " " + content);
                mArtInfoBuilder.setValid(false);
                return;
            }

            int offset = 4;
            mArtInfoBuilder.setVersion(new String(Arrays.copyOfRange(buffer, offset, offset + 4)));
            offset += 4;
            mArtInfoBuilder.setImageBegin(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setImageSize(getIntLittleEndian(buffer, offset));
            offset += 4;

            int oatChecksum = getIntLittleEndian(buffer, offset);
            offset += 4;
            mArtInfoBuilder.setOatChecksum(oatChecksum);
            mCodeId = String.format(CODE_ID_FORMAT, oatChecksum);

            mArtInfoBuilder.setOatFileBegin(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setOatDataBegin(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setOatDataEnd(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setOatFileEnd(getIntLittleEndian(buffer, offset));
            offset += 4;

            mArtInfoBuilder.setBootImageBegin(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setBootImageSize(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setBootOatBegin(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setBootOatSize(getIntLittleEndian(buffer, offset));
            offset += 4;

            mArtInfoBuilder.setPatchDelta(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setImageRoots(getIntLittleEndian(buffer, offset));
            offset += 4;

            mArtInfoBuilder.setPointerSize(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setCompilePic(getIntLittleEndian(buffer, offset));
            offset += 4;

            // Todo check further
            mArtInfoBuilder.setIsPic(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setStorageMode(getIntLittleEndian(buffer, offset));
            offset += 4;
            mArtInfoBuilder.setDataSize(getIntLittleEndian(buffer, offset));

            mArtInfoBuilder.setValid(true);
        } catch (Exception ex) {
            System.err.println("Invalid ART file:" + getFileName());
            mArtInfoBuilder.setValid(false);
        }
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + ArtParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to parse APK file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);

            File aFile = new File(fileName);
            ArtParser aParser = new ArtParser(aFile);
            Entry fileEntry = aParser.getFileEntryBuilder().build();

            writeTextFormatMessage(outputFileName, fileEntry);
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(ArtParser.class.getSimpleName());
    }
}
