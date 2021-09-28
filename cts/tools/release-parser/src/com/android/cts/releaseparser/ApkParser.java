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
import java.util.logging.Level;
import java.util.logging.Logger;

public class ApkParser extends ZipParser {
    private ApiPackage mExternalApiPackage;
    private ApiPackage mInternalApiPackage;
    private AppInfo.Builder mAppInfoBuilder;

    public ApkParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.APK;
    }

    @Override
    public void setAdditionalInfo() {
        getFileEntryBuilder().setAppInfo(getAppInfo());
    }

    public ApiPackage getExternalApiPackage() {
        if (mExternalApiPackage == null) {
            prase();
        }
        return mExternalApiPackage;
    }

    // Todo
    public ApiPackage getTestCaseApiPackage() {
        if (mInternalApiPackage == null) {
            prase();
        }
        return mInternalApiPackage;
    }

    public AppInfo getAppInfo() {
        if (mAppInfoBuilder == null) {
            prase();
        }
        return mAppInfoBuilder.build();
    }

    private void prase() {
        // todo parse dependencies
        processManifest();
        processDex();
        processZip();
        getLogger().log(Level.WARNING, "ToDo,parse dependencies," + getFileName());
    }

    private void processManifest() {
        AndroidManifestParser manifestParser = new AndroidManifestParser(getFile());
        mAppInfoBuilder = manifestParser.getAppInfoBuilder();
    }

    private void processDex() {
        DexParser dexParser = new DexParser(getFile());
        dexParser.setPackageName(mAppInfoBuilder.getPackageName());
        mExternalApiPackage = dexParser.getExternalApiPackage();
        mAppInfoBuilder.addExternalApiPackages(mExternalApiPackage);
        mInternalApiPackage = dexParser.getInternalApiPackage();
        mAppInfoBuilder.addInternalApiPackages(mInternalApiPackage);
    }

    private void processZip() {
        mAppInfoBuilder.setPackageFileContent(getPackageFileContent());
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + ApkParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to prase APK file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n"
                    + "\t-pi \t Parses internal methods and fields too. Output will be large when parsing multiple files in a release.\n"
                    + "\t-s \t Skips parsing embedded SO files if it takes too long time.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);
            boolean parseSo = !argParser.containsOption("s");
            boolean parseInternalApi = argParser.containsOption("pi");

            File aFile = new File(fileName);
            ApkParser aParser = new ApkParser(aFile);
            aParser.setParseSo(parseSo);
            aParser.setParseInternalApi(parseInternalApi);
            Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
            writeTextFormatMessage(outputFileName, fileEntryBuilder.build());
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(ApkParser.class.getSimpleName());
    }
}
