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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class RcParser extends FileParser {
    private static String OPTION_CLASS = "class ";
    private static String OPTION_USER = "user ";
    private static String OPTION_GROUP = "group ";
    private static String OPTION_WRITEPID = "writepid ";
    private static String SECTION_SERVICE = "service";
    private static String SECTION_IMPORT = "import";

    private Entry.EntryType mType;
    private List<Service> mServices;
    private List<String> mImportRc;

    public RcParser(File file) {
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
        getFileEntryBuilder().addAllServices(getServiceList());
    }

    @Override
    public List<String> getDependencies() {
        if (mServices == null) {
            parseFile();
        }

        Map<String, Integer> dependencies = new HashMap<>();
        for (Service service : mServices) {
            // skip /, e.g. /system/bin/sh
            try {
                String file = service.getFile().substring(1);
                dependencies.put(file, 1);
            } catch (Exception e) {
                System.err.println(
                        "err Service " + service.getName() + " File:" + service.getFile());
            }
        }

        for (String importRc : mImportRc) {
            // skip /, e.g. /init.usb.rc
            String file = importRc.substring(1);
            dependencies.put(file, 1);
        }

        return new ArrayList<String>(dependencies.keySet());
    }

    @Override
    public String getCodeId() {
        return getFileContentId();
    }

    public List<Service> getServiceList() {
        if (mServices == null) {
            parseFile();
        }
        return mServices;
    }

    public String toString() {
        return toString(mServices);
    }

    public static String toString(List<Service> services) {
        StringBuilder result = new StringBuilder();
        for (Service service : services) {
            result.append(
                    String.format(
                            "%s %s %s %s;",
                            service.getName(),
                            service.getClazz(),
                            service.getUser(),
                            service.getGroup()));
            // System.err.println(String.format("RcParser-toString %s %s %s ", service.getName(),
            // service.getFile(), String.join(" ", service.getArgumentsList())));
        }
        return result.toString();
    }

    private void parseFile() {
        // rc file spec. at android/system/core/init/README.md
        // android/system/core/init/init.cpp?q=CreateParser
        mServices = new ArrayList<Service>();
        mImportRc = new ArrayList<String>();
        try {
            FileReader fileReader = new FileReader(getFile());
            BufferedReader buffReader = new BufferedReader(fileReader);

            String line;
            while ((line = buffReader.readLine()) != null) {
                if (line.startsWith(SECTION_SERVICE)) {
                    parseService(line, buffReader);
                } else if (line.startsWith(SECTION_IMPORT)) {
                    parseImport(line);
                }
            }
            fileReader.close();
            mType = Entry.EntryType.RC;
        } catch (IOException e) {
            // file is not a RC Config
            System.err.println("RcParser err:" + getFileName() + "\n" + e.getMessage());
            mType = super.getType();
        }
        // System.err.println(this.toString());
    }

    private void parseService(String line, BufferedReader buffReader) throws IOException {
        Service.Builder serviceBld = Service.newBuilder();
        String[] phases = line.split("\\s+");
        serviceBld.setName(phases[1]);
        serviceBld.setFile(phases[2]);
        if (phases.length > 3) {
            serviceBld.addAllArguments(Arrays.asList(phases).subList(3, phases.length));
        }
        String sLine;
        while ((sLine = buffReader.readLine()) != null) {
            String sTrimLine = sLine.trim();
            if (sTrimLine.isEmpty()) {
                // End of a service block
                break;
            }
            if (sTrimLine.startsWith("#")) {
                // Skips comment
                continue;
            } else if (sTrimLine.startsWith(OPTION_CLASS)) {
                serviceBld.setClazz(sTrimLine.substring(OPTION_CLASS.length()));
            } else if (sTrimLine.startsWith(OPTION_USER)) {
                serviceBld.setUser(sTrimLine.substring(OPTION_USER.length()));
            } else if (sTrimLine.startsWith(OPTION_GROUP)) {
                serviceBld.setGroup(sTrimLine.substring(OPTION_GROUP.length()));
            } else if (sTrimLine.startsWith(OPTION_WRITEPID)) {
                serviceBld.setGroup(sTrimLine.substring(OPTION_WRITEPID.length()));
            } else {
                serviceBld.addOptions(sTrimLine);
            }
        }
        mServices.add(serviceBld.build());
    }

    private void parseImport(String line) {
        // e.g.: import /init.environ.rc
        String[] phases = line.split(" ");
        mImportRc.add(phases[1]);
    }
}
