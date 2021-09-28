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

package android.dynamicmime.testapp.util;

import static android.dynamicmime.common.Constants.PACKAGE_ACTIVITIES;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.hasItem;
import static org.junit.Assert.assertThat;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;

import java.util.List;

/**
 * Helper class for {@link PackageManager#queryIntentActivities} results verification
 */
public class IntentsResolutionHelper {
    private final List<ResolveInfo> resolvedActivities;
    private final Intent mIntent;

    public static IntentsResolutionHelper resolve(Context context, Intent intent) {
        return new IntentsResolutionHelper(context, intent);
    }

    private IntentsResolutionHelper(Context context, Intent intent) {
        mIntent = intent;
        this.resolvedActivities = context.getPackageManager().queryIntentActivities(mIntent, PackageManager.GET_RESOLVED_FILTER);
    }

    public void assertMatched(String packageName, String simpleClassName) {
        assertThat("Missing expected match for " + mIntent, resolvedActivities,
                hasItem(activity(packageName, simpleClassName)));
    }

    public void assertNotMatched(String packageName, String simpleClassName) {
        assertThat("Unexpected match for " + mIntent, resolvedActivities,
                not(hasItem(activity(packageName, simpleClassName))));
    }

    public ResolveInfo getAny() {
        assertThat("Missing match for " + mIntent, resolvedActivities, not(empty()));

        return resolvedActivities
                .stream()
                .findAny()
                .orElse(null);
    }

    private Matcher<ResolveInfo> activity(String packageName, String simpleClassName) {
        return new BaseMatcher<ResolveInfo>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("packageName=" + packageName + ", simpleClassName=" + simpleClassName);
            }

            @Override
            public boolean matches(Object item) {
                if (item == null) {
                    return false;
                }
                ResolveInfo info = (ResolveInfo) item;

                return info.activityInfo.packageName.equals(packageName)
                        && info.activityInfo.name.equals(PACKAGE_ACTIVITIES + simpleClassName);
            }
        };
    }
}
