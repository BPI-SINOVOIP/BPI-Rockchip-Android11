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

package com.android.documentsui;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.net.Uri;
import android.os.UserHandle;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.InstrumentationRegistry;

import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestProvidersAccess;

import com.google.android.collect.Lists;
import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Test;

import java.util.Collections;
import java.util.List;

public class ProfileTabsTest {

    private final UserId systemUser = UserId.of(UserHandle.SYSTEM);
    private final UserId managedUser = UserId.of(100);

    private ProfileTabs mProfileTabs;

    private Context mContext;
    private TabLayout mTabLayout;
    private TestEnvironment mTestEnv;
    private State mState;
    private TestUserIdManager mTestUserIdManager;
    private TestCommonAddons mTestCommonAddons;
    private boolean mIsListenerInvoked;

    @Before
    public void setUp() {
        TestEnv.create();
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mContext.setTheme(R.style.DocumentsTheme);
        mContext.getTheme().applyStyle(R.style.DocumentsDefaultTheme, false);
        LayoutInflater layoutInflater = LayoutInflater.from(mContext);
        mState = new State();
        mState.supportsCrossProfile = true;
        mState.stack.changeRoot(TestProvidersAccess.DOWNLOADS);
        mState.stack.push(TestEnv.FOLDER_0);
        View view = layoutInflater.inflate(R.layout.directory_header, null);

        mTabLayout = view.findViewById(R.id.tabs);
        mTestEnv = new TestEnvironment();
        mTestEnv.isSearchExpanded = false;

        mTestUserIdManager = new TestUserIdManager();
        mTestCommonAddons = new TestCommonAddons();
        mTestCommonAddons.mCurrentRoot = TestProvidersAccess.DOWNLOADS;
    }

    @Test
    public void testUpdateView_singleUser_shouldHide() {
        initializeWithUsers(systemUser);

        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
        assertThat(mTabLayout.getTabCount()).isEqualTo(0);
    }

