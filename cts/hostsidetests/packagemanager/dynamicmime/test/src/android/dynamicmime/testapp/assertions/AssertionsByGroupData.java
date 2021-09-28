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

import static android.dynamicmime.common.Constants.PACKAGE_HELPER_APP;
import static android.dynamicmime.common.Constants.PACKAGE_UPDATE_APP;

import android.content.Context;

import org.junit.Assert;

import java.util.Set;

public abstract class AssertionsByGroupData extends MimeGroupAssertions {
    @Override
    public void assertMatchedByMimeGroup(String mimeGroup, String mimeType) {
        // We cannot check if type is matched by group via group info from PackageManager
    }

    @Override
    public void assertNotMatchedByMimeGroup(String mimeGroup, String mimeType) {
        // We cannot check if type is matched by group via group info from PackageManager
    }

    @Override
    public void assertMimeGroupInternal(String mimeGroup, Set<String> mimeTypes) {
        Assert.assertEquals(getMimeGroup(mimeGroup), mimeTypes);
    }

    protected abstract Set<String> getMimeGroup(String mimeGroup);

    public static MimeGroupAssertions testApp(Context context) {
        return new TestAppAssertionsByGroupData(context);
    }

    public static MimeGroupAssertions helperApp(Context context) {
        return new AppAssertionsByGroupData(context, PACKAGE_HELPER_APP);
    }

    public static MimeGroupAssertions appWithUpdates(Context context) {
        return new AppAssertionsByGroupData(context, PACKAGE_UPDATE_APP);
    }
}
