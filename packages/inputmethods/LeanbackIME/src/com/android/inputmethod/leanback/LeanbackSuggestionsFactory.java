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
 * limitations under the License
 */

package com.android.inputmethod.leanback;

import android.inputmethodservice.InputMethodService;
import android.text.TextUtils;
import android.util.Log;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import java.util.ArrayList;

/**
 * This class is used to get suggestions from LatinIme's suggestion engine based
 * on the current composing word
 */
public class LeanbackSuggestionsFactory {

    private static final String TAG = "LbSuggestionsFactory";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.VERBOSE);

    // mode for suggestions
    private static final int MODE_DEFAULT = 0;
    private static final int MODE_DOMAIN = 1;
    private static final int MODE_AUTO_COMPLETE = 2;

    private InputMethodService mContext;
    private int mNumSuggestions;
    private int mMode;

    // current active suggestions
    private final ArrayList<String> mSuggestions = new ArrayList<String>();

    public LeanbackSuggestionsFactory(InputMethodService context, int maxSuggestions) {
        mContext = context;
        mNumSuggestions = maxSuggestions;
    }

    public void onStartInput(EditorInfo attribute) {
        mMode = MODE_DEFAULT;

        if ((attribute.inputType & EditorInfo.TYPE_TEXT_FLAG_AUTO_COMPLETE) != 0) {
            mMode = MODE_AUTO_COMPLETE;
        }

        switch (LeanbackUtils.getInputTypeClass(attribute)) {
            case EditorInfo.TYPE_CLASS_TEXT:
                switch (LeanbackUtils.getInputTypeVariation(attribute)) {
                    case EditorInfo.TYPE_TEXT_VARIATION_EMAIL_ADDRESS:
                    case EditorInfo.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
                        mMode = MODE_DOMAIN;
                        break;
                }
                break;
        }
    }

    /**
     * call this method in {@link InputMethodService#onDisplayCompletions} to
     * insert completions provided by the app in front of the dictionary
     * suggestions
     */
    public void onDisplayCompletions(CompletionInfo[] completions) {
        createSuggestions();

        // insert completions to the front of suggestions
        final int totalCompletions = completions == null ? 0 : completions.length;
        for (int i = 0; i < totalCompletions && mSuggestions.size() < mNumSuggestions; i++) {
            if (TextUtils.isEmpty(completions[i].getText())) {
                break;
            }
            mSuggestions.add(i, completions[i].getText().toString());
        }

        if (Log.isLoggable(TAG, Log.VERBOSE)) {
            for (int i = 0; i < mSuggestions.size(); i++) {
                Log.d(TAG, "completion " + i + ": " + mSuggestions.get(i));
            }
        }
    }

    public boolean shouldSuggestionsAmend() {
        return (mMode == MODE_DOMAIN);
    }

    public ArrayList<String> getSuggestions() {
        return mSuggestions;
    }

    public void clearSuggestions() {
        mSuggestions.clear();
    }

    public void createSuggestions() {
        clearSuggestions();

        if (mMode == MODE_DOMAIN) {
            String[] commonDomains =
                    mContext.getResources().getStringArray(R.array.common_domains);
            for (String domain : commonDomains) {
                mSuggestions.add(domain);
            }
        }
    }
}
