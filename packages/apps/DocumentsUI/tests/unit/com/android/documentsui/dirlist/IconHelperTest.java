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

package com.android.documentsui.dirlist;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.UserHandle;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.documentsui.ThumbnailCache;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;

import org.junit.Before;
import org.junit.Test;

@SmallTest
public final class IconHelperTest {

    private Context mContext;
    private IconHelper mIconHelper;
    private ThumbnailCache mThumbnailCache = new ThumbnailCache(1000);

    private UserId systemUser = UserId.of(UserHandle.SYSTEM);
    private UserId managedUser = UserId.of(100);

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mIconHelper = new IconHelper(mContext, State.MODE_LIST, /* maybeShowBadge= */ true,
                mThumbnailCache, managedUser);
    }

    @Test
    public void testShouldShowBadge_returnFalse_onSystemUser() {
        assertThat(mIconHelper.shouldShowBadge(systemUser.getIdentifier())).isFalse();
    }

    @Test
    public void testShouldShowBadge_returnTrue_onManagedUser() {
        assertThat(mIconHelper.shouldShowBadge(managedUser.getIdentifier())).isTrue();
    }

    @Test
    public void testShouldShowBadge_returnFalse_onManagedUser_doNotShowBadge() {
        mIconHelper = new IconHelper(mContext, State.MODE_LIST, /* maybeShowBadge= */ false,
                mThumbnailCache, managedUser);
        assertThat(mIconHelper.shouldShowBadge(managedUser.getIdentifier())).isFalse();
    }

    @Test
    public void testShouldShowBadge_returnFalse_onManagedUser_withoutManagedUser() {
        mIconHelper = new IconHelper(mContext, State.MODE_LIST, /* maybeShowBadge= */ true,
                mThumbnailCache, /* managedUser= */ null);
        assertThat(mIconHelper.shouldShowBadge(managedUser.getIdentifier())).isFalse();
    }
}
