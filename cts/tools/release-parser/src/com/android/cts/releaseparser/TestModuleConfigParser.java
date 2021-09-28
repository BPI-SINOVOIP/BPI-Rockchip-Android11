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

import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.XMLReaderFactory;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class TestModuleConfigParser extends FileParser {
    private Entry.EntryType mType;
    private TestModuleConfig mTestModuleConfig;

    public TestModuleConfigParser(File file) {
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
        if (mType == Entry.EntryType.TEST_MODULE_CONFIG) {
            // only if it's actaully a test module config file
            getFileEntryBuilder().setTestModuleConfig(getTestModuleConfig());
        }

    }

    public TestModuleConfig getTestModuleConfig() {
        if (mType == null) {
            parseFile();
        }
        return mTestModuleConfig;
    }

    private void parseFile() {
        try {
            XMLReader xmlReader = XMLReaderFactory.createXMLReader();
            TestModuleConfigHandler testModuleXmlHandler =
                    new TestModuleConfigHandler(getFileName());
            xmlReader.setContentHandler(testModuleXmlHandler);
            FileReader fileReader = new FileReader(getFile());
            xmlReader.parse(new InputSource(fileReader));
            mTestModuleConfig = testModuleXmlHandler.getTestModuleConfig();
            fileReader.close();
            mType = Entry.EntryType.TEST_MODULE_CONFIG;
        } catch (IOException | SAXException e) {
            // file is not a Test Module Config
            System.err.println(
                    "TestModuleConfigParser err:" + getFileName() + "\n" + e.getMessage());
            mType = Entry.EntryType.FILE;
        }
    }
}
