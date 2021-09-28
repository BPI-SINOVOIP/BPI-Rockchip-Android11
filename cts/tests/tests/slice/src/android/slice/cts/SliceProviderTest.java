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

package android.slice.cts;

import android.app.slice.Slice;
import android.app.slice.SliceSpec;
import android.content.ContentResolver;
import android.net.Uri;
import android.os.Bundle;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.google.android.collect.Lists;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class SliceProviderTest {

    private static final String VALID_AUTHORITY = "android.slice.cts";
    private static final String SUSPICIOUS_AUTHORITY = "com.suspicious.www";
    private static final String ACTION_BLUETOOTH = "/action/bluetooth";
    private static final String VALID_BASE_URI_STRING = "content://" + VALID_AUTHORITY;
    private static final String VALID_ACTION_URI_STRING =
            "content://" + VALID_AUTHORITY + ACTION_BLUETOOTH;
    private static final String SHADY_ACTION_URI_STRING =
            "content://" + SUSPICIOUS_AUTHORITY + ACTION_BLUETOOTH;

    @Rule
    public ActivityTestRule<Launcher> mLauncherActivityTestRule = new ActivityTestRule<>(Launcher.class);

    private Uri validBaseUri = Uri.parse(VALID_BASE_URI_STRING);
    private Uri validActionUri = Uri.parse(VALID_ACTION_URI_STRING);
    private Uri shadyActionUri = Uri.parse(SHADY_ACTION_URI_STRING);

    private ContentResolver mContentResolver;

    @Before
    public void setUp() {
        mContentResolver = mLauncherActivityTestRule.getActivity().getContentResolver();
    }

    @Test
    public void testCallSliceUri_ValidAuthority() {
        doQuery(validActionUri);
    }

    @Test(expected = SecurityException.class)
    public void testCallSliceUri_ShadyAuthority() {
        doQuery(shadyActionUri);
    }

    private Slice doQuery(Uri actionUri) {
        Bundle extras = new Bundle();
        extras.putParcelable("slice_uri", actionUri);
        extras.putParcelableArrayList("supported_specs", Lists.newArrayList(
                    new SliceSpec("androidx.slice.LIST", 1),
                    new SliceSpec("androidx.app.slice.BASIC", 1),
                    new SliceSpec("androidx.slice.BASIC", 1),
                    new SliceSpec("androidx.app.slice.LIST", 1)
            ));
        Bundle result = mContentResolver.call(
                validBaseUri,
                SliceProvider.METHOD_SLICE,
                null,
                extras
        );
        return result.getParcelable(SliceProvider.EXTRA_SLICE);
    }

}
