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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class ZipParser extends FileParser {
    private PackageFileContent.Builder mPackageFileContentBuilder;
    private HashMap<String, Entry> mFileMap;
    private List<String> mDependencies;
    private List<String> mDynamicLoadingDependencies;
    private boolean mParseSo;
    private boolean mParseInternalApi;
    private StringBuilder mCodeIdStringBuilder;

    // Todo: provide utilities to parse files in zip, e.g. SOs in an APK
    ZipParser(File file) {
        super(file);
        mParseSo = true;
        // default off to avoid a large output
        mParseInternalApi = false;
        mCodeIdStringBuilder = null;
    }

    @Override
    public List<String> getDependencies() {
        if (mDependencies == null) {
            parseFileContent();
        }
        return mDependencies;
    }

    @Override
    public List<String> getDynamicLoadingDependencies() {
        if (mDynamicLoadingDependencies == null) {
            // This also parses DynamicLoadingDependencies
            getDependencies();
        }
        return mDynamicLoadingDependencies;
    }

    @Override
    public String getCodeId() {
        if (mCodeIdStringBuilder == null) {
            parseFileContent();
        }
        return mCodeIdStringBuilder.toString();
    }

    @Override
    public Entry.Builder getFileEntryBuilder() {
        if (mFileEntryBuilder == null) {
            super.getFileEntryBuilder();
            parseFileContent();
            mFileEntryBuilder.addAllDependencies(getDependencies());
            mFileEntryBuilder.addAllDynamicLoadingDependencies(getDynamicLoadingDependencies());
        }
        return mFileEntryBuilder;
    }

    public void setParseSo(boolean parseSo) {
        mParseSo = parseSo;
    }

    public void setParseInternalApi(boolean parseInternalApi) {
        mParseInternalApi = parseInternalApi;
    }

    public static File getFilefromZip(ZipFile zipFile, ZipEntry zipEntry) throws IOException {
        String fileName = zipEntry.getName();
        File tmpFile = File.createTempFile("RPZ", "");
        tmpFile.deleteOnExit();
        InputStream iStream = zipFile.getInputStream(zipEntry);
        FileOutputStream fOutputStream = new FileOutputStream(tmpFile);
        byte[] buffer = new byte[READ_BLOCK_SIZE];
        int length;
        while ((length = iStream.read(buffer)) >= 0) {
            fOutputStream.write(buffer, 0, length);
        }
        iStream.close();
        fOutputStream.close();
        return tmpFile;
    }

    public PackageFileContent getPackageFileContent() {
        if (mPackageFileContentBuilder == null) {
            parseFileContent();
        }
        return mPackageFileContentBuilder.build();
    }

    private void parseFileContent() {
        ZipFile zFile = null;
        mPackageFileContentBuilder = PackageFileContent.newBuilder();
        mFileMap = new HashMap<String, Entry>();
        mDependencies = new ArrayList<String>();
        mDynamicLoadingDependencies = new ArrayList<String>();
        mCodeIdStringBuilder = new StringBuilder();

        try {
            zFile = new ZipFile(getFile());

            final Enumeration<? extends ZipEntry> entries = zFile.entries();
            while (entries.hasMoreElements()) {
                final ZipEntry entry = entries.nextElement();
                final String name = entry.getName();

                Entry.Builder entryBuilder = Entry.newBuilder();
                entryBuilder.setName(name);
                entryBuilder.setContentId(String.format(CODE_ID_FORMAT, entry.hashCode()));
                entryBuilder.setSize(entry.getSize());
                if (entry.isDirectory()) {
                    entryBuilder.setType(Entry.EntryType.FOLDER);
                } else {
                    entryBuilder.setType(Entry.EntryType.FILE);
                    appendToCodeID(entry);
                    if (mParseSo) {
                        // ToDo: to be optimized if taking too long
                        if (name.endsWith(SO_EXT_TAG)) {
                            try {
                                final File soFile = getFilefromZip(zFile, entry);
                                SoParser fParser = new SoParser(soFile);
                                fParser.setPackageName(name);
                                fParser.setParseInternalApi(mParseInternalApi);

                                if (fParser.getDependencies() != null) {
                                    mDependencies.addAll(fParser.getDependencies());
                                }
                                if (fParser.getDynamicLoadingDependencies() != null) {
                                    mDynamicLoadingDependencies.addAll(
                                            fParser.getDynamicLoadingDependencies());
                                }
                                entryBuilder.setAppInfo(fParser.getAppInfo());
                            } catch (IOException ex) {
                                System.err.println(
                                        "Failed to parse: " + name + "\n" + ex.getMessage());
                            }
                        }
                    }
                }
                mFileMap.put(name, entryBuilder.build());
            }
            mPackageFileContentBuilder.putAllEntries(mFileMap);
        } catch (IOException e) {
            System.err.println("Failed to parse: " + getFileName() + "\n" + e.getMessage());
            // error while opening a ZIP file
        } finally {
            if (zFile != null) {
                try {
                    zFile.close();
                } catch (IOException e) {
                    System.err.println("Failed to close: " + getFileName() + "\n" + e.getMessage());
                }
            }
        }
    }

    private void appendToCodeID(ZipEntry zEntry) {
        String name = zEntry.getName();
        if (name.endsWith(SO_EXT_TAG)
                || name.endsWith(DEX_EXT_TAG)
                || name.endsWith(ANDROID_MANIFEST_TAG)) {
            mCodeIdStringBuilder.append(String.format(CODE_ID_FORMAT, zEntry.hashCode()));
        }
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + ZipParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to prase ZIP file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The path of the file to be parsed.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n"
                    + "\t-pi \t Parses internal methods and fields too. Output will be large when parsing multiple files in a release.\n"
                    + "\t-s \t Skips parsing embedded SO files if it takes too long time.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterList("i").get(0);
            String outputFileName = argParser.getParameterElement("of", 0);
            boolean parseSo = !argParser.containsOption("s");
            boolean parseInternalApi = argParser.containsOption("pi");

            File aFile = new File(fileName);
            ZipParser aParser = new ZipParser(aFile);
            aParser.setParseSo(parseSo);
            aParser.setParseInternalApi(parseInternalApi);

            Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
            writeTextFormatMessage(outputFileName, fileEntryBuilder.build());
        } catch (Exception ex) {
            System.out.printf(USAGE_MESSAGE);
            System.err.printf(ex.getMessage());
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(ZipParser.class.getSimpleName());
    }
}
