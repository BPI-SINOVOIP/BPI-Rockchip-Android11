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
package com.android.compatibility.common.tradefed.presubmit;

import static org.junit.Assert.fail;

import com.android.tradefed.config.ConfigurationException;

import com.google.common.collect.ImmutableSet;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

/**
 * Test to check for duplicate files in different jars and prevent the same dependencies of being
 * included several time (which might result in version conflicts).
 */
@RunWith(JUnit4.class)
public class DupFileTest {

    // We ignore directories part of the common java and google packages.
    private static final String[] IGNORE_DIRS =
            new String[] {
                "android/",
                "javax/annotation/",
                "com/google/protobuf/",
                "kotlin/",
                "perfetto/protos/"
            };
    // Temporarily exclude some Tradefed jar while we work on unbundling them.
    private static final Set<String> IGNORE_JARS =
            ImmutableSet.of("tradefed-no-fwk.jar", "tradefed-test-framework.jar");

    /** test if there are duplicate files in different jars. */
    @Test
    public void testDupFilesExist() throws Exception {
        // Get list of jars.
        List<File> jars = getListOfBuiltJars();

        // Create map of files to jars.
        Map<String, List<String>> filesToJars = getMapOfFilesAndJars(jars);

        // Check if there are any files with the same name in diff jars.
        int dupedFiles = 0;
        StringBuilder dupedFilesSummary = new StringBuilder();
        for (Map.Entry<String, List<String>> entry : filesToJars.entrySet()) {
            String file = entry.getKey();
            List<String> jarFiles = entry.getValue();

            if (jarFiles.size() != 1) {
                dupedFiles++;
                dupedFilesSummary.append(file + ": " + jarFiles.toString() + "\n");
            }
        }

        if (dupedFiles != 0) {
            fail(
                    String.format(
                            "%d files are duplicated in different jars:\n%s",
                            dupedFiles, dupedFilesSummary.toString()));
        }
    }

    /** Create map of file to jars */
    private Map<String, List<String>> getMapOfFilesAndJars(List<File> jars) throws IOException {
        Map<String, List<String>> map = new LinkedHashMap<String, List<String>>();
        JarFile jarFile;
        List<String> jarFileList;
        // Map all the files from all the jars.
        for (File jar : jars) {
            if (IGNORE_JARS.contains(jar.getName())) {
                continue;
            }
            jarFile = new JarFile(jar);
            jarFileList = getListOfFiles(jarFile);
            jarFile.close();

            // Add in the jar file to the map.
            for (String file : jarFileList) {
                if (!map.containsKey(file)) {
                    map.put(file, new LinkedList<String>());
                }

                map.get(file).add(jar.getName());
            }
        }
        return map;
    }

    /** Get the list of jars specified in the path. */
    private List<File> getListOfBuiltJars() throws ConfigurationException {
        String classpathStr = System.getProperty("java.class.path");
        if (classpathStr == null) {
            throw new ConfigurationException(
                    "Could not find the classpath property: java.class.path");
        }
        List<File> listOfJars = new ArrayList<File>();
        for (String jar : classpathStr.split(":")) {
            File jarFile = new File(jar);
            if (jarFile.exists()) {
                listOfJars.add(jarFile);
            }
        }
        return listOfJars;
    }

    /** Return the list of files in the jar. */
    private List<String> getListOfFiles(JarFile jar) {
        List<String> files = new ArrayList<String>();
        Enumeration<JarEntry> e = jar.entries();
        while (e.hasMoreElements()) {
            JarEntry entry = e.nextElement();
            String filename = entry.getName();
            if (checkThisFile(filename)) {
                files.add(filename);
            }
        }
        return files;
    }

    /** Check if we should add this file to list of files. We only want to check for classes. */
    private Boolean checkThisFile(String filename) {
        if (!filename.endsWith(".class")) {
            return false;
        }

        for (String skipDir : IGNORE_DIRS) {
            if (filename.startsWith(skipDir)) {
                return false;
            }
        }

        return true;
    }
}
