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

import static com.android.cts.managedprofile.BaseManagedProfileTest.ADMIN_RECEIVER_COMPONENT;
import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * App-side tests for cross-profile sharing
 */
@RunWith(AndroidJUnit4.class)
public class CrossProfileSharingTest {

    @Test
    public void startSwitchToOtherProfileIntent() {
        Intent intent = getSendIntent();
        Context context = InstrumentationRegistry.getContext();
        List<ResolveInfo> resolveInfos =
                context.getPackageManager().queryIntentActivities(intent,
                        PackageManager.MATCH_DEFAULT_ONLY);
        ResolveInfo switchToOtherProfileResolveInfo =
                getSwitchToOtherProfileResolveInfo(resolveInfos);
        assertWithMessage("Could not retrieve the switch to other profile resolve info.")
                .that(switchToOtherProfileResolveInfo)
                .isNotNull();
        ActivityInfo activityInfo = switchToOtherProfileResolveInfo.activityInfo;
        ComponentName componentName =
                new ComponentName(activityInfo.packageName, activityInfo.name);
        Intent switchToOtherProfileIntent = new Intent(intent);
        switchToOtherProfileIntent.setComponent(componentName);
        switchToOtherProfileIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(switchToOtherProfileIntent);
    }

    @Test
    public void startSwitchToOtherProfileIntent_chooser() {
        Intent intent = getSendIntent();
        Context context = InstrumentationRegistry.getContext();
        List<ResolveInfo> resolveInfos =
                context.getPackageManager().queryIntentActivities(intent,
                        PackageManager.MATCH_DEFAULT_ONLY);
        ResolveInfo switchToOtherProfileResolveInfo =
                getSwitchToOtherProfileResolveInfo(resolveInfos);
        assertWithMessage("Could not retrieve the switch to other profile resolve info.")
                .that(switchToOtherProfileResolveInfo)
                .isNotNull();
        ActivityInfo activityInfo = switchToOtherProfileResolveInfo.activityInfo;
        ComponentName componentName =
                new ComponentName(activityInfo.packageName, activityInfo.name);
        Intent chooserIntent = Intent.createChooser(intent, /* title */ null);
        Intent switchToOtherProfileIntent = new Intent(chooserIntent);
        switchToOtherProfileIntent.setComponent(componentName);
        switchToOtherProfileIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(switchToOtherProfileIntent);
    }

    @Test
    public void addCrossProfileIntents()
            throws IntentFilter.MalformedMimeTypeException {
        Context context = InstrumentationRegistry.getContext();
        DevicePolicyManager devicePolicyManager =
                context.getSystemService(DevicePolicyManager.class);
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SEND);
        filter.addCategory(Intent.CATEGORY_DEFAULT);
        filter.addDataType("*/*");
        devicePolicyManager.addCrossProfileIntentFilter(ADMIN_RECEIVER_COMPONENT, filter,
                DevicePolicyManager.FLAG_MANAGED_CAN_ACCESS_PARENT |
                DevicePolicyManager.FLAG_PARENT_CAN_ACCESS_MANAGED);
    }

    @Test
    public void clearCrossProfileIntents() {
        Context context = InstrumentationRegistry.getContext();
        DevicePolicyManager devicePolicyManager =
                context.getSystemService(DevicePolicyManager.class);
        devicePolicyManager.clearCrossProfileIntentFilters(ADMIN_RECEIVER_COMPONENT);
    }

    private Intent getSendIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.putExtra(Intent.EXTRA_TEXT, "test example");
        intent.setType("text/plain");
        return intent;
    }

    private ResolveInfo getSwitchToOtherProfileResolveInfo(List<ResolveInfo> resolveInfos) {
        for (ResolveInfo resolveInfo : resolveInfos) {
            // match == 0 means that this intent actually doesn't match to anything on this profile,
            // meaning it should be on the other side.
            if (resolveInfo.match == 0) {
                return resolveInfo;
            }
        }
        return null;
    }
}
