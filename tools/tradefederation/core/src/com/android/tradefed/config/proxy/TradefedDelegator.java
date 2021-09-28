/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.config.proxy;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.Option;

import com.google.common.base.Joiner;

import java.io.File;
import java.io.FileFilter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Objects that helps delegating the invocation to another Tradefed binary. */
public class TradefedDelegator {

    /** The object reference in the configuration. */
    public static final String DELEGATE_OBJECT = "DELEGATE";

    private static final String DELETEGATED_OPTION_NAME = "delegated-tf";

    @Option(
            name = DELETEGATED_OPTION_NAME,
            description =
                    "Points to the root dir of another Tradefed binary that will be used to drive the invocation")
    private File mDelegatedTfRootDir;

    private String[] mCommandLine = null;

    /** Whether or not trigger the delegation logic. */
    public boolean shouldUseDelegation() {
        return mDelegatedTfRootDir != null;
    }

    /** Returns the directory of a Tradefed binary. */
    public File getTfRootDir() {
        return mDelegatedTfRootDir;
    }

    /** Creates the classpath out of the jars in the directory. */
    public String createClasspath() {
        List<File> jars =
                Arrays.asList(
                        mDelegatedTfRootDir.listFiles(
                                new FileFilter() {
                                    @Override
                                    public boolean accept(File pathname) {
                                        return pathname.getName().endsWith(".jar");
                                    }
                                }));
        return Joiner.on(":").join(jars);
    }

    public void setCommandLine(String[] command) {
        mCommandLine = command;
    }

    public String[] getCommandLine() {
        return mCommandLine;
    }

    public static String[] clearCommandline(String[] originalCommand)
            throws ConfigurationException {
        List<String> argsList = new ArrayList<>(Arrays.asList(originalCommand));
        try {
            while (argsList.contains("--" + DELETEGATED_OPTION_NAME)) {
                int index = argsList.indexOf("--" + DELETEGATED_OPTION_NAME);
                if (index != -1) {
                    argsList.remove(index + 1);
                    argsList.remove(index);
                }
            }
        } catch (RuntimeException e) {
            throw new ConfigurationException(e.getMessage(), e);
        }
        return argsList.toArray(new String[0]);
    }
}
