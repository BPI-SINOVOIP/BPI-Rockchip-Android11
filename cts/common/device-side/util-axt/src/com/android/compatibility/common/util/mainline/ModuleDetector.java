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
package com.android.compatibility.common.util.mainline;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Log;

import java.security.MessageDigest;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.Set;

/**
 * Detects mainline modules.
 */
public class ModuleDetector {
    private static final String LOG_TAG = "MainlineModuleDetector";

    private static final char[] HEX_ARRAY = "0123456789ABCDEF".toCharArray();

    /**
     * Return true if a module is play managed.
     *
     * Example of skipping a test based on mainline modules:
     * <pre>
     *  @Test
     *  public void testPocCVE_1234_5678() throws Exception {
     *      if(!ModuleDetector.moduleIsPlayManaged(
     *          getInstrumentation().getContext().getPackageManager(),
     *          MainlineModule.MEDIA_SOFTWARE_CODEC)) {
     *          doStagefrightTest(R.raw.cve_2018_5882);
     *      }
     *  }
     * </pre>
     */
    public static boolean moduleIsPlayManaged(PackageManager pm, MainlineModule module)
            throws Exception {
        return getPlayManagedModules(pm).contains(module);
    }


    /**
     * Return all play managed mainline modules.
     */
    public static Set<MainlineModule> getPlayManagedModules(PackageManager pm) throws Exception {
        Set<MainlineModule> playManagedModules = new HashSet<>();

        Set<String> packages = new HashSet<>();
        for (PackageInfo info : pm.getInstalledPackages(0)) {
            packages.add(info.packageName);
        }
        for (PackageInfo info : pm.getInstalledPackages(PackageManager.MATCH_APEX)) {
            packages.add(info.packageName);
        }

        for (MainlineModule module : EnumSet.allOf(MainlineModule.class)) {
            if (module.isPlayUpdated && packages.contains(module.packageName)
                    && module.certSHA256.equals(getSignatureDigest(pm, module))) {
                playManagedModules.add(module);
            }
        }
        return playManagedModules;
    }

    private static String getSignatureDigest(PackageManager pm, MainlineModule module)
            throws Exception {
        int flag = PackageManager.GET_SIGNING_CERTIFICATES;
        if (module.moduleType == ModuleType.APEX) {
            flag |= PackageManager.MATCH_APEX;
        }

        PackageInfo packageInfo = pm.getPackageInfo(module.packageName, flag);
        MessageDigest messageDigest = MessageDigest.getInstance("SHA256");
        messageDigest.update(packageInfo.signingInfo.getApkContentsSigners()[0].toByteArray());

        final byte[] digest = messageDigest.digest();
        final int digestLength = digest.length;
        final int charCount = 3 * digestLength - 1;

        final char[] chars = new char[charCount];
        for (int i = 0; i < digestLength; i++) {
            final int byteHex = digest[i] & 0xFF;
            chars[i * 3] = HEX_ARRAY[byteHex >>> 4];
            chars[i * 3 + 1] = HEX_ARRAY[byteHex & 0x0F];
            if (i < digestLength - 1) {
                chars[i * 3 + 2] = ':';
            }
        }

        String ret = new String(chars);
        Log.d(LOG_TAG, "Module: " + module.packageName + " has signature: " + ret);
        return ret;
    }
}
