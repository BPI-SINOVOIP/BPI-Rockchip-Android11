/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification;

import java.util.List;
import java.util.stream.Collectors;

public class GameCoreConfiguration {

    public static String FEATURE_STRING = "android.software.gamecore.preview.certified";

    List<CertificationRequirements> mCertificationRequirements;
    List<ApkInfo> mApkInfo;

    public GameCoreConfiguration(
            List<CertificationRequirements> certificationRequirements,
            List<ApkInfo> apkInfo) {
        mCertificationRequirements = certificationRequirements;
        mApkInfo = apkInfo;
        validateConfiguration();
    }

    private void validateConfiguration() {
        if (mCertificationRequirements == null) {
            throw new IllegalArgumentException("Missing certification requirements");
        }
        if (mApkInfo == null) {
            throw new IllegalArgumentException("Missing apk-info");
        }

        List<String> requirementNames =
                getCertificationRequirements().stream()
                        .map(CertificationRequirements::getName)
                        .collect(Collectors.toList());

        List<String> apkNames =
                getApkInfo().stream()
                        .map(ApkInfo::getName)
                        .collect(Collectors.toList());

        List<String> missingApks =
                requirementNames.stream()
                        .filter((it) -> !apkNames.contains(it))
                        .collect(Collectors.toList());

        if (!missingApks.isEmpty()) {
            throw new IllegalArgumentException(
                    "<certification> contains the following apk that does not exists in "
                            + "<apk-info>:\n"
                            + String.join("\n", missingApks));
        }
    }

    public List<CertificationRequirements> getCertificationRequirements() {
        return mCertificationRequirements;
    }

    public List<ApkInfo> getApkInfo() {
        return mApkInfo;
    }

    public CertificationRequirements findCertificationRequirements(String name) {
        return mCertificationRequirements.stream()
                .filter((it) -> it.getName().equals(name))
                .findFirst()
                .orElse(null);
    }
}
