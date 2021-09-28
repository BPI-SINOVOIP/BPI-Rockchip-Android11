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
package com.android.compatibility.common.tradefed.result.suite;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.result.suite.SuiteResultHolder;
import com.android.tradefed.result.suite.XmlSuiteResultFormatter;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Utility class to save a Compatibility run as an XML.
 */
public class CertificationResultXml extends XmlSuiteResultFormatter {

    public static final String SUITE_PLAN_ATTR = "suite_plan";
    private static final String LOG_URL_ATTR = "log_url";
    private static final String REPORT_VERSION_ATTR = "report_version";
    private static final String REFERENCE_URL_ATTR = "reference_url";
    private static final String RESULT_FILE_VERSION = "5.0";
    private static final String SUITE_NAME_ATTR = "suite_name";
    private static final String SUITE_VERSION_ATTR = "suite_version";
    private static final String SUITE_BUILD_ATTR = "suite_build_number";
    private static final String SUITE_VARIANT_ATTR = "suite_variant";

    private String mSuiteName;
    private String mSuiteVersion;
    private String mSuiteVariant;
    private String mSuitePlan;
    private String mSuiteBuild;
    private String mReferenceUrl;
    private String mLogUrl;
    private Map<String, String> mResultAttributes = new HashMap<String, String>();

    /**
     * Empty version of the constructor when loading results.
     */
    public CertificationResultXml() {}

    /** Create an XML report specialized for the Compatibility Test cases. */
    public CertificationResultXml(
            String suiteName,
            String suiteVersion,
            String suiteVariant,
            String suitePlan,
            String suiteBuild,
            String referenceUrl,
            String logUrl,
            Map<String, String> resultAttributes) {
        mSuiteName = suiteName;
        mSuiteVersion = suiteVersion;
        mSuiteVariant = suiteVariant;
        mSuitePlan = suitePlan;
        mSuiteBuild = suiteBuild;
        mReferenceUrl = referenceUrl;
        mLogUrl = logUrl;
        mResultAttributes = resultAttributes;
    }

    /**
     * Add Compatibility specific attributes.
     */
    @Override
    public void addSuiteAttributes(XmlSerializer serializer)
            throws IllegalArgumentException, IllegalStateException, IOException {
        serializer.attribute(NS, SUITE_NAME_ATTR, mSuiteName);
        if (mSuiteVariant != null) {
            serializer.attribute(NS, SUITE_VARIANT_ATTR, mSuiteVariant);
        } else {
            // Default suite_variant to the suite name itself if it's not a special variant.
            serializer.attribute(NS, SUITE_VARIANT_ATTR, mSuiteName);
        }
        serializer.attribute(NS, SUITE_VERSION_ATTR, mSuiteVersion);
        serializer.attribute(NS, SUITE_PLAN_ATTR, mSuitePlan);
        serializer.attribute(NS, SUITE_BUILD_ATTR, mSuiteBuild);
        serializer.attribute(NS, REPORT_VERSION_ATTR, RESULT_FILE_VERSION);

        if (mReferenceUrl != null) {
            serializer.attribute(NS, REFERENCE_URL_ATTR, mReferenceUrl);
        }

        if (mLogUrl != null) {
            serializer.attribute(NS, LOG_URL_ATTR, mLogUrl);
        }

        if (mResultAttributes != null) {
            for (Entry<String, String> entry : mResultAttributes.entrySet()) {
                serializer.attribute(NS, entry.getKey(), entry.getValue());
            }
        }
    }

    @Override
    public void parseSuiteAttributes(XmlPullParser parser, IInvocationContext context)
            throws XmlPullParserException {
        mSuiteName = parser.getAttributeValue(NS, SUITE_NAME_ATTR);
        context.addInvocationAttribute(SUITE_NAME_ATTR, mSuiteName);

        mSuiteVersion = parser.getAttributeValue(NS, SUITE_VERSION_ATTR);
        context.addInvocationAttribute(SUITE_VERSION_ATTR, mSuiteVersion);

        mSuitePlan = parser.getAttributeValue(NS, SUITE_PLAN_ATTR);
        context.addInvocationAttribute(SUITE_PLAN_ATTR, mSuitePlan);

        mSuiteBuild = parser.getAttributeValue(NS, SUITE_BUILD_ATTR);
        context.addInvocationAttribute(SUITE_BUILD_ATTR, mSuiteBuild);

        mReferenceUrl = parser.getAttributeValue(NS, REFERENCE_URL_ATTR);
        if (mReferenceUrl != null) {
            context.addInvocationAttribute(REFERENCE_URL_ATTR, mReferenceUrl);
        }
        mLogUrl = parser.getAttributeValue(NS, LOG_URL_ATTR);
        if (mLogUrl != null) {
            context.addInvocationAttribute(LOG_URL_ATTR, mLogUrl);
        }
    }

    /**
     * Add compatibility specific build info attributes.
     */
    @Override
    public void addBuildInfoAttributes(XmlSerializer serializer, SuiteResultHolder holder)
            throws IllegalArgumentException, IllegalStateException, IOException {
        for (IBuildInfo build : holder.context.getBuildInfos()) {
            for (String key : build.getBuildAttributes().keySet()) {
                if (key.startsWith(getAttributesPrefix())) {
                    String newKey = key.split(getAttributesPrefix())[1];
                    serializer.attribute(NS, newKey, build.getBuildAttributes().get(key));
                }
            }
        }
    }

    /**
     * Parse the information in 'Build' tag.
     */
    @Override
    public void parseBuildInfoAttributes(XmlPullParser parser, IInvocationContext context)
            throws XmlPullParserException {
        for (int index = 0; index < parser.getAttributeCount(); index++) {
            String key = parser.getAttributeName(index);
            String value = parser.getAttributeValue(NS, key);
            context.addInvocationAttribute(key, value);
        }
    }

    /**
     * Returns the compatibility prefix for attributes.
     */
    public String getAttributesPrefix() {
        return "cts:";
    }
}
