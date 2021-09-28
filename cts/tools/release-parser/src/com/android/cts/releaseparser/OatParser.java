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

import com.android.compatibility.common.util.ReadElf;
import com.android.cts.releaseparser.ReleaseProto.*;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Logger;

// Oat is embedded in an ELF file .rodata secion
// art/runtime/oat.h & oat.cc
// art/dex2oat/linker/oat_writer.h
public class OatParser extends FileParser {
    // The magic values for the OAT identification.
    private static final byte[] OAT_MAGIC = {(byte) 'o', (byte) 'a', (byte) 't', (byte) 0x0A};
    private static final int HEADER_SIZE = 64;
    private static String FIRST_NO_PIC_VERSION = "162\0";
    private OatInfo.Builder mOatInfoBuilder;
    private int mBits;
    private String mArch;
    private byte[] mRoData;
    private List<String> mDependencies;

    public OatParser(File file) {
        super(file);
        mDependencies = null;
    }

    @Override
    public List<String> getDependencies() {
        if (mDependencies == null) {
            parse();
        }
        return mDependencies;
    }

    @Override
    public String getCodeId() {
        if (mOatInfoBuilder == null) {
            parse();
        }
        return mCodeId;
    }

    @Override
    public void setAdditionalInfo() {
        getFileEntryBuilder().setOatInfo(getOatInfo());
    }

    public OatInfo getOatInfo() {
        if (mOatInfoBuilder == null) {
            parse();
        }
        return mOatInfoBuilder.build();
    }

    private void parse() {
        mOatInfoBuilder = OatInfo.newBuilder();
        try {
            mDependencies = new ArrayList<String>();
            ReadElf elf = ReadElf.read(getFile());
            mOatInfoBuilder.setBits(elf.getBits());
            mOatInfoBuilder.setArchitecture(elf.getArchitecture());
            mRoData = elf.getRoData();
            praseOat(mRoData);
            mOatInfoBuilder.setValid(true);
        } catch (Exception ex) {
            System.err.println("Invalid OAT file:" + getFileName());
            ex.printStackTrace(System.out);
            mOatInfoBuilder.setValid(false);
        }
    }