    @Test
    public void testUpdateView_twoUsers_shouldShow() {
        initializeWithUsers(systemUser, managedUser);

        assertThat(mTabLayout.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(mTabLayout.getTabCount()).isEqualTo(2);

        TabLayout.Tab tab1 = mTabLayout.getTabAt(0);
        assertThat(tab1.getTag()).isEqualTo(systemUser);
        assertThat(tab1.getText()).isEqualTo(mContext.getString(R.string.personal_tab));

        TabLayout.Tab tab2 = mTabLayout.getTabAt(1);
        assertThat(tab2.getTag()).isEqualTo(managedUser);
        assertThat(tab2.getText()).isEqualTo(mContext.getString(R.string.work_tab));
    }

    @Test
    public void testUpdateView_twoUsers_doesNotSupportCrossProfile_shouldHide() {
        initializeWithUsers(systemUser, managedUser);

        mState.supportsCrossProfile = false;
        mProfileTabs.updateView();

        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testUpdateView_twoUsers_subFolder_shouldHide() {
        initializeWithUsers(systemUser, managedUser);

        // Push 1 more folder. Now the stack has size of 2.
        mState.stack.push(TestEnv.FOLDER_1);

        mProfileTabs.updateView();
        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
        assertThat(mTabLayout.getTabCount()).isEqualTo(2);
    }

    @Test
    public void testUpdateView_twoUsers_recents_subFolder_shouldHide() {
        initializeWithUsers(systemUser, managedUser);

        mState.stack.changeRoot(TestProvidersAccess.RECENTS);
        // This(stack of size 2 in Recents) may not happen in real world.
        mState.stack.push((TestEnv.FOLDER_0));

        mProfileTabs.updateView();
        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
        assertThat(mTabLayout.getTabCount()).isEqualTo(2);
    }

    @Test
    public void testUpdateView_twoUsers_thirdParty_shouldHide() {
        initializeWithUsers(systemUser, managedUser);

        mState.stack.changeRoot(TestProvidersAccess.PICKLES);
        mState.stack.push((TestEnv.FOLDER_0));

        mProfileTabs.updateView();
        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
        assertThat(mTabLayout.getTabCount()).isEqualTo(2);
    }

    @Test
    public void testUpdateView_twoUsers_isSearching_shouldHide() {
        mTestEnv.isSearchExpanded = true;
        initializeWithUsers(systemUser, managedUser);

        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
        assertThat(mTabLayout.getTabCount()).isEqualTo(2);
    }

    @Test
    public void testUpdateView_getSelectedUser_afterUsersChanged() {
        initializeWithUsers(systemUser, managedUser);
        mProfileTabs.updateView();
        mTabLayout.selectTab(mTabLayout.getTabAt(1));
        assertThat(mTabLayout.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(managedUser);

        mTestUserIdManager.userIds = Collections.singletonList(systemUser);
        mProfileTabs.updateView();
        assertThat(mTabLayout.getVisibility()).isEqualTo(View.GONE);
        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(systemUser);
    }

    @Test
    public void testUpdateView_afterCurrentRootChanged_shouldChangeSelectedUser() {
        initializeWithUsers(systemUser, managedUser);
        mProfileTabs.updateView();

        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(systemUser);

        RootInfo newRoot = RootInfo.copyRootInfo(mTestCommonAddons.mCurrentRoot);
        newRoot.userId = managedUser;
        mTestCommonAddons.mCurrentRoot = newRoot;
        mProfileTabs.updateView();

        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(managedUser);
        // updating view should not trigger listener callback.
        assertThat(mIsListenerInvoked).isFalse();
    }

    @Test
    public void testgetSelectedUser_twoUsers() {
        initializeWithUsers(systemUser, managedUser);

        mTabLayout.selectTab(mTabLayout.getTabAt(0));
        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(systemUser);

        mTabLayout.selectTab(mTabLayout.getTabAt(1));
        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(managedUser);
        assertThat(mIsListenerInvoked).isTrue();
    }

    @Test
    public void testReselectedUser_doesNotInvokeListener() {
        initializeWithUsers(systemUser, managedUser);

        assertThat(mTabLayout.getSelectedTabPosition()).isAtLeast(0);
        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(systemUser);

        mTabLayout.selectTab(mTabLayout.getTabAt(0));
        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(systemUser);
        assertThat(mIsListenerInvoked).isFalse();
    }

    @Test
    public void testgetSelectedUser_singleUsers() {
        initializeWithUsers(systemUser);

        assertThat(mProfileTabs.getSelectedUser()).isEqualTo(systemUser);
    }

    private void initializeWithUsers(UserId... userIds) {
        mTestUserIdManager.userIds = Lists.newArrayList(userIds);
        for (UserId userId : userIds) {
            if (userId.isSystem()) {
                mTestUserIdManager.systemUser = userId;
            } else {
                mTestUserIdManager.managedUser = userId;
            }
        }

        mProfileTabs = new ProfileTabs(mTabLayout, mState, mTestUserIdManager, mTestEnv,
                mTestCommonAddons);
        mProfileTabs.updateView();
        mProfileTabs.setListener(userId -> mIsListenerInvoked = true);
    }

    /**
     * A test implementation of {@link NavigationViewManager.Environment}.
     */
    private static class TestEnvironment implements NavigationViewManager.Environment {

        public boolean isSearchExpanded = false;

        @Override
        public RootInfo getCurrentRoot() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public String getDrawerTitle() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void refreshCurrentRootAndDirectory(int animation) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public boolean isSearchExpanded() {
            return isSearchExpanded;
        }
    }

    private static class TestCommonAddons implements AbstractActionHandler.CommonAddons {

        private RootInfo mCurrentRoot;

        @Override
        public void restoreRootAndDirectory() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void refreshCurrentRootAndDirectory(int anim) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void onRootPicked(RootInfo root) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void onDocumentsPicked(List<DocumentInfo> docs) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void onDocumentPicked(DocumentInfo doc) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public RootInfo getCurrentRoot() {
            return mCurrentRoot;
        }

        @Override
        public DocumentInfo getCurrentDirectory() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public UserId getSelectedUser() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public boolean isInRecents() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void setRootsDrawerOpen(boolean open) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void setRootsDrawerLocked(boolean locked) {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void updateNavigator() {
            throw new UnsupportedOperationException("not implemented");
        }

        @Override
        public void notifyDirectoryNavigated(Uri docUri) {
            throw new UnsupportedOperationException("not implemented");
        }
    }
}

