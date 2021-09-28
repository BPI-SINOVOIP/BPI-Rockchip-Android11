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

import android.content.pm.PackageManager;
import android.text.TextUtils;
import android.view.View;

import com.android.settings.intelligence.R;
import com.android.settings.intelligence.search.AppSearchResult;
import com.android.settings.intelligence.search.SearchResult;

/**
 * ViewHolder for intent based search results.
 */
public class CarIntentSearchViewHolder extends CarSearchViewHolder {

    public CarIntentSearchViewHolder(View view) {
        super(view);
    }

    @Override
    public void onBind(CarSearchFragment fragment, SearchResult result) {
        mTitle.setText(result.title);
        if (result instanceof AppSearchResult) {
            AppSearchResult appResult = (AppSearchResult) result;
            PackageManager pm = fragment.getActivity().getPackageManager();
            mIcon.setImageDrawable(appResult.info.loadIcon(pm));
        } else {
            mIcon.setImageDrawable(result.icon);
        }
        bindBreadcrumbView(result);

        itemView.setOnClickListener(v -> fragment.onSearchResultClicked(result));
    }

    private void bindBreadcrumbView(SearchResult result) {
        if (result.breadcrumbs == null || result.breadcrumbs.isEmpty()) {
            mSummary.setVisibility(View.GONE);
            return;
        }
        String breadcrumb = result.breadcrumbs.get(0);
        int count = result.breadcrumbs.size();
        for (int i = 1; i < count; i++) {
            breadcrumb = mContext.getString(R.string.search_breadcrumb_connector,
                    breadcrumb, result.breadcrumbs.get(i));
        }
        if (breadcrumb == null || TextUtils.isEmpty(breadcrumb.trim())) {
            mSummary.setVisibility(View.GONE);
        } else {
            mSummary.setText(breadcrumb);
            mSummary.setVisibility(View.VISIBLE);
        }
    }
}
