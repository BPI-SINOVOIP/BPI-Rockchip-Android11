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
import com.google.protobuf.TextFormat;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

public class SoParser extends FileParser {
    private int mBits;
    private String mArch;
    private List<String> mDependencies;
    private List<String> mDynamicLoadingDependencies;
    private ReadElf mElf;
    private String mPackageName;
    private ApiPackage.Builder mExternalApiPackageBuilder;
    private HashMap<String, ApiClass.Builder> mExternalApiClassBuilderMap;
    private ApiPackage.Builder mInternalApiPackageBuilder;
    private AppInfo.Builder mAppInfoBuilder;
    private boolean mParseInternalApi;

    public SoParser(File file) {
        super(file);
        mBits = 0;
        mArch = null;
        mDependencies = null;
        mDynamicLoadingDependencies = null;
        mAppInfoBuilder = null;
        // default is the file name with out extenion
        mPackageName = getFileName().split("\\.")[0];
        // default off to avoid a large output
        mParseInternalApi = false;
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.SO;
    }

    @Override
    public String getCodeId() {
        return getFileContentId();
    }

    @Override
    public List<String> getDependencies() {
        if (mDependencies == null) {
            parse();
        }
        return mDependencies;
    }

    @Override
    public List<String> getDynamicLoadingDependencies() {
        if (mDynamicLoadingDependencies == null) {
            parse();
        }
        return mDynamicLoadingDependencies;
    }

    @Override
    public int getAbiBits() {
        if (mBits == 0) {
            parse();
        }
        return mBits;
    }

    @Override
    public String getAbiArchitecture() {
        if (mArch == null) {
            parse();
        }
        return mArch;
    }

    public void setPackageName(String name) {
        String[] subStr = name.split(File.separator);
        mPackageName = subStr[subStr.length - 1];
    }

    public void setParseInternalApi(boolean parseInternalApi) {
        mParseInternalApi = parseInternalApi;
    }

    public AppInfo getAppInfo() {
        if (mAppInfoBuilder == null) {
            mAppInfoBuilder = AppInfo.newBuilder();
            mAppInfoBuilder.setPackageName(mPackageName);
            mAppInfoBuilder.addInternalApiPackages(getInternalApiPackage());
            mAppInfoBuilder.addExternalApiPackages(getExternalApiPackage());
        }
        return mAppInfoBuilder.build();
    }

    public ApiPackage getExternalApiPackage() {
        if (mExternalApiPackageBuilder == null) {
            parse();
        }
        return mExternalApiPackageBuilder.build();
    }

    public ApiPackage getInternalApiPackage() {
        if (mInternalApiPackageBuilder == null) {
            parse();
        }
        return mInternalApiPackageBuilder.build();
    }

    private void parse() {
        mExternalApiPackageBuilder = ApiPackage.newBuilder();
        mExternalApiClassBuilderMap = new HashMap<String, ApiClass.Builder>();
        mInternalApiPackageBuilder = ApiPackage.newBuilder();
        try {
            ReadElf mElf = ReadElf.read(getFile());
            mBits = mElf.getBits();
            mArch = mElf.getArchitecture();
            mDependencies = mElf.getDynamicDependencies();
            // Check Dynamic Loading dependencies
            mDynamicLoadingDependencies = getDynamicLoadingDependencies(mElf);
            parseApi(mElf.getDynSymArr());
        } catch (Exception ex) {
            mDependencies = super.getDependencies();
            mDynamicLoadingDependencies = super.getDynamicLoadingDependencies();
            mBits = -1;
            mArch = "unknown";
            getLogger()
                    .log(
                            Level.SEVERE,
                            String.format(
                                    "SoParser fails to parse %s. \n%s",
                                    getFileName(), ex.getMessage()));
            ex.printStackTrace();
        }
    }

