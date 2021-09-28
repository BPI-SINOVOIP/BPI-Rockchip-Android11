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

import android.content.res.Resources;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.R;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.testing.TestProvidersAccess;

import com.google.common.collect.Lists;
import com.google.common.truth.Correspondence;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * A unit test for {@link UserItemsCombiner}.
 */
@RunWith(AndroidJUnit4.class)
@MediumTest
public class UserItemsCombinerTest {
    private static final UserId PERSONAL_USER = TestProvidersAccess.USER_ID;
    private static final UserId WORK_USER = TestProvidersAccess.OtherUser.USER_ID;

    private static final List<Item> PERSONAL_ITEMS = Lists.newArrayList(
            personalItem("personal 1"),
            personalItem("personal 2"),
            personalItem("personal 3")
    );

    private static final List<Item> WORK_ITEMS = Lists.newArrayList(
            workItem("work 1")
    );

    private static final Correspondence<Item, Item> ITEM_CORRESPONDENCE =
            new Correspondence<Item, Item>() {
                @Override
                public boolean compare(Item actual, Item expected) {
                    return Objects.equals(actual.title, expected.title)
                            && Objects.equals(actual.userId, expected.userId);
                }

                @Override
                public String toString() {
                    return "has same title and userId as in";
                }
            };

    private final State mState = new State();
    private final Resources mResources =
            InstrumentationRegistry.getInstrumentation().getTargetContext().getResources();
    private UserItemsCombiner mCombiner;

    @Before
    public void setUp() {
        mState.canShareAcrossProfile = true;
        mState.supportsCrossProfile = true;
    }

    @Test
    public void testCreatePresentableList_empty() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .setRootListForCurrentUser(Collections.emptyList())
                .setRootListForOtherUser(Collections.emptyList());
        assertThat(mCombiner.createPresentableList()).isEmpty();
    }

    @Test
    public void testCreatePresentableList_currentIsPersonal_personalItemsOnly() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .setRootListForCurrentUser(PERSONAL_ITEMS)
                .setRootListForOtherUser(Collections.emptyList());
        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(PERSONAL_ITEMS)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsWork_personalItemsOnly() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .overrideCurrentUserForTest(WORK_USER)
                .setRootListForCurrentUser(Collections.emptyList())
                .setRootListForOtherUser(PERSONAL_ITEMS);
        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(PERSONAL_ITEMS)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsPersonal_workItemsOnly() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .setRootListForCurrentUser(Collections.emptyList())
                .setRootListForOtherUser(WORK_ITEMS);
        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(WORK_ITEMS)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsWork_workItemsOnly() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .overrideCurrentUserForTest(WORK_USER)
                .setRootListForCurrentUser(WORK_ITEMS)
                .setRootListForOtherUser(Collections.emptyList());
        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(WORK_ITEMS)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsPersonal_personalAndWorkItems() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .setRootListForCurrentUser(PERSONAL_ITEMS)
                .setRootListForOtherUser(WORK_ITEMS);

        List<Item> expected = Lists.newArrayList();
        expected.add(new HeaderItem(mResources.getString(R.string.personal_tab)));
        expected.addAll(PERSONAL_ITEMS);
        expected.add(new HeaderItem(mResources.getString(R.string.work_tab)));
        expected.addAll(WORK_ITEMS);

        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(expected)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsWork_personalAndWorkItems() {
        mCombiner = new UserItemsCombiner(mResources, mState)
                .overrideCurrentUserForTest(WORK_USER)
                .setRootListForCurrentUser(WORK_ITEMS)
                .setRootListForOtherUser(PERSONAL_ITEMS);

        List<Item> expected = Lists.newArrayList();
        expected.add(new HeaderItem(mResources.getString(R.string.personal_tab)));
        expected.addAll(PERSONAL_ITEMS);
        expected.add(new HeaderItem(mResources.getString(R.string.work_tab)));
        expected.addAll(WORK_ITEMS);

        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(expected)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsPersonal_personalAndWorkItems_cannotShare() {
        mState.canShareAcrossProfile = false;
        mCombiner = new UserItemsCombiner(mResources, mState)
                .setRootListForCurrentUser(PERSONAL_ITEMS)
                .setRootListForOtherUser(WORK_ITEMS);

        assertThat(mCombiner.createPresentableList())
                .comparingElementsUsing(ITEM_CORRESPONDENCE)
                .containsExactlyElementsIn(PERSONAL_ITEMS)
                .inOrder();
    }

    @Test
    public void testCreatePresentableList_currentIsWork_personalItemsOnly_cannotShare() {
        mState.canShareAcrossProfile = false;
        mCombiner = new UserItemsCombiner(mResources, mState)
                .overrideCurrentUserForTest(WORK_USER)
                .setRootListForCurrentUser(Collections.emptyList())
                .setRootListForOtherUser(PERSONAL_ITEMS);

        assertThat(mCombiner.createPresentableList()).isEmpty();
    }

    private static TestItem personalItem(String title) {
        return new TestItem(title, PERSONAL_USER);
    }

    private static TestItem workItem(String title) {
        return new TestItem(title, WORK_USER);
    }

    private static class TestItem extends Item {

        TestItem(String title, UserId userId) {
            super(/* layoutId= */ 0, title, /* stringId= */ "", userId);
        }

        @Override
        void bindView(View convertView) {
        }

        @Override
        boolean isRoot() {
            return false;
        }

        @Override
        void open() {
        }

        @Override
        public String toString() {
            return title + "(" + userId + ")";
        }
    }
}
