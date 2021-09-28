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

import android.database.Cursor;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.dialer.ui.common.ContactResultsLiveData;
import com.android.car.dialer.ui.common.DialerUtils;
import com.android.car.telephony.common.Contact;
import com.android.car.ui.recyclerview.ContentLimitingAdapter;

import java.util.ArrayList;
import java.util.List;

/**
 * An adapter that will parse a list of contacts given by a {@link Cursor} that display the
 * results as a list.
 */
public class ContactResultsAdapter extends ContentLimitingAdapter<ContactResultViewHolder> {

    private Integer mSortMethod;

    interface OnShowContactDetailListener {
        void onShowContactDetail(Contact contact);
    }

    private final List<ContactResultsLiveData.ContactResultListItem> mContactResults =
            new ArrayList<>();
    private final OnShowContactDetailListener mOnShowContactDetailListener;
    private LinearLayoutManager mLayoutManager;

    public ContactResultsAdapter(OnShowContactDetailListener onShowContactDetailListener) {
        mOnShowContactDetailListener = onShowContactDetailListener;
    }

    /**
     * Clears all contact results from this adapter.
     */
    public void clear() {
        mContactResults.clear();
        notifyDataSetChanged();
    }

    /**
     * Sets the list of contacts that should be displayed. The given {@link Cursor} can be safely
     * closed after this call.
     */
    public void setData(List<ContactResultsLiveData.ContactResultListItem> data) {
        mContactResults.clear();
        mContactResults.addAll(data);
        // New search result is available, move the window to the head of the new list.
        updateUnderlyingDataChanged(data.size(), 0 /* Jump to the head */);
        notifyDataSetChanged();
    }

    /**
     * Sets the sorting method for the list.
     */
    public void setSortMethod(Integer sortMethod) {
        mSortMethod = sortMethod;
    }

    @Override
    public ContactResultViewHolder onCreateViewHolderImpl(ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.contact_result, parent, false);
        return new ContactResultViewHolder(view, mOnShowContactDetailListener, null);
    }

    @Override
    public void onBindViewHolderImpl(ContactResultViewHolder holder, int position) {
        holder.bindSearchResult(mContactResults.get(position), mSortMethod);
    }

    @Override
    public void onViewRecycledImpl(ContactResultViewHolder holder) {
        holder.recycle();
    }

    @Override
    public int getItemViewTypeImpl(int position) {
        // Only one type of view is created, so no need for an individualized view type.
        return 0;
    }

    @Override
    public int getUnrestrictedItemCount() {
        return mContactResults.size();
    }

    /**
     * Returns contact results.
     */
    public List<ContactResultsLiveData.ContactResultListItem> getContactResults() {
        return mContactResults;
    }

    @Override
    public int getConfigurationId() {
        return R.id.search_result_uxr_config;
    }

    @Override
    public int computeAnchorIndexWhenRestricting() {
        return DialerUtils.getFirstVisibleItemPosition(mLayoutManager);
    }

    @Override
    public void onAttachedToRecyclerView(@NonNull RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);
        mLayoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
    }

    @Override
    public void onDetachedFromRecyclerView(@NonNull RecyclerView recyclerView) {
        mLayoutManager = null;
        super.onDetachedFromRecyclerView(recyclerView);
    }

    /**
     * Returns the sort method.
     */
    public Integer getSortMethod() {
        return mSortMethod;
    }
}
