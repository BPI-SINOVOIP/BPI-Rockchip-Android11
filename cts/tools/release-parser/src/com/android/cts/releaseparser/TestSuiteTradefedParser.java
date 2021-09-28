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

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

public class TestSuiteTradefedParser extends JarParser {
    private static final String TEST_SUITE_INFO_PROPERTIES_FILE = "test-suite-info.properties";
    private static final String KNOWN_FAILURES_XML_FILE = "-known-failures.xml";
    private static final String EXCLUDE_FILTER_TAG = "compatibility:exclude-filter";
    private static final String NAME_TAG = "name";
    private static final String VALUE_TAG = "value";

    private Entry.EntryType mType;
    private List<String> mKnownFailureList;
    private String mName;
    private String mFullname;
    private String mBuildNumber;
    private String mTargetArch;
    private String mVersion;

    public TestSuiteTradefedParser(File file) {
        super(file);
    }

    @Override
    public Entry.EntryType getType() {
        if (mType == null) {
            parseFile();
        }
        return mType;
    }

    public String getName() {
        if (mType == null) {
            parseFile();
        }
        return mName;
    }

    public String getFullName() {
        if (mType == null) {
            parseFile();
        }
        return mFullname;
    }

    public String getBuildNumber() {
        if (mType == null) {
            parseFile();
        }
        return mBuildNumber;
    }

    public String getTargetArch() {
        if (mType == null) {
            parseFile();
        }
        return mTargetArch;
    }

    public String getVersion() {
        if (mType == null) {
            parseFile();
        }
        return mVersion;
    }

    public List<String> getKnownFailureList() {
        if (mKnownFailureList == null) {
            mKnownFailureList = new ArrayList<String>();
            praseKnownFailure();
        }
        return mKnownFailureList;
    }

    private void praseKnownFailure() {
        try {
            ZipFile zip = new ZipFile(getFile());
            try {
                Enumeration<? extends ZipEntry> entries = zip.entries();
                while (entries.hasMoreElements()) {
                    ZipEntry entry = entries.nextElement();

                    if (entry.getName().endsWith(KNOWN_FAILURES_XML_FILE)) {
                        SAXParserFactory spf = SAXParserFactory.newInstance();
                        spf.setNamespaceAware(false);
                        SAXParser saxParser = spf.newSAXParser();
                        InputStream xmlStream = zip.getInputStream(entry);
                        KnownFailuresXmlHandler kfXmlHandler =
                                new KnownFailuresXmlHandler();
                        saxParser.parse(xmlStream, kfXmlHandler);
                        xmlStream.close();
                    }
                }
            } finally {
                zip.close();
            }
        } catch (Exception e) {
            System.err.println(String.format("Cannot praseKnownFailure %s", e.getMessage()));
        }
    }

    private class KnownFailuresXmlHandler extends DefaultHandler {
        @Override
        public void startElement(String uri, String localName, String name, Attributes attributes)
                throws SAXException {
            super.startElement(uri, localName, name, attributes);
            if (EXCLUDE_FILTER_TAG.equals(attributes.getValue(NAME_TAG))) {
                String kfFilter = attributes.getValue(VALUE_TAG).replace(' ', '.');
                mKnownFailureList.add(kfFilter);
            }
        }
    }

    private void parseFile() {
        try {
            ZipFile zip = new ZipFile(getFile());
            try {
                Enumeration<? extends ZipEntry> entries = zip.entries();
                while (entries.hasMoreElements()) {
                    ZipEntry entry = entries.nextElement();

                    if (entry.getName().equals(TEST_SUITE_INFO_PROPERTIES_FILE)) {
                        InputStream inStream = zip.getInputStream(entry);
                        InputStreamReader isReader = new InputStreamReader(inStream, "UTF-8");
                        BufferedReader bfReader = new BufferedReader(isReader);
                        String ln;
                        while((ln = bfReader.readLine()) != null) {
                            String[] tokens = ln.split(" = ");
                            switch (tokens[0]) {
                                case "build_number":
                                    mBuildNumber = tokens[1];
                                    break;
                                case "target_arch":
                                    mTargetArch = tokens[1];
                                    break;
                                case "name":
                                    mName = tokens[1];
                                    break;
                                case "fullname":
                                    mFullname = tokens[1];
                                    break;
                                case "version":
                                    mVersion = tokens[1];
                                    break;
                            }
                        }
                        inStream.close();
                        isReader.close();
                        bfReader.close();
                    }
                }
                mType = Entry.EntryType.TEST_SUITE_TRADEFED;
            } finally {
                zip.close();
            }
        } catch (Exception e) {
            System.err.println(
                    String.format("Cannot %s %s", TEST_SUITE_INFO_PROPERTIES_FILE, e.getMessage()));
            mType = super.getType();
        }
    }
}
