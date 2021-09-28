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
package android.tzdata.mts;

import static org.junit.Assert.assertEquals;

import android.icu.util.VersionInfo;
import android.os.Build;
import android.util.TimeUtils;

import org.junit.Test;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/**
 * Tests concerning version information associated with, or affected by, the time zone data module.
 *
 * <p>Generally we don't want to assert anything too specific here (like exact version), since that
 * would mean more to update every tzdb release. Also, if the module being tested contains an old
 * version then why wouldn't the tests be just as old too?
 */
public class TimeZoneVersionTest {

    private static final File TIME_ZONE_MODULE_VERSION_FILE =
            new File("/apex/com.android.tzdata/etc/tz/tz_version");

    @Test
    public void timeZoneModuleIsCompatibleWithThisRelease() throws Exception {
        String majorVersion = readMajorFormatVersionFromModuleVersionFile();
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.Q) {

            // TODO Hack for master: This ain't Q. Remove this after R devices are behaving
            //  properly.
            if (VersionInfo.ICU_VERSION.getMajor() > 63) {
                // R is 4.x.
                assertEquals("004", majorVersion);
            } else {
                // Q is 3.x.
                assertEquals("003", majorVersion);
            }
        } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
            // R is 4.x.
            assertEquals("004", majorVersion);
        }
    }

    /**
     * Confirms that tzdb version information available via published APIs is consistent.
     */
    @Test
    public void tzdbVersionIsConsistentAcrossApis() throws Exception {
        String tzModuleTzdbVersion = readTzDbVersionFromModuleVersionFile();

        String icu4jTzVersion = android.icu.util.TimeZone.getTZDataVersion();
        assertEquals(tzModuleTzdbVersion, icu4jTzVersion);

        assertEquals(tzModuleTzdbVersion, TimeUtils.getTimeZoneDatabaseVersion());
    }

    /**
     * Reads up to {@code maxBytes} bytes from the specified file. The returned array can be
     * shorter than {@code maxBytes} if the file is shorter.
     */
    private static byte[] readBytes(File file, int maxBytes) throws IOException {
        if (maxBytes <= 0) {
            throw new IllegalArgumentException("maxBytes ==" + maxBytes);
        }

        try (FileInputStream in = new FileInputStream(file)) {
            byte[] max = new byte[maxBytes];
            int bytesRead = in.read(max, 0, maxBytes);
            byte[] toReturn = new byte[bytesRead];
            System.arraycopy(max, 0, toReturn, 0, bytesRead);
            return toReturn;
        }
    }

    private static String readTzDbVersionFromModuleVersionFile() throws IOException {
        byte[] versionBytes = readBytes(TIME_ZONE_MODULE_VERSION_FILE, 13);
        assertEquals(13, versionBytes.length);

        String versionString = new String(versionBytes, StandardCharsets.US_ASCII);
        // Format is: xxx.yyy|zzzzz|...., we want zzzzz
        String[] dataSetVersionComponents = versionString.split("\\|");
        return dataSetVersionComponents[1];
    }

    private static String readMajorFormatVersionFromModuleVersionFile() throws IOException {
        byte[] versionBytes = readBytes(TIME_ZONE_MODULE_VERSION_FILE, 7);
        assertEquals(7, versionBytes.length);

        String versionString = new String(versionBytes, StandardCharsets.US_ASCII);
        // Format is: xxx.yyy|zzzz|.... we want xxx
        String[] dataSetVersionComponents = versionString.split("\\.");
        return dataSetVersionComponents[0];
    }
}
