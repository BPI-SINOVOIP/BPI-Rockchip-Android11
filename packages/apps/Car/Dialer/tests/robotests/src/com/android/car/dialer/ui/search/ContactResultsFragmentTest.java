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

package com.android.car.dialer.ui.search;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.view.View;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.MutableLiveData;

import com.android.car.arch.common.FutureData;
import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.FragmentTestActivity;
import com.android.car.dialer.R;
import com.android.car.dialer.testutils.ShadowAndroidViewModelFactory;
import com.android.car.dialer.ui.common.ContactResultsLiveData;
import com.android.car.dialer.ui.contact.ContactDetailsFragment;
import com.android.car.dialer.ui.contact.ContactDetailsViewModel;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@Config(shadows = {ShadowAndroidViewModelFactory.class})
@RunWith(CarDialerRobolectricTestRunner.class)
public class ContactResultsFragmentTest {

    private static final String INITIAL_SEARCH_QUERY = "";
    private static final String[] DISPLAY_NAMES = {"name1", "name2", "name3"};

    private ContactResultsFragment mContactResultsFragment;
    private FragmentTestActivity mFragmentTestActivity;
    private CarUiRecyclerView mListView;
    private MutableLiveData<List<ContactResultsLiveData.ContactResultListItem>>
            mContactSearchResultsLiveData;
    @Mock
    private ContactResultsViewModel mMockContactResultsViewModel;
    @Mock
    private ContactDetailsViewModel mMockContactDetailsViewModel;
    @Mock
    private Contact mMockContact, mContact1, mContact2, mContact3;
    @Mock
    private ContactResultsLiveData.ContactResultListItem mContactResult1, mContactResult2,
            mContactResult3;
    @Mock
    private PhoneNumber mPhoneNumber;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        InMemoryPhoneBook.init(RuntimeEnvironment.application);
        mContactSearchResultsLiveData = new MutableLiveData<>();
        when(mMockContactResultsViewModel.getContactSearchResults())
                .thenReturn(mContactSearchResultsLiveData);
        when(mMockContactResultsViewModel.getSortOrderLiveData()).thenReturn(
                LiveDataFunctions.dataOf(TelecomUtils.SORT_BY_FIRST_NAME));
        ShadowAndroidViewModelFactory.add(
                ContactResultsViewModel.class, mMockContactResultsViewModel);

        when(mContactResult1.getContact()).thenReturn(mContact1);
        when(mContact1.getDisplayName()).thenReturn(DISPLAY_NAMES[0]);
        when(mContact1.getNumbers()).thenReturn(Collections.singletonList(mPhoneNumber));
        when(mContactResult2.getContact()).thenReturn(mContact2);
        when(mContact2.getDisplayName()).thenReturn(DISPLAY_NAMES[1]);
        when(mContact2.getNumbers()).thenReturn(Collections.singletonList(mPhoneNumber));
        when(mContactResult3.getContact()).thenReturn(mContact3);
        when(mContact3.getDisplayName()).thenReturn(DISPLAY_NAMES[2]);
        when(mContact3.getNumbers()).thenReturn(Collections.singletonList(mPhoneNumber));
    }

    @After
    public void tearDown() {
        InMemoryPhoneBook.tearDown();
    }

    @Test
    public void testDisplaySearchResults_emptyResult() {
        mContactResultsFragment = ContactResultsFragment.newInstance(INITIAL_SEARCH_QUERY);
        setUpFragment();

        assertThat(mListView.findViewHolderForLayoutPosition(0)).isNull();
    }

    @Test
    public void testDisplaySearchResults_multipleResults() {
        mContactSearchResultsLiveData.setValue(
                Arrays.asList(mContactResult1, mContactResult2, mContactResult3));

        mContactResultsFragment = ContactResultsFragment.newInstance(INITIAL_SEARCH_QUERY);
        setUpFragment();

        verifyChildAt(0);
        verifyChildAt(1);
        verifyChildAt(2);
    }

    @Test
    public void testClickSearchResult_showContactDetailPage() {
        mContactSearchResultsLiveData.setValue(
                Arrays.asList(mContactResult1, mContactResult2, mContactResult3));

        MutableLiveData<FutureData<Contact>> contactDetailLiveData = new MutableLiveData<>();
        contactDetailLiveData.setValue(new FutureData<>(false, mMockContact));
        ShadowAndroidViewModelFactory
                .add(ContactDetailsViewModel.class, mMockContactDetailsViewModel);
        when(mMockContactDetailsViewModel.getContactDetails(any()))
                .thenReturn(contactDetailLiveData);

        mContactResultsFragment = ContactResultsFragment.newInstance(INITIAL_SEARCH_QUERY);
        setUpFragment();

        mListView.findViewHolderForLayoutPosition(1).itemView.findViewById(R.id.contact_result)
                .performClick();

        // verify contact detail is shown.
        verifyShowContactDetail();
    }

    private void setUpFragment() {
        mFragmentTestActivity = Robolectric.buildActivity(
                FragmentTestActivity.class).create().resume().get();
        mFragmentTestActivity.setFragment(mContactResultsFragment);

        mListView = mContactResultsFragment.getView().findViewById(R.id.list_view);
        // Set up layout for recyclerView
        mListView.layout(0, 0, 100, 1000);
    }

    private void verifyChildAt(int position) {
        View childView = mListView.findViewHolderForLayoutPosition(position).itemView;

        assertThat(childView).isNotNull();
        assertThat(childView.findViewById(R.id.contact_result).hasOnClickListeners()).isTrue();
        assertThat(((TextView) childView.findViewById(R.id.contact_name)).getText())
                .isEqualTo(DISPLAY_NAMES[position]);
    }

    private void verifyShowContactDetail() {
        FragmentManager manager = mFragmentTestActivity.getSupportFragmentManager();
        String tag = manager.getBackStackEntryAt(manager.getBackStackEntryCount() - 1).getName();
        Fragment fragment = manager.findFragmentByTag(tag);
        assertThat(fragment instanceof ContactDetailsFragment).isTrue();
    }
}
