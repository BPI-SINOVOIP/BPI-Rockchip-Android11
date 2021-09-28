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

package com.android.settings.intelligence.search.car;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.android.settings.intelligence.search.SearchResult;

/** The ViewHolder for the car search RecyclerView.
 * There are multiple search result types with different UI requirements, such as Intent results
 * and saved query results.
 */
public abstract class CarSearchViewHolder extends RecyclerView.ViewHolder {
    protected Context mContext;
    protected ImageView mIcon;
    protected TextView mTitle;
    protected TextView mSummary;

    public CarSearchViewHolder(View view) {
        super(view);
        mContext = view.getContext();
        mIcon = view.findViewById(android.R.id.icon);
        mTitle = view.findViewById(android.R.id.title);
        mSummary = view.findViewById(android.R.id.summary);
    }

    /**
     * Update the ViewHolder data when bound.
     */
    public abstract void onBind(CarSearchFragment fragment, SearchResult result);
}
