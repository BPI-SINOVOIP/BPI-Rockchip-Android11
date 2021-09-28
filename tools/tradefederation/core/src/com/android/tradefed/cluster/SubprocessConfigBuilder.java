/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.tradefed.cluster;

import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationUtil;
import com.android.tradefed.result.LegacySubprocessResultsReporter;

import org.kxml2.io.KXmlSerializer;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;

/**
 * Build a wrapper TF config XML for an existing TF config.
 *
 * <p>A wrapper XML allows to enable subprocess reporting on an existing TF config.
 */
public class SubprocessConfigBuilder {
    private static final String INCLUDE_NAME = "include";
    private static final String REPORTER_CLASS = LegacySubprocessResultsReporter.class.getName();
    private static final String OPTION_KEY = "subprocess-report-port";
    private static final String CONFIG_DESCRIPTION = "Cluster Command Launcher config";

    private File mWorkdir;

    private String mOriginalConfig;

    private String mPort;

    public SubprocessConfigBuilder setWorkingDir(File dir) {
        mWorkdir = dir;
        return this;
    }

    public SubprocessConfigBuilder setOriginalConfig(String config) {
        mOriginalConfig = config;
        return this;
    }

    public SubprocessConfigBuilder setPort(String port) {
        mPort = port;
        return this;
    }

    /**
     * Current handling of ATS for the naming of injected config. Exposed so it can be used to align
     * the test harness side.
     */
    public static String createConfigName(String originalConfigName) {
        return "_" + originalConfigName.replace("/", "$") + ".xml";
    }

    public File build() throws IOException {
        // Make a new config name based on the original config name to make it possible to find
        // out the original command line from a modified one.
        // FIXME: Find a better way to preserve the original command line.
        String configName = createConfigName(mOriginalConfig);
        // mOriginalConfig is from another test suite, so its content is hard to know at this
        // time. So it doesn't load mOriginalConfig as IConfiguration and add additional config.
        // Instead, it creates a wrapper config including mOriginalConfig.
        File f = new File(mWorkdir, configName);
        PrintWriter writer = new PrintWriter(f);
        KXmlSerializer serializer = new KXmlSerializer();
        serializer.setOutput(writer);
        serializer.setFeature("http://xmlpull.org/v1/doc/features.html#indent-output", true);
        serializer.startDocument("UTF-8", null);
        serializer.startTag(null, ConfigurationUtil.CONFIGURATION_NAME);
        serializer.attribute(
                null, Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, CONFIG_DESCRIPTION);

        serializer.startTag(null, INCLUDE_NAME);
        serializer.attribute(null, ConfigurationUtil.NAME_NAME, mOriginalConfig);
        serializer.endTag(null, INCLUDE_NAME);

        if (mPort != null) {
            serializer.startTag(null, Configuration.RESULT_REPORTER_TYPE_NAME);
            serializer.attribute(null, ConfigurationUtil.CLASS_NAME, REPORTER_CLASS);

            serializer.startTag(null, ConfigurationUtil.OPTION_NAME);
            serializer.attribute(null, ConfigurationUtil.NAME_NAME, OPTION_KEY);
            serializer.attribute(null, ConfigurationUtil.VALUE_NAME, mPort);
            serializer.endTag(null, ConfigurationUtil.OPTION_NAME);

            serializer.endTag(null, Configuration.RESULT_REPORTER_TYPE_NAME);
        }

        serializer.endTag(null, ConfigurationUtil.CONFIGURATION_NAME);
        serializer.endDocument();

        writer.close();
        return f;
    }
}
