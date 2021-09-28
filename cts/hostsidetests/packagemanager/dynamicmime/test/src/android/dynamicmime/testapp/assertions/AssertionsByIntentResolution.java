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

package android.dynamicmime.testapp.assertions;

import static android.dynamicmime.common.Constants.ACTIVITY_BOTH;
import static android.dynamicmime.common.Constants.ACTIVITY_BOTH_AND_TYPE;
import static android.dynamicmime.common.Constants.ACTIVITY_FIRST;
import static android.dynamicmime.common.Constants.ACTIVITY_SECOND;
import static android.dynamicmime.common.Constants.ALIAS_BOTH_GROUPS;
import static android.dynamicmime.common.Constants.ALIAS_BOTH_GROUPS_AND_TYPE;
import static android.dynamicmime.common.Constants.GROUP_FIRST;
import static android.dynamicmime.common.Constants.GROUP_SECOND;
import static android.dynamicmime.common.Constants.PACKAGE_HELPER_APP;
import static android.dynamicmime.common.Constants.PACKAGE_TEST_APP;
import static android.dynamicmime.common.Constants.PACKAGE_UPDATE_APP;
import static android.dynamicmime.testapp.util.IntentsResolutionHelper.resolve;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.ArraySet;

import java.util.Set;

public class AssertionsByIntentResolution extends MimeGroupAssertions {
    private final Context mContext;
    private final String mPackageName;

    private AssertionsByIntentResolution(String packageName, Context context) {
        mContext = context;
        mPackageName = packageName;
    }

    @Override
    public void assertMatchedByMimeGroup(String mimeGroup, String mimeType) {
        resolve(mContext, intentSend(mimeType))
                .assertMatched(mPackageName, activity(mimeGroup));
    }

    @Override
    public void assertNotMatchedByMimeGroup(String mimeGroup, String mimeType) {
        resolve(mContext, intentSend(mimeType))
                .assertNotMatched(mPackageName, activity(mimeGroup));
    }

    @Override
    public void assertMimeGroupInternal(String mimeGroup, Set<String> mimeTypes) {
        if (mimeTypes == null) {
            // We can't distinguish empty group from null group via intent resolution
            return;
        }

        // Check MIME group types retrieved from declared intent filter
        assertEquals("Mismatch for " + mimeGroup + " MIME types",
                mimeTypes, getMimeGroup(mimeGroup, mimeTypes));

        // Additionally we check if all types are matched by this MIME group
        for (String mimeType: mimeTypes) {
            assertMatchedByMimeGroup(mimeGroup, mimeType);
        }
    }

    public static MimeGroupAssertions testApp(Context context) {
        return new AssertionsByIntentResolution(PACKAGE_TEST_APP, context);
    }

    public static MimeGroupAssertions helperApp(Context context) {
        return new AssertionsByIntentResolution(PACKAGE_HELPER_APP, context);
    }

    public static MimeGroupAssertions appWithUpdates(Context context) {
        return new AssertionsByIntentResolution(PACKAGE_UPDATE_APP, context);
    }

    private Set<String> getMimeGroup(String mimeGroup, Set<String> mimeTypes) {
        IntentFilter filter = resolve(mContext, intentGroup(mimeGroup, mimeTypes))
                .getAny()
                .filter;

        Set<String> actualTypes = new ArraySet<>(filter.countDataTypes());

        for (int i = 0; i < filter.countDataTypes(); i++) {
            actualTypes.add(appendWildcard(filter.getDataType(i)));
        }

        return actualTypes;
    }

    private Intent intentGroup(String mimeGroup, Set<String> mimeTypes) {
        Intent intent = new Intent(action(mimeGroup));

        if (mimeTypes != null && !mimeTypes.isEmpty()) {
            intent.setType(mimeTypes.iterator().next());
        }

        return intent;
    }

    private static Intent intentSend(String mimeType) {
        Intent sendIntent = new Intent();
        sendIntent.setAction(Intent.ACTION_SEND);
        sendIntent.setType(mimeType);
        return sendIntent;
    }

    private static String appendWildcard(String actualType) {
        int slashpos = actualType.indexOf('/');

        if (slashpos < 0) {
            return actualType + "/*";
        } else {
            return actualType;
        }
    }

    private static String activity(String mimeGroup) {
        switch (mimeGroup) {
            case GROUP_FIRST:
                return ACTIVITY_FIRST;
            case GROUP_SECOND:
                return ACTIVITY_SECOND;
            case ALIAS_BOTH_GROUPS:
                return ACTIVITY_BOTH;
            case ALIAS_BOTH_GROUPS_AND_TYPE:
                return ACTIVITY_BOTH_AND_TYPE;
            default:
                throw new IllegalArgumentException("Unexpected MIME group " + mimeGroup);
        }
    }

    private String action(String mimeGroup) {
        return mPackageName + ".FILTER_INFO_HOOK_"+ mimeGroup;
    }
}