    private void praseOat(byte[] buffer) throws IllegalArgumentException {
        if (buffer[0] != OAT_MAGIC[0]
                || buffer[1] != OAT_MAGIC[1]
                || buffer[2] != OAT_MAGIC[2]
                || buffer[3] != OAT_MAGIC[3]) {
            String content = new String(buffer);
            System.err.println("Invalid OAT file:" + getFileName() + " " + content);
            throw new IllegalArgumentException("Invalid OAT MAGIC");
        }

        int offset = 4;
        String version = new String(Arrays.copyOfRange(buffer, offset, offset + 4));
        mOatInfoBuilder.setVersion(version);
        offset += 4;
        mOatInfoBuilder.setAdler32Checksum(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setInstructionSet(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setInstructionSetFeaturesBitmap(getIntLittleEndian(buffer, offset));
        offset += 4;
        int dexFileCount = getIntLittleEndian(buffer, offset);
        mOatInfoBuilder.setDexFileCount(dexFileCount);
        offset += 4;
        int dexFileOffset = getIntLittleEndian(buffer, offset);
        mOatInfoBuilder.setOatDexFilesOffset(dexFileOffset);
        offset += 4;
        mOatInfoBuilder.setExecutableOffset(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setInterpreterToInterpreterBridgeOffset(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setInterpreterToCompiledCodeBridgeOffset(
                getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setJniDlsymLookupOffset(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setQuickGenericJniTrampolineOffset(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setQuickImtConflictTrampolineOffset(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setQuickResolutionTrampolineOffset(getIntLittleEndian(buffer, offset));
        offset += 4;
        mOatInfoBuilder.setQuickToInterpreterBridgeOffset(getIntLittleEndian(buffer, offset));
        offset += 4;

        // for backward compatibility, removed from version 162, see
        // aosp/e0669326c0282b5b645aba75160425eef9d57617
        if (version.compareTo(FIRST_NO_PIC_VERSION) < 0) {
            mOatInfoBuilder.setImagePatchDelta(getIntLittleEndian(buffer, offset));
            offset += 4;
        }

        mOatInfoBuilder.setImageFileLocationOatChecksum(getIntLittleEndian(buffer, offset));
        offset += 4;

        // for backward compatibility, removed from version 162, see
        // aosp/e0669326c0282b5b645aba75160425eef9d57617
        if (version.compareTo(FIRST_NO_PIC_VERSION) < 0) {
            mOatInfoBuilder.setImageFileLocationOatDataBegin(getIntLittleEndian(buffer, offset));
            offset += 4;
        }
        int storeSize = getIntLittleEndian(buffer, offset);
        mOatInfoBuilder.setKeyValueStoreSize(storeSize);
        offset += 4;

        // adds dependencies at the build time
        // trims device & dex_bootjars from the path
        // e.g. out/target/product/sailfish/dex_bootjars/system/framework/arm64/boot.art
        final String dexBootJars = "/dex_bootjars/";
        Map<String, String> kvMap = getKeyValuePairMap(buffer, offset, storeSize);
        mOatInfoBuilder.putAllKeyValueStore(kvMap);
        String imageLocation = kvMap.get("image-location");
        if (imageLocation != null) {
            for (String path : imageLocation.split(":")) {
                mDependencies.add(path.substring(path.indexOf(dexBootJars) + dexBootJars.length()));
            }
        }
        String bootClasspath = kvMap.get("bootclasspath");
        if (bootClasspath != null) {
            for (String path : bootClasspath.split(":")) {
                mDependencies.add(path.substring(path.indexOf(dexBootJars) + dexBootJars.length()));
            }
        }
        if (!mDependencies.isEmpty()) {
            getFileEntryBuilder().addAllDependencies(mDependencies);
        }

        StringBuilder codeIdSB = new StringBuilder();
        // art/dex2oat/linker/oat_writer.cc OatDexFile
        offset = dexFileOffset;
        for (int i = 0; i < dexFileCount; i++) {
            OatDexInfo.Builder oatDexInfoBuilder = OatDexInfo.newBuilder();

            // dex_file_location_size_
            int length = getIntLittleEndian(buffer, offset);
            offset += 4;
            // dex_file_location_data_
            oatDexInfoBuilder.setDexFileLocationData(
                    new String(Arrays.copyOfRange(buffer, offset, offset + length)));
            offset += length;

            // dex_file_location_checksum_
            int dexFileLocationChecksum = getIntLittleEndian(buffer, offset);
            offset += 4;
            oatDexInfoBuilder.setDexFileLocationChecksum(dexFileLocationChecksum);
            codeIdSB.append(String.format(CODE_ID_FORMAT, dexFileLocationChecksum));

            // dex_file_offset_
            oatDexInfoBuilder.setDexFileOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            // lookup_table_offset_
            oatDexInfoBuilder.setLookupTableOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            // uint32_t class_offsets_offset_;
            oatDexInfoBuilder.setClassOffsetsOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            // uint32_t method_bss_mapping_offset_;
            oatDexInfoBuilder.setMethodBssMappingOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            // uint32_t type_bss_mapping_offset_;
            oatDexInfoBuilder.setTypeBssMappingOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            // uint32_t string_bss_mapping_offset_;
            oatDexInfoBuilder.setStringBssMappingOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            // uint32_t dex_sections_layout_offset_;
            oatDexInfoBuilder.setDexSectionsLayoutOffset(getIntLittleEndian(buffer, offset));
            offset += 4;
            mOatInfoBuilder.addOatDexInfo(oatDexInfoBuilder.build());
        }
        mCodeId = codeIdSB.toString();
    }

    // as art/runtime/oat.cc GetStoreValueByKey
    private Map<String, String> getKeyValuePairMap(byte[] buffer, int start, int size) {
        HashMap<String, String> keyValuePairMap = new HashMap<String, String>();
        int currentPosition = start;
        int end = start + size;
        String key, value;
        while (currentPosition < end) {
            key = getString(buffer, currentPosition, end);
            currentPosition += key.length() + 1;
            value = getString(buffer, currentPosition, end);
            currentPosition += value.length() + 1;
            keyValuePairMap.put(key, value);
        }
        return keyValuePairMap;
    }

    private String getString(byte[] buffer, int start, int end) {
        String str = null;
        int currentPosition = start;
        while (currentPosition < end) {
            if (buffer[currentPosition] == 0x0) {
                str = new String(Arrays.copyOfRange(buffer, start, currentPosition));
                break;
            } else {
                currentPosition++;
            }
        }
        return str;
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.OAT;
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + OatParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to prase OAT file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);

            File aFile = new File(fileName);
            OatParser aParser = (OatParser) FileParser.getParser(aFile);
            Entry fileEntry = aParser.getFileEntryBuilder().build();

            writeTextFormatMessage(outputFileName, fileEntry);
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(OatParser.class.getSimpleName());
    }
}
