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
 * limitations under the License
 */

package com.android.tests.stagedinstall;

import android.content.pm.PackageInstaller;

import com.google.common.truth.FailureMetadata;
import com.google.common.truth.Subject;
import com.google.common.truth.Truth;

import javax.annotation.Nullable;

final class PackageInstallerSessionInfoSubject extends
        Subject<PackageInstallerSessionInfoSubject, PackageInstaller.SessionInfo> {

    private PackageInstallerSessionInfoSubject(FailureMetadata failureMetadata,
            @Nullable PackageInstaller.SessionInfo subject) {
        super(failureMetadata, subject);
    }

    private static Subject.Factory<PackageInstallerSessionInfoSubject,
            PackageInstaller.SessionInfo> sessions() {
        return new Subject.Factory<PackageInstallerSessionInfoSubject,
                PackageInstaller.SessionInfo>() {
            @Override
            public PackageInstallerSessionInfoSubject createSubject(FailureMetadata failureMetadata,
                    PackageInstaller.SessionInfo session) {
                return new PackageInstallerSessionInfoSubject(failureMetadata, session);
            }
        };
    }

    static PackageInstallerSessionInfoSubject assertThat(
            PackageInstaller.SessionInfo session) {
        return Truth.assertAbout(sessions()).that(session);
    }

    public void isStagedSessionReady() {
        check().withMessage(failureMessage("in state READY")).that(
                getSubject().isStagedSessionReady()).isTrue();
    }

    public void isStagedSessionApplied() {
        check().withMessage(failureMessage("in state APPLIED")).that(
                getSubject().isStagedSessionApplied()).isTrue();
    }

    public void isStagedSessionFailed() {
        check().withMessage(failureMessage("in state FAILED")).that(
                getSubject().isStagedSessionFailed()).isTrue();
    }

    private String failureMessage(String suffix) {
        return String.format("Not true that session %s is %s", subjectAsString(), suffix);
    }

    private String subjectAsString() {
        PackageInstaller.SessionInfo session = getSubject();
        return "{" + "appPackageName = " + session.getAppPackageName() + "; "
                + "sessionId = " + session.getSessionId() + "; "
                + "isStagedSessionReady = " + session.isStagedSessionReady() + "; "
                + "isStagedSessionApplied = " + session.isStagedSessionApplied() + "; "
                + "isStagedSessionFailed = " + session.isStagedSessionFailed() + "; "
                + "stagedSessionErrorMessage = " + session.getStagedSessionErrorMessage() + "}";
    }
}
