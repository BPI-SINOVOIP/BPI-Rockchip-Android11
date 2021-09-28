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

package com.android.cts.context;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.UserHandle;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * This suite of test ensures that certain APIs in {@link Context} behaves correctly across users.
 */
@RunWith(JUnit4.class)
public class ContextCrossProfileDeviceTest {
    private static final String INTERACT_ACROSS_USERS_FULL_PERMISSION =
            "android.permission.INTERACT_ACROSS_USERS_FULL";
    private static final String INTERACT_ACROSS_USERS_PERMISSION =
            "android.permission.INTERACT_ACROSS_USERS";
    private static final String INTERACT_ACROSS_PROFILES_PERMISSION =
            "android.permission.INTERACT_ACROSS_PROFILES";
    private static final String MANAGE_APP_OPS_MODE = "android.permission.MANAGE_APP_OPS_MODES";

    private static final String TEST_SERVICE_PKG =
            "com.android.cts.testService";
    private static final String TEST_SERVICE_IN_DIFFERENT_PKG_CLASS =
            TEST_SERVICE_PKG + ".ContextCrossProfileTestService";
    public static final ComponentName TEST_SERVICE_IN_DIFFERENT_PKG_COMPONENT_NAME =
            new ComponentName(TEST_SERVICE_PKG, TEST_SERVICE_IN_DIFFERENT_PKG_CLASS);

    private static final String TEST_SERVICE_IN_SAME_PKG_CLASS =
            InstrumentationRegistry.getContext().getPackageName()
                    + ".ContextCrossProfileSamePackageTestService";
    public static final ComponentName TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME =
            new ComponentName(InstrumentationRegistry.getContext().getPackageName(),
                    TEST_SERVICE_IN_SAME_PKG_CLASS);
    @After
    public void tearDown() throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    private int getTestUser() {
        final Bundle testArguments = InstrumentationRegistry.getArguments();
        if (testArguments.containsKey("testUser")) {
            try {
                return Integer.parseInt(testArguments.getString("testUser"));
            } catch (NumberFormatException ignore) {
            }
        }
        fail("testUser not found.");
        return -1;
    }

