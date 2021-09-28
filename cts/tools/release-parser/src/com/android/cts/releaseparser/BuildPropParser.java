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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.logging.Logger;

public class BuildPropParser extends FileParser {
    private Entry.EntryType mType;
    private HashMap<String, String> mProp;

    public BuildPropParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        if (mType == null) {
            parseFile();
        }
        return mType;
    }

    @Override
    public void setAdditionalInfo() {
        Map<String, String> properties = getProperties();
        if (properties != null) {
            getFileEntryBuilder().putAllProperties(properties);
        }
    }

    public String getBuildNumber() {
        return getProperty("ro.build.version.incremental");
    }

    public String getVersion() {
        return getProperty("ro.build.id");
    }

    public String getName() {
        return getProperty("ro.product.device");
    }

    public String getFullName() {
        return getProperty("ro.build.flavor");
    }

    public Map<String, String> getProperties() {
        if (mType == null) {
            parseFile();
        }
        return mProp;
    }

    public String getProperty(String propertyName) {
        if (mType == null) {
            parseFile();
        }
        return mProp.get(propertyName);
    }

    private void parseFile() {
        try {
            FileReader fileReader = new FileReader(getFile());
            BufferedReader buffReader = new BufferedReader(fileReader);
            String line;
            mProp = new HashMap<>();
            while ((line = buffReader.readLine()) != null) {
                String trimLine = line.trim();
                // skips blank lines or comments e.g. # begin build properties
                if (trimLine.length() > 0 && !trimLine.startsWith("#")) {
                    // gets name=value pair, e.g. ro.build.id=PPR1.180610.011
                    String[] phases = trimLine.split("=");
                    if (phases.length > 1) {
                        mProp.put(phases[0], phases[1]);
                    } else {
                        mProp.put(phases[0], "");
                    }
                }
            }
            fileReader.close();
            mType = Entry.EntryType.BUILD_PROP;
        } catch (IOException e) {
            // file is not a Test Module Config
            System.err.println("BuildProp err:" + getFileName() + "\n" + e.getMessage());
            mType = super.getType();
        }
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + BuildPropParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to prase build.prop file meta data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);

            File aFile = new File(fileName);
            BuildPropParser aParser = new BuildPropParser(aFile);
            Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
            writeTextFormatMessage(outputFileName, fileEntryBuilder.build());
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(BuildPropParser.class.getSimpleName());
    }
}
