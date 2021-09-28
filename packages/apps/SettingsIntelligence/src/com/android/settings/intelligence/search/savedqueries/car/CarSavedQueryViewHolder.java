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

import android.view.View;

import com.android.settings.intelligence.R;
import com.android.settings.intelligence.search.car.CarSearchFragment;
import com.android.settings.intelligence.search.car.CarSearchViewHolder;
import com.android.settings.intelligence.search.SearchResult;

/**
 * ViewHolder for saved queries from past searches.
 */
public class CarSavedQueryViewHolder extends CarSearchViewHolder {

    public CarSavedQueryViewHolder(View view) {
        super(view);
    }

    @Override
    public void onBind(CarSearchFragment fragment, SearchResult result) {
        mTitle.setText(result.title);
        mIcon.setImageResource(R.drawable.ic_restore);
        mSummary.setVisibility(View.GONE);
        itemView.setOnClickListener(v -> {
            fragment.onSavedQueryClicked(result.title);
        });
    }
}
