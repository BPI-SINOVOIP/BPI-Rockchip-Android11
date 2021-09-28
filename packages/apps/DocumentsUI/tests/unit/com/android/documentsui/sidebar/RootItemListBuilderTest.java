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

package com.android.documentsui.sidebar;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.base.UserId;
import com.android.documentsui.testing.TestProvidersAccess;

import com.google.common.collect.Lists;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Collections;
import java.util.List;

/**
 * A unit test for {@link RootItemListBuilderTest}.
 */
@RunWith(AndroidJUnit4.class)
@MediumTest
public class RootItemListBuilderTest {

    private static final RootItem DOWNLOADS_DEFAULT_USER =
            new RootItem(TestProvidersAccess.DOWNLOADS, null, false);
    private static final RootItem HAMMY_DEFAULT_USER =
            new RootItem(TestProvidersAccess.HAMMY, null, false);
    private static final RootItem HOME_DEFAULT_USER =
            new RootItem(TestProvidersAccess.HOME, null, false);
    private static final RootItem IMAGE_DEFAULT_USER =
            new RootItem(TestProvidersAccess.IMAGE, null, false);
    private static final RootItem PICKLES_DEFAULT_USER =
            new RootItem(TestProvidersAccess.PICKLES, null, false);
    private static final RootItem SDCARD_DEFAULT_USER =
            new RootItem(TestProvidersAccess.SD_CARD, null, false);

    private static final RootItem DOWNLOADS_OTHER_USER =
            new RootItem(TestProvidersAccess.OtherUser.DOWNLOADS, null, false);
    private static final RootItem HOME_OTHER_USER =
            new RootItem(TestProvidersAccess.OtherUser.HOME, null, false);
    private static final RootItem IMAGE_OTHER_USER =
            new RootItem(TestProvidersAccess.OtherUser.IMAGE, null, false);
    private static final RootItem PICKLES_OTHER_USER =
            new RootItem(TestProvidersAccess.OtherUser.PICKLES, null, false);

    private RootItemListBuilder mBuilder;

    @Test
    public void testGetList_empty() {
        mBuilder = new RootItemListBuilder(
                UserId.DEFAULT_USER, Collections.singletonList(UserId.DEFAULT_USER));

        List<RootItem> result = mBuilder.getList();
        assertThat(result).isEmpty();
    }

    @Test
    public void testGetList_singleUser() {
        mBuilder = new RootItemListBuilder(
                UserId.DEFAULT_USER, Collections.singletonList(UserId.DEFAULT_USER));

        mBuilder.add(HAMMY_DEFAULT_USER);
        mBuilder.add(PICKLES_DEFAULT_USER);
        mBuilder.add(HOME_DEFAULT_USER);

        List<RootItem> result = mBuilder.getList();
        assertThat(result)
                .containsExactly(HAMMY_DEFAULT_USER, PICKLES_DEFAULT_USER, HOME_DEFAULT_USER);
    }

    @Test
    public void testGetList_twoUsers_allMultiProfileRootsMatchOther_defaultUser() {
        mBuilder = new RootItemListBuilder(UserId.DEFAULT_USER,
                Lists.newArrayList(UserId.DEFAULT_USER, TestProvidersAccess.OtherUser.USER_ID));

        mBuilder.add(DOWNLOADS_DEFAULT_USER); // support multi-profile
        mBuilder.add(HAMMY_DEFAULT_USER);
        mBuilder.add(HOME_DEFAULT_USER); // support multi-profile
        mBuilder.add(IMAGE_DEFAULT_USER); // support multi-profile
        mBuilder.add(PICKLES_DEFAULT_USER);

        mBuilder.add(DOWNLOADS_OTHER_USER); // support multi-profile
        mBuilder.add(HOME_OTHER_USER); // support multi-profile
        mBuilder.add(IMAGE_OTHER_USER); // support multi-profile
        mBuilder.add(PICKLES_OTHER_USER);

        List<RootItem> result = mBuilder.getList();
        assertThat(result).containsExactly(
                DOWNLOADS_DEFAULT_USER,
                HAMMY_DEFAULT_USER,
                HOME_DEFAULT_USER,
                IMAGE_DEFAULT_USER,
                PICKLES_DEFAULT_USER,
                PICKLES_OTHER_USER);
    }

