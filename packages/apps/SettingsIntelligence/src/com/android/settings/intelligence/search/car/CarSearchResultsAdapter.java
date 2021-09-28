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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.RecyclerView;

import com.android.settings.intelligence.R;
import com.android.settings.intelligence.search.ResultPayload;
import com.android.settings.intelligence.search.SearchResult;
import com.android.settings.intelligence.search.SearchResultDiffCallback;
import com.android.settings.intelligence.search.savedqueries.car.CarSavedQueryViewHolder;

import java.util.ArrayList;
import java.util.List;

/**
 * RecyclerView Adapter for the car search results RecyclerView.
 * The adapter uses the CarSearchViewHolder for its view contents.
 */
public class CarSearchResultsAdapter extends RecyclerView.Adapter<CarSearchViewHolder> {

    private final CarSearchFragment mFragment;
    private final List<SearchResult> mSearchResults;

    public CarSearchResultsAdapter(CarSearchFragment fragment) {
        mFragment = fragment;
        mSearchResults = new ArrayList<>();

        setHasStableIds(true);
    }

    @Override
    public CarSearchViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        Context context = parent.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        View view;
        switch (viewType) {
            case ResultPayload.PayloadType.INTENT:
                view = inflater.inflate(R.layout.car_ui_preference, parent,
                        /* attachToRoot= */ false);
                return new CarIntentSearchViewHolder(view);
            case ResultPayload.PayloadType.SAVED_QUERY:
                view = inflater.inflate(R.layout.car_ui_preference, parent,
                        /* attachToRoot= */ false);
                return new CarSavedQueryViewHolder(view);
            default:
                return null;
        }
    }

    @Override
    public void onBindViewHolder(CarSearchViewHolder holder, int position) {
        holder.onBind(mFragment, mSearchResults.get(position));
    }

    @Override
    public long getItemId(int position) {
        return mSearchResults.get(position).hashCode();
    }

    @Override
    public int getItemViewType(int position) {
        return mSearchResults.get(position).viewType;
    }

    @Override
    public int getItemCount() {
        return mSearchResults.size();
    }

    protected void postSearchResults(List<? extends SearchResult> newSearchResults) {
        DiffUtil.DiffResult diffResult = DiffUtil.calculateDiff(
                new SearchResultDiffCallback(mSearchResults, newSearchResults));
        mSearchResults.clear();
        mSearchResults.addAll(newSearchResults);
        diffResult.dispatchUpdatesTo(/* adapter= */ this);
    }

    /**
     * Displays recent searched queries.
     */
    public void displaySavedQuery(List<? extends SearchResult> data) {
        clearResults();
        mSearchResults.addAll(data);
        notifyDataSetChanged();
    }

    /**
     * Clear current search results.
     */
    public void clearResults() {
        mSearchResults.clear();
        notifyDataSetChanged();
    }

    /**
     * Get current search results.
     */
    public List<SearchResult> getSearchResults() {
        return mSearchResults;
    }
}
