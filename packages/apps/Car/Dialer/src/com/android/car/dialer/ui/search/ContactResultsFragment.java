/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.ui.common.DialerListBaseFragment;
import com.android.car.dialer.ui.contact.ContactDetailsFragment;
import com.android.car.telephony.common.Contact;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.ToolbarController;
import com.android.car.uxr.LifeCycleObserverUxrContentLimiter;
import com.android.car.uxr.UxrContentLimiterImpl;

/**
 * A fragment that will take a search query, look up contacts that match and display those
 * results as a list.
 */
public class ContactResultsFragment extends DialerListBaseFragment implements
        ContactResultsAdapter.OnShowContactDetailListener, Toolbar.OnSearchListener {

    /**
     * Creates a new instance of the {@link ContactResultsFragment}.
     *
     * @param initialSearchQuery An optional search query that will be inputted when the fragment
     *                           starts up.
     */
    public static ContactResultsFragment newInstance(@Nullable String initialSearchQuery) {
        ContactResultsFragment fragment = new ContactResultsFragment();
        Bundle args = new Bundle();
        args.putString(SEARCH_QUERY, initialSearchQuery);
        fragment.setArguments(args);
        return fragment;
    }

    public static final String FRAGMENT_TAG = "ContactResultsFragment";

    private static final String TAG = "CD.ContactResultsFragment";
    private static final String SEARCH_QUERY = "SearchQuery";

    private ContactResultsViewModel mContactResultsViewModel;
    private final ContactResultsAdapter mAdapter = new ContactResultsAdapter(this);

    private RecyclerView.OnScrollListener mOnScrollChangeListener;
    private ToolbarController mToolbar;
    @Nullable
    private RecyclerView mToolbarSearchResultsView;

    private LifeCycleObserverUxrContentLimiter mUxrContentLimiter;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mContactResultsViewModel = ViewModelProviders.of(this).get(
                ContactResultsViewModel.class);
        mContactResultsViewModel.getContactSearchResults().observe(this,
                contactResults -> {
                    mAdapter.setData(contactResults);
                    showContent();
                });
        mContactResultsViewModel.getSortOrderLiveData().observe(this,
                v -> mAdapter.setSortMethod(v));

        // Set the initial search query, if one was provided from a Intent.ACTION_SEARCH
        if (getArguments() != null) {
            String initialQuery = getArguments().getString(SEARCH_QUERY);
            if (!TextUtils.isEmpty(initialQuery)) {
                setSearchQuery(initialQuery);
            }
            getArguments().clear();
        }

        mUxrContentLimiter = new LifeCycleObserverUxrContentLimiter(
                new UxrContentLimiterImpl(getContext(), R.xml.uxr_config));
        getLifecycle().addObserver(mUxrContentLimiter);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        getRecyclerView().setAdapter(mAdapter);

        mOnScrollChangeListener = new RecyclerView.OnScrollListener() {
            @Override
            public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState) {
            }

            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                if (dy != 0) {
                    // Clear the focus to dismiss the keyboard in touch mode.
                    View focusedView = getActivity().getCurrentFocus();
                    if (focusedView != null && focusedView.isInTouchMode()) {
                        focusedView.clearFocus();
                    }
                }
            }
        };
        getRecyclerView().addOnScrollListener(mOnScrollChangeListener);
        if (mToolbarSearchResultsView != null) {
            mToolbarSearchResultsView.setAdapter(mAdapter);
        }

        mUxrContentLimiter.setAdapter(mAdapter);
    }

    @Override
    public void onDestroyView() {
        getRecyclerView().removeOnScrollListener(mOnScrollChangeListener);
        mToolbar.unregisterOnSearchListener(this);
        super.onDestroyView();
    }

    @Override
    protected void setupToolbar(@NonNull ToolbarController toolbar) {
        super.setupToolbar(toolbar);
        mToolbar = toolbar;
        mToolbar.registerOnSearchListener(this);
        mToolbar.setSearchIcon(R.drawable.ic_app_icon);
        setSearchQuery(mContactResultsViewModel.getSearchQuery());

        if (mToolbar.canShowSearchResultsView()) {
            mToolbarSearchResultsView = new RecyclerView(getContext());
            ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mToolbarSearchResultsView.setLayoutParams(params);
            mToolbarSearchResultsView.setLayoutManager(createLayoutManager());
            mToolbarSearchResultsView.setAdapter(mAdapter);
            mToolbarSearchResultsView.setBackground(
                    getContext().getDrawable(R.drawable.car_ui_ime_wide_screen_background));
            mToolbar.setSearchResultsView(mToolbarSearchResultsView);
        }
    }

    /** Sets the search query that should be used to filter contacts. */
    public void setSearchQuery(String query) {
        if (mToolbar != null) {
            mToolbar.setSearchQuery(query);
        } else {
            onSearch(query);
        }
    }

    /** Triggered by search view text change. */
    @Override
    public void onSearch(String newQuery) {
        L.d(TAG, "onSearch: %s", newQuery);
        mContactResultsViewModel.setSearchQuery(newQuery);
    }

    @Override
    public void onShowContactDetail(Contact contact) {
        Fragment contactDetailsFragment = ContactDetailsFragment.newInstance(contact);
        pushContentFragment(contactDetailsFragment, ContactDetailsFragment.FRAGMENT_TAG);
    }

    @Override
    protected Toolbar.State getToolbarState() {
        return Toolbar.State.SEARCH;
    }
}
