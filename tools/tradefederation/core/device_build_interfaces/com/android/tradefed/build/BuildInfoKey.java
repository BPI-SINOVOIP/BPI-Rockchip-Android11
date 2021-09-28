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
package com.android.tradefed.build;

import java.util.HashSet;
import java.util.Set;

/** Class holding enumeration related to build information queries. */
public class BuildInfoKey {

    /**
     * Enum describing all the known file types that can be queried through {@link
     * IBuildInfo#getFile(BuildInfoFileKey)}.
     */
    public enum BuildInfoFileKey {
        DEVICE_IMAGE("device", false),
        USERDATA_IMAGE("userdata", false),
        TESTDIR_IMAGE("testsdir", false),
        BASEBAND_IMAGE("baseband", false),
        BOOTLOADER_IMAGE("bootloader", false),
        OTA_IMAGE("ota", false),
        MKBOOTIMG_IMAGE("mkbootimg", false),
        RAMDISK_IMAGE("ramdisk", false),

        // Root folder directory
        ROOT_DIRECTORY("rootdirectory", false),

        // Externally linked files in the testsdir:
        // ANDROID_HOST_OUT_TESTCASES and ANDROID_TARGET_OUT_TESTCASES are linked in the tests dir
        // of the build info.
        TARGET_LINKED_DIR("target_testcases", false),
        HOST_LINKED_DIR("host_testcases", false),

        // Keys that can hold lists of files.
        PACKAGE_FILES("package_files", true);

        private final String mFileKey;
        private final boolean mCanBeList;

        private BuildInfoFileKey(String fileKey, boolean canBeList) {
            mFileKey = fileKey;
            mCanBeList = canBeList;
        }

        public String getFileKey() {
            return mFileKey;
        }

        public boolean isList() {
            return mCanBeList;
        }

        @Override
        public String toString() {
            return mFileKey;
        }

        /**
         * Convert a key name to its {@link BuildInfoFileKey} if any is found. Returns null
         * otherwise.
         */
        public static BuildInfoFileKey fromString(String keyName) {
            for (BuildInfoFileKey v : BuildInfoFileKey.values()) {
                if (v.getFileKey().equals(keyName)) {
                    return v;
                }
            }
            return null;
        }
    }

    /**
     * Files key that should be shared from a resources build info to all build infos via the {@link
     * IDeviceBuildInfo#getResourcesDir()}.
     */
    public static final Set<BuildInfoFileKey> SHARED_KEY = new HashSet<>();

    static {
        SHARED_KEY.add(BuildInfoFileKey.PACKAGE_FILES);
        SHARED_KEY.add(BuildInfoFileKey.TESTDIR_IMAGE);
        SHARED_KEY.add(BuildInfoFileKey.ROOT_DIRECTORY);
    }
}
