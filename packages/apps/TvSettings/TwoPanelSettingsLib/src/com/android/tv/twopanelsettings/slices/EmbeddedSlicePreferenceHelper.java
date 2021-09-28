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
 * limitations under the License
 */

package com.android.tv.twopanelsettings.slices;

import static android.app.slice.Slice.HINT_PARTIAL;

import android.content.Context;
import android.net.Uri;
import android.view.ContextThemeWrapper;

import androidx.lifecycle.Observer;
import androidx.preference.Preference;
import androidx.slice.Slice;
import androidx.slice.SliceItem;
import androidx.slice.widget.ListContent;
import androidx.slice.widget.SliceContent;

import java.util.List;

/**
 * Helper class to handle the updates for embedded slice preferences.
 */
public class EmbeddedSlicePreferenceHelper implements Observer<Slice> {
    private final Preference mPreference;
    private final Context mContext;
    SlicePreferenceListener mListener;
    Preference mNewPref;
    private String mUri;
    private Slice mSlice;

    EmbeddedSlicePreferenceHelper(Preference preference, String uri) {
        mPreference = preference;
        mUri = uri;
        mContext = mPreference.getContext();
    }

    void onAttached() {
        getSliceLiveData().observeForever(this);
    }

    void onDetached() {
        getSliceLiveData().removeObserver(this);
    }

    private PreferenceSliceLiveData.SliceLiveDataImpl getSliceLiveData() {
        return ContextSingleton.getInstance()
                .getSliceLiveData(mContext, Uri.parse(mUri));
    }

    @Override
    public void onChanged(Slice slice) {
        mSlice = slice;
        if (slice == null || slice.getHints() == null || slice.getHints().contains(HINT_PARTIAL)) {
            updateVisibility(false);
            return;
        }
        update();
    }

    private void updateVisibility(boolean visible) {
        mPreference.setVisible(visible);
        if (mListener != null) {
            mListener.onChangeVisibility();
        }
    }

    private void update() {
        ListContent mListContent = new ListContent(mSlice);
        List<SliceContent> items = mListContent.getRowItems();
        if (items == null || items.size() == 0) {
            updateVisibility(false);
            return;
        }
        SliceItem embeddedItem = SlicePreferencesUtil.getEmbeddedItem(items);
        mNewPref = SlicePreferencesUtil.getPreference(embeddedItem,
                (ContextThemeWrapper) mContext, null);
        if (mNewPref == null) {
            updateVisibility(false);
            return;
        }
        updateVisibility(true);
        if (mPreference instanceof EmbeddedSlicePreference) {
            ((EmbeddedSlicePreference) mPreference).update();
        } else if (mPreference instanceof EmbeddedSliceSwitchPreference) {
            ((EmbeddedSliceSwitchPreference) mPreference).update();
        }
    }

    /**
     * Implement this if the container needs to do something when embedded slice preference change
     * visibility.
     **/
    public interface SlicePreferenceListener {
        /**
         * Callback when the preference change visibility
         */
        void onChangeVisibility();
    }
}
