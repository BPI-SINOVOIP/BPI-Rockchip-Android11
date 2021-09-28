/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * {@link DefaultHandler} that builds an empty {@link ApiCoverage} object from scanning
 * TestModule.xml.
 */
class TestModuleConfigHandler extends DefaultHandler {
    private static final String CONFIGURATION_TAG = "configuration";
    private static final String DESCRIPTION_TAG = "description";
    private static final String OPTION_TAG = "option";
    private static final String TARGET_PREPARER_TAG = "target_preparer";
    private static final String TEST_TAG = "test";
    private static final String CLASS_TAG = "class";
    private static final String NAME_TAG = "name";
    private static final String KEY_TAG = "key";
    private static final String VALUE_TAG = "value";
    private static final String MODULE_NAME_TAG = "module-name";
    private static final String GTEST_CLASS_TAG = "com.android.tradefed.testtype.GTest";
    // com.android.compatibility.common.tradefed.testtype.JarHostTest option
    private static final String JAR_TAG = "jar";
    // com.android.tradefed.targetprep.suite.SuiteApkInstaller option
    private static final String TEST_FILE_NAME_TAG = "test-file-name";

    private TestModuleConfig.Builder mTestModuleConfig;
    private TestModuleConfig.TargetPreparer.Builder mTargetPreparer;
    private TestModuleConfig.TestClass.Builder mTestCase;
    private String mModuleName = null;

    TestModuleConfigHandler(String configFileName) {
        mTestModuleConfig = TestModuleConfig.newBuilder();
        mTestCase = null;
        mTargetPreparer = null;
        // Default Module Name is the Config File Name
        mModuleName = configFileName.replaceAll(".config$", "");
    }

    @Override
    public void startElement(String uri, String localName, String name, Attributes attributes)
            throws SAXException {
        super.startElement(uri, localName, name, attributes);

        switch (localName) {
            case CONFIGURATION_TAG:
                if (null != attributes.getValue(DESCRIPTION_TAG)) {
                    mTestModuleConfig.setDescription(attributes.getValue(DESCRIPTION_TAG));
                } else {
                    mTestModuleConfig.setDescription("WARNING: no description.");
                }
                break;
            case TEST_TAG:
                mTestCase = TestModuleConfig.TestClass.newBuilder();
                mTestCase.setTestClass(attributes.getValue(CLASS_TAG));
                break;
            case TARGET_PREPARER_TAG:
                mTargetPreparer = TestModuleConfig.TargetPreparer.newBuilder();
                mTargetPreparer.setTestClass(attributes.getValue(CLASS_TAG));
                break;
            case OPTION_TAG:
                Option.Builder option = Option.newBuilder();
                option.setName(attributes.getValue(NAME_TAG));
                option.setValue(attributes.getValue(VALUE_TAG));
                String keyStr = attributes.getValue(KEY_TAG);
                if (null != keyStr) {
                    option.setKey(keyStr);
                }
                if (null != mTestCase) {
                    mTestCase.addOptions(option);
                    switch (option.getName()) {
                        case JAR_TAG:
                            mTestModuleConfig.addTestJars(option.getValue());
                            break;
                        case GTEST_CLASS_TAG:
                            mModuleName = option.getValue();
                            break;
                    }
                } else if (null != mTargetPreparer) {
                    mTargetPreparer.addOptions(option);
                    if (TEST_FILE_NAME_TAG.equalsIgnoreCase(option.getName())) {
                        mTestModuleConfig.addTestFileNames(option.getValue());
                    }
                }
                break;
        }
    }

    @Override
    public void endElement(String uri, String localName, String name) throws SAXException {
        super.endElement(uri, localName, name);
        switch (localName) {
            case CONFIGURATION_TAG:
                mTestModuleConfig.setModuleName(mModuleName);
                break;
            case TARGET_PREPARER_TAG:
                mTestModuleConfig.addTargetPreparers(mTargetPreparer);
                mTargetPreparer = null;
                break;
            case TEST_TAG:
                mTestModuleConfig.addTestClasses(mTestCase);
                mTestCase = null;
                break;
        }
    }

    public String getModuleName() {
        return mModuleName;
    }

    public String getTestClassName() {
        //return the 1st Test Class
        return mTestModuleConfig.getTestClassesList().get(0).getTestClass();
    }

    public TestModuleConfig getTestModuleConfig() {
        return mTestModuleConfig.build();
    }
}
