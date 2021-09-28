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

package com.android.car.dialer.ui.favorite;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.lifecycle.MutableLiveData;

import com.android.car.arch.common.FutureData;
import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.FragmentTestActivity;
import com.android.car.dialer.R;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.testutils.ShadowAndroidViewModelFactory;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import java.util.Arrays;
import java.util.List;

@Config(shadows = {ShadowAndroidViewModelFactory.class})
@RunWith(CarDialerRobolectricTestRunner.class)
public class FavoriteFragmentTest {
    private static final String RAW_NUMBER = "6502530000";

    private FavoriteFragment mFavoriteFragment;
    private FavoriteContactViewHolder mViewHolder;
    @Mock
    private UiCallManager mMockUiCallManager;
    @Mock
    private Contact mMockContact;
    @Mock
    private FavoriteViewModel mMockFavoriteViewModel;
    @Mock
    private PhoneNumber mMockPhoneNumber;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        UiCallManager.set(mMockUiCallManager);

        when(mMockPhoneNumber.getRawNumber()).thenReturn(RAW_NUMBER);
        MutableLiveData<FutureData<List<Object>>> favoriteContacts = new MutableLiveData<>();
        favoriteContacts.setValue(new FutureData<>(false, Arrays.asList(mMockContact)));
        ShadowAndroidViewModelFactory.add(FavoriteViewModel.class, mMockFavoriteViewModel);
        when(mMockFavoriteViewModel.getFavoriteContacts()).thenReturn(favoriteContacts);
        when(mMockFavoriteViewModel.getSortOrderLiveData()).thenReturn(
                LiveDataFunctions.dataOf(TelecomUtils.SORT_BY_FIRST_NAME));

        mFavoriteFragment = FavoriteFragment.newInstance();
        FragmentTestActivity fragmentTestActivity = Robolectric.buildActivity(
                FragmentTestActivity.class).create().resume().get();
        fragmentTestActivity.setFragment(mFavoriteFragment);

        CarUiRecyclerView recyclerView = mFavoriteFragment.getView().findViewById(R.id.list_view);
        // set up layout for recyclerView
        recyclerView.layout(0, 0, 100, 1000);
        mViewHolder = (FavoriteContactViewHolder) recyclerView.findViewHolderForLayoutPosition(0);
    }

    @After
    public void tearDown() {
        UiCallManager.set(null);
    }

    @Test
    public void testOnItemClick_contactHasPrimaryNumber_placeCall() {
        when(mMockContact.getNumbers()).thenReturn(Arrays.asList(mMockPhoneNumber));
        when(mMockContact.hasPrimaryPhoneNumber()).thenReturn(true);
        when(mMockContact.getPrimaryPhoneNumber()).thenReturn(mMockPhoneNumber);

        mViewHolder.itemView.performClick();

        ArgumentCaptor<String> mCaptor = ArgumentCaptor.forClass(String.class);
        verify(mMockUiCallManager).placeCall(mCaptor.capture());
        assertThat(mCaptor.getValue()).isEqualTo(RAW_NUMBER);
    }

    @Test
    public void testOnItemClick_contactHasOnlyOneNumber_placeCall() {
        when(mMockContact.hasPrimaryPhoneNumber()).thenReturn(false);
        when(mMockContact.getNumbers()).thenReturn(Arrays.asList(mMockPhoneNumber));

        mViewHolder.itemView.performClick();

        ArgumentCaptor<String> mCaptor = ArgumentCaptor.forClass(String.class);
        verify(mMockUiCallManager).placeCall(mCaptor.capture());
        assertThat(mCaptor.getValue()).isEqualTo(RAW_NUMBER);
    }

    @Test
    public void testOnItemClick_contactHasMultiNumbers_notPlaceCall() {
        when(mMockContact.hasPrimaryPhoneNumber()).thenReturn(false);
        PhoneNumber otherMockPhoneNumber = mock(PhoneNumber.class);
        when(mMockContact.getNumbers()).thenReturn(
                Arrays.asList(mMockPhoneNumber, otherMockPhoneNumber));

        ShadowLooper.pauseMainLooper();
        mViewHolder.itemView.performClick();

        verify(mMockUiCallManager, never()).placeCall(any());
    }
}
