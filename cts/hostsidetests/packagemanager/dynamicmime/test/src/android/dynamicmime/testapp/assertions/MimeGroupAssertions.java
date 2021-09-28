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

import android.content.Context;
import android.util.ArraySet;

import java.util.Collections;
import java.util.Set;

public abstract class MimeGroupAssertions {
    public abstract void assertMatchedByMimeGroup(String mimeGroup, String mimeType);

    public abstract void assertNotMatchedByMimeGroup(String mimeGroup, String mimeType);

    public abstract void assertMimeGroupInternal(String mimeGroup, Set<String> mimeTypes);

    public final void assertMimeGroup(String mimeGroup, String... mimeTypes) {
        assertMimeGroupInternal(mimeGroup, new ArraySet<>(mimeTypes));
    }

    public final void assertMimeGroupIsEmpty(String mimeGroup) {
        assertMimeGroupInternal(mimeGroup, Collections.emptySet());
    }

    public final void assertMimeGroupUndefined(String mimeGroup) {
        assertMimeGroupInternal(mimeGroup, null);
    }

    public final void assertMatchedByMimeGroup(String mimeGroup, String... mimeTypes) {
        for (String mimeType: mimeTypes) {
            assertMatchedByMimeGroup(mimeGroup, mimeType);
        }
    }

    public final void assertNotMatchedByMimeGroup(String mimeGroup, String... mimeTypes) {
        for (String mimeType: mimeTypes) {
            assertNotMatchedByMimeGroup(mimeGroup, mimeType);
        }
    }

    public static MimeGroupAssertions testApp(Context context) {
        return AssertionsByGroupData.testApp(context)
                .mergeWith(AssertionsByIntentResolution.testApp(context));
    }

    public static MimeGroupAssertions helperApp(Context context) {
        return AssertionsByGroupData.helperApp(context)
                .mergeWith(AssertionsByIntentResolution.helperApp(context));
    }

    public static MimeGroupAssertions appWithUpdates(Context context) {
        return AssertionsByGroupData.appWithUpdates(context)
                .mergeWith(AssertionsByIntentResolution.appWithUpdates(context));
    }

    public static MimeGroupAssertions noOp() {
        return new MimeGroupAssertions() {
            @Override
            public void assertMatchedByMimeGroup(String mimeGroup, String mimeType) {
            }

            @Override
            public void assertNotMatchedByMimeGroup(String mimeGroup, String mimeType) {
            }

            @Override
            public void assertMimeGroupInternal(String mimeGroup, Set<String> mimeTypes) {
            }
        };
    }

    public static MimeGroupAssertions notUsed() {
        return new MimeGroupAssertions() {
            @Override
            public void assertMatchedByMimeGroup(String group, String type) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void assertNotMatchedByMimeGroup(String group, String type) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void assertMimeGroupInternal(String group, Set<String> types) {
                throw new UnsupportedOperationException();
            }
        };
    }

    protected final MimeGroupAssertions mergeWith(MimeGroupAssertions other) {
        return new MimeGroupAssertions() {
            @Override
            public void assertMatchedByMimeGroup(String mimeGroup, String mimeType) {
                other.assertMatchedByMimeGroup(mimeGroup, mimeType);
                MimeGroupAssertions.this.assertMatchedByMimeGroup(mimeGroup, mimeType);
            }

            @Override
            public void assertNotMatchedByMimeGroup(String mimeGroup, String mimeType) {
                other.assertNotMatchedByMimeGroup(mimeGroup, mimeType);
                MimeGroupAssertions.this.assertNotMatchedByMimeGroup(mimeGroup, mimeType);
            }

            @Override
            public void assertMimeGroupInternal(String mimeGroup, Set<String> mimeTypes) {
                other.assertMimeGroupInternal(mimeGroup, mimeTypes);
                MimeGroupAssertions.this.assertMimeGroupInternal(mimeGroup, mimeTypes);
            }
        };
    }
}
