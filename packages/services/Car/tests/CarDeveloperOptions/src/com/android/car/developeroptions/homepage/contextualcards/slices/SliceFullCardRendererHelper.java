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

package com.android.car.developeroptions.homepage.contextualcards.slices;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.RecyclerView;
import androidx.slice.Slice;
import androidx.slice.widget.SliceView;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.homepage.contextualcards.ContextualCard;
import com.android.car.developeroptions.homepage.contextualcards.logging.ContextualCardLogUtils;
import com.android.car.developeroptions.overlay.FeatureFactory;
import com.android.settingslib.core.instrumentation.MetricsFeatureProvider;

/**
 * Card renderer helper for {@link ContextualCard} built as slice full card.
 */
class SliceFullCardRendererHelper {

    private final Context mContext;

    SliceFullCardRendererHelper(Context context) {
        mContext = context;
    }

    RecyclerView.ViewHolder createViewHolder(View view) {
        return new SliceViewHolder(view);
    }

    void bindView(RecyclerView.ViewHolder holder, ContextualCard card, Slice slice) {
        final SliceViewHolder cardHolder = (SliceViewHolder) holder;
        cardHolder.sliceView.setScrollable(false);
        cardHolder.sliceView.setTag(card.getSliceUri());
        //TODO(b/114009676): We will soon have a field to decide what slice mode we should set.
        cardHolder.sliceView.setMode(SliceView.MODE_LARGE);
        cardHolder.sliceView.setSlice(slice);
        // Set this listener so we can log the interaction users make on the slice
        cardHolder.sliceView.setOnSliceActionListener(
                (eventInfo, sliceItem) -> {
                    final String log = ContextualCardLogUtils.buildCardClickLog(card, eventInfo.rowIndex,
                            eventInfo.actionType, cardHolder.getAdapterPosition());

                    final MetricsFeatureProvider metricsFeatureProvider =
                            FeatureFactory.getFactory(mContext).getMetricsFeatureProvider();

                    metricsFeatureProvider.action(mContext,
                            SettingsEnums.ACTION_CONTEXTUAL_CARD_CLICK, log);
                });

        // Customize slice view for Settings
        cardHolder.sliceView.showTitleItems(true);
        if (card.isLargeCard()) {
            cardHolder.sliceView.showHeaderDivider(true);
            cardHolder.sliceView.showActionDividers(true);
        }
    }

    static class SliceViewHolder extends RecyclerView.ViewHolder {
        public final SliceView sliceView;

        public SliceViewHolder(View view) {
            super(view);
            sliceView = view.findViewById(R.id.slice_view);
        }
    }
}