    private void parseApi(ReadElf.Symbol[] symArr) {
        ApiClass.Builder mInternalApiClassBuilder = ApiClass.newBuilder();
        mInternalApiClassBuilder.setName(mPackageName);

        for (ReadElf.Symbol symbol : symArr) {
            if (symbol.isExtern()) {
                // Internal methods & fields
                if (mParseInternalApi) {
                    // Skips reference symbols
                    if (isInternalReferenceSymbol(symbol)) {
                        continue;
                    }
                    if (symbol.type == ReadElf.Symbol.STT_OBJECT) {
                        ApiField.Builder fieldBuilder = ApiField.newBuilder();
                        fieldBuilder.setName(symbol.name);
                        mInternalApiClassBuilder.addFields(fieldBuilder.build());
                    } else {
                        ApiMethod.Builder methodBuilder = ApiMethod.newBuilder();
                        methodBuilder.setName(symbol.name);
                        mInternalApiClassBuilder.addMethods(methodBuilder.build());
                    }
                }
            } else if (symbol.isGlobalUnd()) {
                // External dependency
                String className = symbol.getExternalLibFileName();
                ApiClass.Builder apiClassBuilder =
                        ClassUtils.getApiClassBuilder(mExternalApiClassBuilderMap, className);
                if (symbol.type == ReadElf.Symbol.STT_OBJECT) {
                    ApiField.Builder fieldBuilder = ApiField.newBuilder();
                    fieldBuilder.setName(symbol.name);
                    apiClassBuilder.addFields(fieldBuilder.build());
                } else {
                    ApiMethod.Builder methodBuilder = ApiMethod.newBuilder();
                    methodBuilder.setName(symbol.name);
                    apiClassBuilder.addMethods(methodBuilder.build());
                }
            }
        }
        if (mParseInternalApi) {
            mInternalApiPackageBuilder.addClasses(mInternalApiClassBuilder.build());
        }
        ClassUtils.addAllApiClasses(mExternalApiClassBuilderMap, mExternalApiPackageBuilder);
    }

    private ApiClass.Builder getApiClassBuilder(
            HashMap<String, ApiClass.Builder> apiClassBuilderMap, String name) {
        ApiClass.Builder builder = apiClassBuilderMap.get(name);
        if (builder == null) {
            builder = ApiClass.newBuilder().setName(ClassUtils.getCanonicalName(name));
            apiClassBuilderMap.put(name, builder);
        }
        return builder;
    }

    // internal reference symboles
    private static final HashMap<String, String> sInternalReferenceSymboleMap;

    static {
        sInternalReferenceSymboleMap = new HashMap<String, String>();
        sInternalReferenceSymboleMap.put("__bss_start", "bss");
        sInternalReferenceSymboleMap.put("_end", "initialized data");
        sInternalReferenceSymboleMap.put("_edata", "uninitialized data");
    }

    private static boolean isInternalReferenceSymbol(ReadElf.Symbol sym) {
        String value = sInternalReferenceSymboleMap.get(sym.name);
        if (value == null) {
            return false;
        } else {
            return true;
        }
    }

    private List<String> getDynamicLoadingDependencies(ReadElf elf) throws IOException {
        List<String> depList = new ArrayList<>();
        // check if it does refer to dlopen
        if (elf.getDynamicSymbol("dlopen") != null) {
            List<String> roStrings = elf.getRoStrings();
            for (String str : roStrings) {
                // skip ".so" or less
                if (str.length() < 4) {
                    continue;
                }

                if (str.endsWith(".so")) {
                    // skip itself
                    if (str.contains(getFileName())) {
                        continue;
                    }
                    if (str.contains(" ")) {
                        continue;
                    }
                    if (str.contains("?")) {
                        continue;
                    }
                    if (str.contains("%")) {
                        System.err.println("ToDo getDynamicLoadingDependencies: " + str);
                        continue;
                    }
                    if (str.startsWith("_")) {
                        System.err.println("ToDo getDynamicLoadingDependencies: " + str);
                        continue;
                    }
                    depList.add(str);
                }
            }
        }

        // specific for frameworks/native/opengl/libs/EGL/Loader.cpp load_system_driver()
        if ("libEGL.so".equals(getFileName())) {
            depList.add("libEGL*.so");
            depList.add("libGLESv1_CM*.so");
            depList.add("GLESv2*.so");
        }

        return depList;
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + ApkParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to prase SO file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-pi \t Parses internal methods and fields too. Output will be large when parsing multiple files in a release.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);
            boolean parseInternalApi = argParser.containsOption("pi");

            File aFile = new File(fileName);
            SoParser aParser = new SoParser(aFile);
            aParser.setPackageName(fileName);
            aParser.setParseInternalApi(parseInternalApi);

            if (outputFileName != null) {
                FileOutputStream txtOutput = new FileOutputStream(outputFileName);
                txtOutput.write(
                        TextFormat.printToString(aParser.getAppInfo())
                                .getBytes(Charset.forName("UTF-8")));
                txtOutput.flush();
                txtOutput.close();
            } else {
                System.out.println(TextFormat.printToString(aParser.getAppInfo()));
            }
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(SoParser.class.getSimpleName());
    }
}