    @Test
    public void testGetList_twoUsers_allMultiProfileRootsMatchOther_otherUser() {
        mBuilder = new RootItemListBuilder(TestProvidersAccess.OtherUser.USER_ID,
                Lists.newArrayList(UserId.DEFAULT_USER, TestProvidersAccess.OtherUser.USER_ID));

        mBuilder.add(DOWNLOADS_DEFAULT_USER); // support multi-profile
        mBuilder.add(SDCARD_DEFAULT_USER);
        mBuilder.add(HOME_DEFAULT_USER); // support multi-profile
        mBuilder.add(IMAGE_DEFAULT_USER); // support multi-profile
        mBuilder.add(PICKLES_DEFAULT_USER);

        mBuilder.add(DOWNLOADS_OTHER_USER); // support multi-profile
        mBuilder.add(HOME_OTHER_USER); // support multi-profile
        mBuilder.add(IMAGE_OTHER_USER); // support multi-profile

        List<RootItem> result = mBuilder.getList();
        assertThat(result).containsExactly(
                DOWNLOADS_OTHER_USER,
                SDCARD_DEFAULT_USER,
                HOME_OTHER_USER,
                IMAGE_OTHER_USER,
                PICKLES_DEFAULT_USER);
    }

    @Test
    public void testGetList_twoUsers_defaultUserHasAllMatchingRoots() {
        mBuilder = new RootItemListBuilder(UserId.DEFAULT_USER,
                Lists.newArrayList(UserId.DEFAULT_USER, TestProvidersAccess.OtherUser.USER_ID));

        mBuilder.add(DOWNLOADS_DEFAULT_USER); // support multi-profile
        mBuilder.add(SDCARD_DEFAULT_USER);
        mBuilder.add(HOME_DEFAULT_USER); // support multi-profile
        mBuilder.add(IMAGE_DEFAULT_USER); // support multi-profile
        mBuilder.add(PICKLES_DEFAULT_USER);

        mBuilder.add(IMAGE_OTHER_USER); // support multi-profile

        List<RootItem> result = mBuilder.getList();
        assertThat(result).containsExactly(
                DOWNLOADS_DEFAULT_USER,
                SDCARD_DEFAULT_USER,
                HOME_DEFAULT_USER,
                IMAGE_DEFAULT_USER,
                PICKLES_DEFAULT_USER);
    }

    @Test
    public void testGetList_twoUsers_secondUserFillsUpNonMatchingRoots() {
        mBuilder = new RootItemListBuilder(TestProvidersAccess.OtherUser.USER_ID,
                Lists.newArrayList(UserId.DEFAULT_USER, TestProvidersAccess.OtherUser.USER_ID));

        mBuilder.add(DOWNLOADS_DEFAULT_USER); // support multi-profile
        mBuilder.add(SDCARD_DEFAULT_USER);
        mBuilder.add(HOME_DEFAULT_USER); // support multi-profile
        mBuilder.add(IMAGE_DEFAULT_USER); // support multi-profile
        mBuilder.add(PICKLES_DEFAULT_USER);

        mBuilder.add(IMAGE_OTHER_USER); // support multi-profile

        List<RootItem> result = mBuilder.getList();
        assertThat(result).containsExactlyElementsIn(Lists.newArrayList(
                RootItem.createDummyItem(
                        DOWNLOADS_DEFAULT_USER, TestProvidersAccess.OtherUser.USER_ID),
                SDCARD_DEFAULT_USER,
                RootItem.createDummyItem(HOME_DEFAULT_USER, TestProvidersAccess.OtherUser.USER_ID),
                IMAGE_OTHER_USER,
                PICKLES_DEFAULT_USER));
    }
}