    @Test
    public void testBindServiceAsUser_differentUser_bindsServiceToCorrectUser() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_USERS_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_DIFFERENT_PKG_COMPONENT_NAME);

        assertThat(context.bindServiceAsUser(
                bindIntent, new ContextCrossProfileTestConnection(),
                Context.BIND_AUTO_CREATE, otherProfileHandle))
                .isTrue();
        assertThat(context.bindService(
                    bindIntent, new ContextCrossProfileTestConnection(), Context.BIND_AUTO_CREATE))
                .isFalse();
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossUsersPermission_bindsService() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_USERS_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

        assertThat(context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(),
                    Context.BIND_AUTO_CREATE, otherProfileHandle)).isTrue();
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossUsersPermission_bindsService() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_USERS_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_DIFFERENT_PKG_COMPONENT_NAME);

        assertThat(context.bindServiceAsUser(
                bindIntent, new ContextCrossProfileTestConnection(),
                Context.BIND_AUTO_CREATE, otherProfileHandle)).isTrue();
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossProfilesPermission_bindsService() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        final AppOpsManager appOpsManager = context.getSystemService(AppOpsManager.class);
        uiAutomation.adoptShellPermissionIdentity(MANAGE_APP_OPS_MODE);
        appOpsManager.setMode(AppOpsManager.permissionToOp(INTERACT_ACROSS_PROFILES_PERMISSION),
                Binder.getCallingUid(), context.getPackageName(), AppOpsManager.MODE_DEFAULT);
        uiAutomation.dropShellPermissionIdentity();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_PROFILES_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

        assertThat(context.bindServiceAsUser(
                bindIntent, new ContextCrossProfileTestConnection(),
                Context.BIND_AUTO_CREATE, otherProfileHandle)).isTrue();
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossProfilesPermission_throwsException() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_PROFILES_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_DIFFERENT_PKG_COMPONENT_NAME);

        try {
            context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(), Context.BIND_AUTO_CREATE,
                    otherProfileHandle);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossProfilesAppOp_bindsService(){
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        final AppOpsManager appOpsManager = context.getSystemService(AppOpsManager.class);
        uiAutomation.adoptShellPermissionIdentity(MANAGE_APP_OPS_MODE);
        appOpsManager.setMode(AppOpsManager.permissionToOp(INTERACT_ACROSS_PROFILES_PERMISSION),
                Binder.getCallingUid(), context.getPackageName(), AppOpsManager.MODE_ALLOWED);
        uiAutomation.dropShellPermissionIdentity();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

        assertThat(context.bindServiceAsUser(
                bindIntent, new ContextCrossProfileTestConnection(),
                Context.BIND_AUTO_CREATE, otherProfileHandle)).isTrue();
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossProfilesAppOp_throwsException(){
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        final AppOpsManager appOpsManager = context.getSystemService(AppOpsManager.class);
        uiAutomation.adoptShellPermissionIdentity(MANAGE_APP_OPS_MODE);
        appOpsManager.setMode(AppOpsManager.permissionToOp(INTERACT_ACROSS_PROFILES_PERMISSION),
                Binder.getCallingUid(), context.getPackageName(), AppOpsManager.MODE_ALLOWED);
        uiAutomation.dropShellPermissionIdentity();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_DIFFERENT_PKG_COMPONENT_NAME);

        try {
            context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(), Context.BIND_AUTO_CREATE,
                    otherProfileHandle);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_withInteractAcrossUsersPermission_throwsException() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherUserId = getTestUser();
        UserHandle otherUserHandle = UserHandle.of(otherUserId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_USERS_PERMISSION);
        try {
            Intent bindIntent = new Intent();
            bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

            context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(), Context.BIND_AUTO_CREATE,
                    otherUserHandle);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_withInteractAcrossProfilesPermission_throwsException() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherUserId = getTestUser();
        UserHandle otherUserHandle = UserHandle.of(otherUserId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_PROFILES_PERMISSION);
        try {
            Intent bindIntent = new Intent();
            bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

            context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(), Context.BIND_AUTO_CREATE,
                    otherUserHandle);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_withInteractAcrossProfilesAppOp_throwsException(){
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        final AppOpsManager appOpsManager = context.getSystemService(AppOpsManager.class);
        uiAutomation.adoptShellPermissionIdentity(MANAGE_APP_OPS_MODE);
        appOpsManager.setMode(AppOpsManager.permissionToOp(INTERACT_ACROSS_PROFILES_PERMISSION),
                Binder.getCallingUid(), context.getPackageName(), AppOpsManager.MODE_ALLOWED);
        uiAutomation.dropShellPermissionIdentity();
        int otherUserId = getTestUser();
        UserHandle otherUserHandle = UserHandle.of(otherUserId);
        try {
            Intent bindIntent = new Intent();
            bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

            context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(),
                    Context.BIND_AUTO_CREATE, otherUserHandle);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_withNoPermissions_throwsException() {
        final Context context = InstrumentationRegistry.getContext();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        try {
            Intent bindIntent = new Intent();
            bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

            context.bindServiceAsUser(
                    bindIntent, new ContextCrossProfileTestConnection(), Context.BIND_AUTO_CREATE,
                    otherProfileHandle);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testBindServiceAsUser_withInteractAcrossProfilePermission_noAsserts() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        final AppOpsManager appOpsManager = context.getSystemService(AppOpsManager.class);
        uiAutomation.adoptShellPermissionIdentity(MANAGE_APP_OPS_MODE);
        appOpsManager.setMode(AppOpsManager.permissionToOp(INTERACT_ACROSS_PROFILES_PERMISSION),
                Binder.getCallingUid(), context.getPackageName(), AppOpsManager.MODE_DEFAULT);
        uiAutomation.dropShellPermissionIdentity();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_PROFILES_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

        context.bindServiceAsUser(
                bindIntent, new ContextCrossProfileTestConnection(),
                Context.BIND_AUTO_CREATE, otherProfileHandle);
    }

    @Test
    public void testBindServiceAsUser_withInteractAcrossUsersFullPermission_noAsserts() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(
                INTERACT_ACROSS_USERS_FULL_PERMISSION, INTERACT_ACROSS_USERS_PERMISSION);
        Intent bindIntent = new Intent();
        bindIntent.setComponent(TEST_SERVICE_IN_SAME_PKG_COMPONENT_NAME);

        context.bindServiceAsUser(
                bindIntent, new ContextCrossProfileTestConnection(),
                Context.BIND_AUTO_CREATE, otherProfileHandle);
    }

    @Test
    public void testCreateContextAsUser_sameProfileGroup_withInteractAcrossProfilesPermission_throwsException() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_PROFILES_PERMISSION);

        try {
            context.createContextAsUser(otherProfileHandle, /*flags= */0);

            fail("Should throw a Security Exception");
        } catch (SecurityException ignored) {
        }
    }

    @Test
    public void testCreateContextAsUser_sameProfileGroup_withInteractAcrossUsersPermission_createsContext() {
        final Context context = InstrumentationRegistry.getContext();
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        int otherProfileId = getTestUser();
        UserHandle otherProfileHandle = UserHandle.of(otherProfileId);
        uiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_USERS_PERMISSION);

        Context otherProfileContext = context.createContextAsUser(
                otherProfileHandle, /*flags= */0);

        assertThat(otherProfileContext.getUserId()).isEqualTo(otherProfileId);
    }
}
