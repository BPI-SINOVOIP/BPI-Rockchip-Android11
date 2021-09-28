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

package com.android.cts.managedprofile;

import static android.app.admin.DevicePolicyManager.FLAG_MANAGED_CAN_ACCESS_PARENT;
import static android.app.admin.DevicePolicyManager.FLAG_PARENT_CAN_ACCESS_MANAGED;
import static android.util.DisplayMetrics.DENSITY_DEFAULT;

import static com.android.cts.managedprofile.BaseManagedProfileTest.ADMIN_RECEIVER_COMPONENT;

import static junit.framework.Assert.assertTrue;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.LauncherApps;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.UserHandle;
import android.util.DisplayMetrics;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import static com.google.common.truth.Truth.assertThat;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Collections;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class LauncherAppsTest {

    private static final String INTERACT_ACROSS_USERS_FULL_PERMISSION =
            "android.permission.INTERACT_ACROSS_USERS_FULL";
    private static final String ACCESS_SHORTCUTS_PERMISSION = "android.permission.ACCESS_SHORTCUTS";

    private final Context mContext = InstrumentationRegistry.getContext();

    @After
    public void tearDown() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Test
    public void addDynamicShortcuts() {
        Bitmap bitmap = BitmapFactory.decodeResource(
                mContext.getResources(), R.drawable.black_32x32);
        final Icon icon1 = Icon.createWithBitmap(bitmap);
        final ShortcutInfo s1 = new ShortcutInfo.Builder(mContext, "s1")
                .setShortLabel("shortlabel1")
                .setIcon(icon1)
                .setIntents(new Intent[]{new Intent(Intent.ACTION_VIEW)})
                .build();
        ShortcutManager shortcutManager = mContext.getSystemService(ShortcutManager.class);
        shortcutManager.addDynamicShortcuts(Collections.singletonList(s1));
        shortcutManager.updateShortcuts(Collections.singletonList(s1));
    }

    @Test
    public void removeAllDynamicShortcuts() {
        ShortcutManager shortcutManager = mContext.getSystemService(ShortcutManager.class);
        shortcutManager.removeAllDynamicShortcuts();
    }

    @Test
    public void shortcutIconDrawable_currentToOtherProfile_withUsersFullPermission_isNotNull() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                INTERACT_ACROSS_USERS_FULL_PERMISSION,
                ACCESS_SHORTCUTS_PERMISSION);
        UserHandle otherProfileUserHandle =
                getOtherProfileUserId(InstrumentationRegistry.getArguments());
        Context contextAsUser = mContext.createContextAsUser(otherProfileUserHandle, /* flags */ 0);
        List<ShortcutInfo> shortcuts = getShortcuts(otherProfileUserHandle, contextAsUser);
        LauncherApps launcherApps = contextAsUser.getSystemService(LauncherApps.class);

        Drawable drawable = launcherApps.getShortcutIconDrawable(shortcuts.get(0), DENSITY_DEFAULT);

        assertThat(drawable).isNotNull();
    }

    @Test
    public void shortcutIconDrawable_currentToOtherProfile_withoutUsersFullPermission_isNull() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                INTERACT_ACROSS_USERS_FULL_PERMISSION,
                ACCESS_SHORTCUTS_PERMISSION);
        UserHandle otherProfileUserHandle =
                getOtherProfileUserId(InstrumentationRegistry.getArguments());
        Context contextAsUser = mContext.createContextAsUser(otherProfileUserHandle, /* flags */ 0);
        List<ShortcutInfo> shortcuts = getShortcuts(otherProfileUserHandle, contextAsUser);
        LauncherApps launcherApps = contextAsUser.getSystemService(LauncherApps.class);
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
        InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                ACCESS_SHORTCUTS_PERMISSION);

        Drawable drawable = launcherApps.getShortcutIconDrawable(shortcuts.get(0), DENSITY_DEFAULT);

        assertThat(drawable).isNull();
    }

    private List<ShortcutInfo> getShortcuts(UserHandle otherProfileUserHandle,
            Context contextAsUser) {
        LauncherApps.ShortcutQuery query = new LauncherApps.ShortcutQuery()
                .setPackage(mContext.getPackageName())
                .setQueryFlags(LauncherApps.ShortcutQuery.FLAG_MATCH_DYNAMIC);
        return contextAsUser.getSystemService(LauncherApps.class)
                .getShortcuts(query, otherProfileUserHandle);
    }

    private UserHandle getOtherProfileUserId(Bundle bundle) {
        return UserHandle.of(Integer.parseInt(bundle.getString("otherProfileUserId")));
    }
}
