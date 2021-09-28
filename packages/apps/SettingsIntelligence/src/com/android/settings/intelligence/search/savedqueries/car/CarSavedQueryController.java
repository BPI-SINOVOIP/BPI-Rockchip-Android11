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

package com.android.settings.intelligence.search.savedqueries.car;

import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.MenuItem;

import androidx.annotation.NonNull;
import androidx.loader.app.LoaderManager;
import androidx.loader.content.Loader;

import com.android.car.ui.imewidescreen.CarUiImeSearchListItem;
import com.android.car.ui.recyclerview.CarUiContentListItem;
import com.android.car.ui.toolbar.ToolbarController;
import com.android.settings.intelligence.R;
import com.android.settings.intelligence.overlay.FeatureFactory;
import com.android.settings.intelligence.search.SearchCommon;
import com.android.settings.intelligence.search.SearchFeatureProvider;
import com.android.settings.intelligence.search.SearchResult;
import com.android.settings.intelligence.search.car.CarSearchFragment;
import com.android.settings.intelligence.search.car.CarSearchResultsAdapter;
import com.android.settings.intelligence.search.savedqueries.SavedQueryRecorder;
import com.android.settings.intelligence.search.savedqueries.SavedQueryRemover;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper class for managing saved queries.
 */
public class CarSavedQueryController implements LoaderManager.LoaderCallbacks,
        MenuItem.OnMenuItemClickListener {

    private static final String ARG_QUERY = "remove_query";
    private static final String TAG = "CarSearchSavedQueryCtrl";

    private static final int MENU_SEARCH_HISTORY = 1000;

    private final Context mContext;
    private final LoaderManager mLoaderManager;
    private final SearchFeatureProvider mSearchFeatureProvider;
    private final CarSearchResultsAdapter mResultAdapter;
    private ToolbarController mToolbar;
    private CarSearchFragment mFragment;

    public CarSavedQueryController(Context context, LoaderManager loaderManager,
            CarSearchResultsAdapter resultsAdapter, @NonNull ToolbarController toolbar,
            CarSearchFragment fragment) {
        mContext = context;
        mLoaderManager = loaderManager;
        mResultAdapter = resultsAdapter;
        mSearchFeatureProvider = FeatureFactory.get(context)
                .searchFeatureProvider();
        mToolbar = toolbar;
        mFragment = fragment;
    }

    @Override
    public Loader onCreateLoader(int id, Bundle args) {
        switch (id) {
            case SearchCommon.SearchLoaderId.SAVE_QUERY_TASK:
                return new SavedQueryRecorder(mContext, args.getString(ARG_QUERY));
            case SearchCommon.SearchLoaderId.REMOVE_QUERY_TASK:
                return new SavedQueryRemover(mContext);
            case SearchCommon.SearchLoaderId.SAVED_QUERIES:
                return mSearchFeatureProvider.getSavedQueryLoader(mContext);
        }
        return null;
    }

    @Override
    public void onLoadFinished(Loader loader, Object data) {
        switch (loader.getId()) {
            case SearchCommon.SearchLoaderId.REMOVE_QUERY_TASK:
                mLoaderManager.restartLoader(SearchCommon.SearchLoaderId.SAVED_QUERIES,
                        /* args= */ null, /* callback= */ this);
                break;
            case SearchCommon.SearchLoaderId.SAVED_QUERIES:
                if (SearchFeatureProvider.DEBUG) {
                    Log.d(TAG, "Saved queries loaded");
                }
                List<SearchResult> results = (List<SearchResult>) data;
                if (mToolbar.canShowSearchResultItems()) {
                    List<CarUiImeSearchListItem> searchItems = new ArrayList<>();
                    for (SearchResult result : results) {
                        CarUiImeSearchListItem item = new CarUiImeSearchListItem(
                                CarUiContentListItem.Action.ICON);
                        item.setTitle(result.title);
                        item.setIconResId(R.drawable.ic_restore);
                        item.setOnItemClickedListener(
                                v -> mFragment.onSavedQueryClicked(result.title));

                        searchItems.add(item);
                    }
                    mToolbar.setSearchResultItems(searchItems);
                }

                mResultAdapter.displaySavedQuery(results);
                break;
        }
    }

    @Override
    public void onLoaderReset(Loader loader) {
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (item.getItemId() != MENU_SEARCH_HISTORY) {
            return false;
        }
        removeQueries();
        return true;
    }

    /**
     * Save a query to the DB.
     */
    public void saveQuery(String query) {
        Bundle args = new Bundle();
        args.putString(ARG_QUERY, query);
        mLoaderManager.restartLoader(SearchCommon.SearchLoaderId.SAVE_QUERY_TASK, args,
                /* callback= */ this);
    }

    /**
     * Remove all saved queries from the DB.
     */
    public void removeQueries() {
        Bundle args = new Bundle();
        mLoaderManager.restartLoader(SearchCommon.SearchLoaderId.REMOVE_QUERY_TASK, args,
                /* callback= */ this);
    }

    /**
     * Load the saved queries from the DB.
     */
    public void loadSavedQueries() {
        if (SearchFeatureProvider.DEBUG) {
            Log.d(TAG, "loading saved queries");
        }
        mLoaderManager.restartLoader(SearchCommon.SearchLoaderId.SAVED_QUERIES,
                /* args= */ null, /* callback= */ this);
    }
}