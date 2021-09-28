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

package com.android.tv.twopanelsettings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.leanback.preference.LeanbackListPreferenceDialogFragment;
import androidx.preference.DialogPreference;
import androidx.preference.ListPreference;
import androidx.preference.MultiSelectListPreference;
import androidx.recyclerview.widget.RecyclerView;

/** A workaround for pi-tv-dev to fix the issue that ListPreference is not correctly handled by two
 * panel lib. When moving to Q, we should fix this problem in androidx(b/139085296).
 */
public class TwoPanelListPreferenceDialogFragment extends LeanbackListPreferenceDialogFragment {
    private static final String SAVE_STATE_IS_MULTI =
            "LeanbackListPreferenceDialogFragment.isMulti";
    private static final String SAVE_STATE_ENTRIES = "LeanbackListPreferenceDialogFragment.entries";
    private static final String SAVE_STATE_ENTRY_VALUES =
            "LeanbackListPreferenceDialogFragment.entryValues";
    private static final String SAVE_STATE_INITIAL_SELECTION =
            "LeanbackListPreferenceDialogFragment.initialSelection";
    private boolean mMultiCopy;
    private CharSequence[] mEntriesCopy;
    private CharSequence[] mEntryValuesCopy;
    private String mInitialSelectionCopy;

    /** Provide a ListPreferenceDialogFragment which satisfy the use of two panel lib **/
    public static TwoPanelListPreferenceDialogFragment newInstanceSingle(String key) {
        final Bundle args = new Bundle(1);
        args.putString(ARG_KEY, key);

        final TwoPanelListPreferenceDialogFragment
                fragment = new TwoPanelListPreferenceDialogFragment();
        fragment.setArguments(args);

        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState == null) {
            final DialogPreference preference = getPreference();
            if (preference instanceof ListPreference) {
                mMultiCopy = false;
                mEntriesCopy = ((ListPreference) preference).getEntries();
                mEntryValuesCopy = ((ListPreference) preference).getEntryValues();
                mInitialSelectionCopy = ((ListPreference) preference).getValue();
            } else if (preference instanceof MultiSelectListPreference) {
                mMultiCopy = true;
            } else {
                throw new IllegalArgumentException("Preference must be a ListPreference or "
                    + "MultiSelectListPreference");
            }
        } else {
            mMultiCopy = savedInstanceState.getBoolean(SAVE_STATE_IS_MULTI);
            mEntriesCopy = savedInstanceState.getCharSequenceArray(SAVE_STATE_ENTRIES);
            mEntryValuesCopy = savedInstanceState.getCharSequenceArray(SAVE_STATE_ENTRY_VALUES);
            if (!mMultiCopy) {
                mInitialSelectionCopy = savedInstanceState.getString(SAVE_STATE_INITIAL_SELECTION);
            }
        }
    }

    @Override
    public RecyclerView.Adapter onCreateAdapter() {
        if (!mMultiCopy) {
            return new TwoPanelAdapterSingle(mEntriesCopy, mEntryValuesCopy, mInitialSelectionCopy);
        }
        return super.onCreateAdapter();
    }

    private class TwoPanelAdapterSingle extends RecyclerView.Adapter<ViewHolder>
            implements ViewHolder.OnItemClickListener  {
        private final CharSequence[] mEntries;
        private final CharSequence[] mEntryValues;
        private CharSequence mSelectedValue;

        TwoPanelAdapterSingle(CharSequence[] entries, CharSequence[] entryValues,
                CharSequence selectedValue) {
            mEntries = entries;
            mEntryValues = entryValues;
            mSelectedValue = selectedValue;
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
            final View view = inflater.inflate(R.layout.leanback_list_preference_item_single,
                    parent, false);
            return new ViewHolder(view, this);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            holder.getWidgetView().setChecked(mEntryValues[position].equals(mSelectedValue));
            holder.getTitleView().setText(mEntries[position]);
        }

        @Override
        public int getItemCount() {
            return mEntries.length;
        }

        @Override
        public void onItemClick(ViewHolder viewHolder) {
            final int index = viewHolder.getAdapterPosition();
            if (index == RecyclerView.NO_POSITION) {
                return;
            }
            final CharSequence entry = mEntryValues[index];
            final ListPreference preference = (ListPreference) getPreference();
            if (index >= 0) {
                String value = mEntryValues[index].toString();
                if (preference.callChangeListener(value)) {
                    preference.setValue(value);
                    mSelectedValue = entry;
                }
            }

            if (getParentFragment() instanceof TwoPanelSettingsFragment) {
                TwoPanelSettingsFragment parent = (TwoPanelSettingsFragment) getParentFragment();
                parent.navigateBack();
            }
            notifyDataSetChanged();
        }
    }
}
