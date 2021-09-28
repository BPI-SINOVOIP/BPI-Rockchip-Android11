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

package com.android.car.developeroptions.homepage.contextualcards.legacysuggestion;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.service.settings.suggestions.Suggestion;
import android.util.ArrayMap;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.homepage.contextualcards.ContextualCard;
import com.android.car.developeroptions.homepage.contextualcards.ContextualCardController;
import com.android.car.developeroptions.homepage.contextualcards.ContextualCardUpdateListener;
import com.android.car.developeroptions.overlay.FeatureFactory;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import com.android.settingslib.core.lifecycle.events.OnStart;
import com.android.settingslib.core.lifecycle.events.OnStop;
import com.android.settingslib.suggestions.SuggestionController;
import com.android.settingslib.suggestions.SuggestionController.ServiceConnectionListener;
import com.android.settingslib.utils.ThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class LegacySuggestionContextualCardController implements ContextualCardController,
        LifecycleObserver, OnStart, OnStop, ServiceConnectionListener {

    private static final String TAG = "LegacySuggestCardCtrl";

    @VisibleForTesting
    SuggestionController mSuggestionController;

    private ContextualCardUpdateListener mCardUpdateListener;
    private final Context mContext;


    public LegacySuggestionContextualCardController(Context context) {
        mContext = context;
        if (!mContext.getResources().getBoolean(R.bool.config_use_legacy_suggestion)) {
            Log.w(TAG, "Legacy suggestion contextual card disabled, skipping.");
            return;
        }
        final ComponentName suggestionServiceComponent =
                FeatureFactory.getFactory(mContext).getSuggestionFeatureProvider(mContext)
                        .getSuggestionServiceComponent();
        mSuggestionController = new SuggestionController(
                mContext, suggestionServiceComponent, this /* listener */);

    }

    @Override
    public int getCardType() {
        return ContextualCard.CardType.LEGACY_SUGGESTION;
    }

    @Override
    public void onPrimaryClick(ContextualCard card) {
        try {
            ((LegacySuggestionContextualCard) card).getPendingIntent().send();
        } catch (PendingIntent.CanceledException e) {
            Log.w(TAG, "Failed to start suggestion " + card.getTitleText());
        }
    }

    @Override
    public void onActionClick(ContextualCard card) {

    }

    @Override
    public void onDismissed(ContextualCard card) {

    }

    @Override
    public void setCardUpdateListener(ContextualCardUpdateListener listener) {
        mCardUpdateListener = listener;
    }

    @Override
    public void onStart() {
        if (mSuggestionController == null) {
            return;
        }
        mSuggestionController.start();
    }

    @Override
    public void onStop() {
        if (mSuggestionController == null) {
            return;
        }
        mSuggestionController.stop();
    }

    @Override
    public void onServiceConnected() {
        loadSuggestions();
    }

    @Override
    public void onServiceDisconnected() {

    }

    private void loadSuggestions() {
        ThreadUtils.postOnBackgroundThread(() -> {
            if (mSuggestionController == null || mCardUpdateListener == null) {
                return;
            }
            final List<Suggestion> suggestions = mSuggestionController.getSuggestions();
            final String suggestionCount = suggestions == null
                    ? "null"
                    : String.valueOf(suggestions.size());
            Log.d(TAG, "Loaded suggests: " + suggestionCount);

            final List<ContextualCard> cards = new ArrayList<>();
            if (suggestions != null) {
                // Convert suggestion to ContextualCard
                for (Suggestion suggestion : suggestions) {
                    final LegacySuggestionContextualCard.Builder cardBuilder =
                            new LegacySuggestionContextualCard.Builder();
                    if (suggestion.getIcon() != null) {
                        cardBuilder.setIconDrawable(suggestion.getIcon().loadDrawable(mContext));
                    }
                    cardBuilder
                            .setPendingIntent(suggestion.getPendingIntent())
                            .setName(suggestion.getId())
                            .setTitleText(suggestion.getTitle().toString())
                            .setSummaryText(suggestion.getSummary().toString())
                            .setViewType(LegacySuggestionContextualCardRenderer.VIEW_TYPE);

                    cards.add(cardBuilder.build());
                }
            }

            // Update adapter
            final Map<Integer, List<ContextualCard>> suggestionCards = new ArrayMap<>();
            suggestionCards.put(ContextualCard.CardType.LEGACY_SUGGESTION, cards);
            ThreadUtils.postOnMainThread(
                    () -> mCardUpdateListener.onContextualCardUpdated(suggestionCards));

        });
    }
}
