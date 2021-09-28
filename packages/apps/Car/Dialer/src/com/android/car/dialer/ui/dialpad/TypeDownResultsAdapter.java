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

package com.android.car.dialer.ui.dialpad;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.android.car.dialer.R;
import com.android.car.dialer.ui.common.OnItemClickedListener;
import com.android.car.dialer.ui.search.ContactResultViewHolder;
import com.android.car.dialer.ui.search.ContactResultsAdapter;

/**
 * An adapter used for type down functionality.
 */
public class TypeDownResultsAdapter extends ContactResultsAdapter {

    private OnItemClickedListener mOnItemClickedListener;

    private int mUnrestrictedItemCount = Integer.MAX_VALUE;

    public TypeDownResultsAdapter() {
        super(null);
    }

    public void setOnItemClickedListener(OnItemClickedListener onItemClickedListener) {
        mOnItemClickedListener = onItemClickedListener;
    }

    /**
     * Sets the unrestricted item count. This sets the limit for when UXR is not enforced.
     */
    public void setUnrestrictedItemCount(int limit) {
        mUnrestrictedItemCount = limit < 0 ? Integer.MAX_VALUE : limit;
    }

    @Override
    public int getUnrestrictedItemCount() {
        return Math.min(mUnrestrictedItemCount, getContactResults().size());
    }

    @Override
    public ContactResultViewHolder onCreateViewHolderImpl(ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.type_down_list_item, parent, false);
        return new ContactResultViewHolder(view, null, mOnItemClickedListener);
    }

    @Override
    public void onBindViewHolderImpl(ContactResultViewHolder holder, int position) {
        holder.bindTypeDownResult(getContactResults().get(position), getSortMethod());
    }

    @Override
    public int getConfigurationId() {
        return R.id.dialpad_type_down_uxr_config;
    }
}
