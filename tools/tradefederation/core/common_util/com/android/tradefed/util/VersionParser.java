/*
 * Copyright (C) 2013 The Android Open Source Project
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
package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil.CLog;

import java.io.File;
import java.io.IOException;

/** Fetch the version of the running tradefed artifacts. */
public class VersionParser {

    public static final String DEFAULT_IMPLEMENTATION_VERSION = "default";
    private static final String VERSION_FILE = "version.txt";
    private static final String TF_MAIN_JAR = "/tradefed.jar";

    public static String fetchVersion() {
        return getPackageVersion();
    }

    /**
     * Version fetcher on the implementation version of the jar files from this class to ensure
     * a match in the classloader.
     */
    private static String getPackageVersion() {
        Package p = VersionParser.class.getPackage();
        if (p != null) {
            String packageVersion = p.getImplementationVersion();
            if (packageVersion != null && !DEFAULT_IMPLEMENTATION_VERSION.equals(packageVersion)) {
                return packageVersion;
            }
        }
        File dir = getTradefedJarDir();
        if (dir != null) {
            File versionFile = new File(dir, VERSION_FILE);
            if (versionFile.exists()) {
                try {
                    String version = FileUtil.readStringFromFile(versionFile);
                    return version.trim();
                } catch (IOException e) {
                    // Failed to fetch version.
                    CLog.e(e);
                }
            }
            CLog.e("Did not find Version file in directory: %s", dir.getAbsolutePath());
        }
        return null;
    }

    private static File getTradefedJarDir() {
        try {
            File f =
                    new File(
                            VersionParser.class
                                    .getProtectionDomain()
                                    .getCodeSource()
                                    .getLocation()
                                    .getPath());
            return f.getParentFile();
        } catch (Exception e) {
            CLog.e("Failed to find TF JAR dir:");
            CLog.e(e);
        }
        return null;
    }
}
