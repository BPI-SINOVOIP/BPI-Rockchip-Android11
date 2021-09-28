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
import com.google.protobuf.TextFormat;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class AndroidManifestParser extends FileParser {
    private static Pattern pattern_element = Pattern.compile("E: (\\S*) ");
    private static Pattern pattern_item =
            Pattern.compile("A: (?:android:)?(\\w*)(?:\\S*)?=(?:\\(\\S*\\s*\\S*\\))?(\\S*)");
    private AppInfo.Builder mAppInfoBuilder;
    private String mElementName;
    private BufferedReader mReader;
    private Map<String, String> mProperties;
    private UsesFeature.Builder mUsesFeatureBuilder;
    private UsesLibrary.Builder mUsesLibraryBuilder;

    public AndroidManifestParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.APK;
    }

    public AppInfo getAppInfo() {
        return getAppInfoBuilder().build();
    }

    public AppInfo.Builder getAppInfoBuilder() {
        if (mAppInfoBuilder == null) {
            prase();
        }
        return mAppInfoBuilder;
    }

    private void prase() {
        mAppInfoBuilder = AppInfo.newBuilder();
        processManifest();
    }

    // build cmd list as: "aapt", "d", "xmltree", fileName, "AndroidManifest.xml"
    private List<String> getAaptCmds(String file) {
        List<String> cmds = new ArrayList<>();
        cmds.add("aapt");
        cmds.add("d");
        cmds.add("xmltree");
        cmds.add(file);
        cmds.add("AndroidManifest.xml");
        return cmds;
    }

    private void processManifest() {
        try {
            // ToDo prasing as android/frameworks/base/tools/aapt/Resource.cpp parsePackage()
            Process process = new ProcessBuilder(getAaptCmds(mFile.getAbsolutePath())).start();
            mReader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            parseElement();
        } catch (Exception ex) {
            System.err.println("Failed to aapt d badging " + mFile.getAbsolutePath());
            ex.printStackTrace();
        }
    }

    private void parseElement() {
        String eleName;
        Matcher matcher;
        String line;
        mProperties = new HashMap<>();
        int indent;

        try {
            while ((line = mReader.readLine()) != null) {
                eleName = getElementName(line);
                if (eleName != null) {
                    mElementName = eleName;
                    parseElementItem();
                }
            }
            mAppInfoBuilder.putAllProperties(mProperties);
        } catch (Exception ex) {
            System.err.println(
                    "Failed to aapt & parse AndroidManifest.xml from: " + mFile.getAbsolutePath());
            ex.printStackTrace();
        }
    }

    private void parseElementItem() throws IOException {
        String eleName;
        Matcher matcher;
        String line;
        int indent;
        while ((line = mReader.readLine()) != null) {
            indent = parseItem(line);
            if (indent == -1) {
                eleName = getElementName(line);
                if (eleName == null) {
                    break;
                }

                switch (mElementName) {
                    case "uses-feature":
                        mAppInfoBuilder.addUsesFeatures(mUsesFeatureBuilder);
                        mUsesFeatureBuilder = null;
                        break;
                    case "uses-library":
                        mAppInfoBuilder.addUsesLibraries(mUsesLibraryBuilder);
                        mUsesLibraryBuilder = null;
                        break;
                }
                mElementName = eleName;
            }
        }
    }

    private int parseItem(String line) {
        String key;
        String value;
        Matcher matcher;
        int indent = line.indexOf("A: ");
        if (indent != -1) {
            matcher = pattern_item.matcher(line);
            if (matcher.find()) {
                int processed = 0;
                key = matcher.group(1);
                value = matcher.group(2).replace("\"", "");

                switch (mElementName) {
                    case "manifest":
                        if (key.equals("package")) {
                            mAppInfoBuilder.setPackageName(value);
                            processed = 1;
                            break;
                        }
                    case "uses-sdk":
                    case "application":
                    case "supports-screens":
                        mProperties.put(key, value);
                        processed = 1;
                        break;
                    case "original-package":
                        if (key.equals("name")) {
                            mProperties.put(mElementName, value);
                            processed = 1;
                        }
                        break;
                    case "uses-permission":
                        if (key.equals("name")) {
                            mAppInfoBuilder.addUsesPermissions(value);
                            processed = 1;
                        }
                        break;
                    case "activity":
                        if (key.equals("name")) {
                            mAppInfoBuilder.addActivities(value);
                            processed = 1;
                        }
                        break;
                    case "service":
                        if (key.equals("name")) {
                            mAppInfoBuilder.addServices(value);
                            processed = 1;
                        }
                        break;
                    case "provider":
                        if (key.equals("name")) {
                            mAppInfoBuilder.addProviders(value);
                            processed = 1;
                        }
                        break;
                    case "uses-feature":
                        putFeature(key, value);
                        processed = 1;
                        break;
                    case "uses-library":
                        putLibrary(key, value);
                        processed = 1;
                        break;
                    default:
                }
                System.out.println(
                        String.format("%s,%s,%s,%d", mElementName, key, value, processed));
            }
        }
        return indent;
    }

    private void putFeature(String key, String value) {
        if (mUsesFeatureBuilder == null) {
            mUsesFeatureBuilder = UsesFeature.newBuilder();
        }
        switch (key) {
            case "name":
                mUsesFeatureBuilder.setName(value);
            case "required":
                mUsesFeatureBuilder.setRequired(value);
        }
    }

    private void putLibrary(String key, String value) {
        if (mUsesLibraryBuilder == null) {
            mUsesLibraryBuilder = UsesLibrary.newBuilder();
        }
        switch (key) {
            case "name":
                mUsesLibraryBuilder.setName(value);
            case "required":
                mUsesLibraryBuilder.setRequired(value);
        }
    }

    private String getElementName(String line) {
        String name;
        Matcher matcher;
        int indent = line.indexOf("E: ");
        if (indent != -1) {
            matcher = pattern_element.matcher(line);
            if (matcher.find()) {
                name = matcher.group(1);
                return name;
            }
        }
        return null;
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar com.android.cts.releaseparser.ApkParser [-options] <path> [args...]\n"
                    + "           to prase an APK for API\n"
                    + "Options:\n"
                    + "\t-i PATH\t APK path \n";

    /** Get the argument or print out the usage and exit. */
    private static void printUsage() {
        System.out.printf(USAGE_MESSAGE);
        System.exit(1);
    }

    /** Get the argument or print out the usage and exit. */
    private static String getExpectedArg(String[] args, int index) {
        if (index < args.length) {
            return args[index];
        } else {
            printUsage();
            return null; // Never will happen because printUsage will call exit(1)
        }
    }

    public static void main(String[] args) throws IOException {
        String apkFileName = null;

        for (int i = 0; i < args.length; i++) {
            if (args[i].startsWith("-")) {
                if ("-i".equals(args[i])) {
                    apkFileName = getExpectedArg(args, ++i);
                }
            }
        }

        if (apkFileName == null) {
            printUsage();
        }

        File apkFile = new File(apkFileName);
        AndroidManifestParser manifestParser = new AndroidManifestParser(apkFile);
        System.out.println();
        System.out.println(TextFormat.printToString(manifestParser.getAppInfo()));
    }
}
