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

package com.android.cts.rollback.lib;

import android.content.pm.VersionedPackage;
import android.content.rollback.PackageRollbackInfo;
import android.content.rollback.RollbackInfo;

import com.android.cts.install.lib.TestApp;

import com.google.common.truth.FailureMetadata;
import com.google.common.truth.Subject;
import com.google.common.truth.Truth;

import java.util.ArrayList;
import java.util.List;

/**
 * Subject for asserting things about RollbackInfo instances.
 */
public final class RollbackInfoSubject extends Subject<RollbackInfoSubject, RollbackInfo> {
    /**
     * Asserts something about RollbackInfo.
     */
    public static RollbackInfoSubject assertThat(RollbackInfo rollback) {
        return Truth.assert_().about(rollbacks()).that(rollback);
    }

    /**
     * Gets the subject factory for RollbackInfo.
     */
    public static Subject.Factory<RollbackInfoSubject, RollbackInfo> rollbacks() {
        return SUBJECT_FACTORY;
    }

    private static final Subject.Factory<RollbackInfoSubject, RollbackInfo> SUBJECT_FACTORY =
            new Subject.Factory<RollbackInfoSubject, RollbackInfo>() {
                @Override
                public RollbackInfoSubject createSubject(FailureMetadata fs, RollbackInfo that) {
                    return new RollbackInfoSubject(fs, that);
                }
            };

    private RollbackInfoSubject(FailureMetadata failureMetadata, RollbackInfo subject) {
        super(failureMetadata, subject);
    }

    /**
     * Asserts that the RollbackInfo has given rollbackId.
     */
    public void hasRollbackId(int rollbackId) {
        check().that(getSubject().getRollbackId()).isEqualTo(rollbackId);
    }

    /**
     * Asserts that the RollbackInfo is for a staged rollback.
     */
    public void isStaged() {
        check().that(getSubject().isStaged()).isTrue();
    }

    /**
     * Asserts that the RollbackInfo is not for a staged rollback.
     */
    public void isNotStaged() {
        check().that(getSubject().isStaged()).isFalse();
    }

    /**
     * Asserts that the RollbackInfo contains exactly the list of provided
     * package rollbacks. Though they may be in any order.
     */
    public void packagesContainsExactly(Rollback... expected) {
        List<Rollback> actualPackages = new ArrayList<>();
        for (PackageRollbackInfo info : getSubject().getPackages()) {
            actualPackages.add(new Rollback(info));
        }
        check().that(actualPackages).containsExactly((Object[]) expected);
    }

    /**
     * Asserts that the RollbackInfo contains exactly the list of provided
     * cause packages. Though they may be in any order.
     */
    public void causePackagesContainsExactly(TestApp... causes) {
        List<VersionedPackage> expectedVps = new ArrayList<>();
        for (TestApp cause : causes) {
            expectedVps.add(cause.getVersionedPackage());
        }

        check().that(getSubject().getCausePackages()).containsExactlyElementsIn(expectedVps);
    }
}
