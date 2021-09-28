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

package com.android.tv.twopanelsettings.slices;

import android.content.Context;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import com.android.tv.twopanelsettings.R;

import java.util.List;

/**
 * InfoPreference which could be used to display a list of information.
 */
public class InfoPreference extends Preference {

    private List<Pair<CharSequence, CharSequence>> mInfoList;

    public InfoPreference(Context context, List<Pair<CharSequence, CharSequence>> infoList) {
        super(context);
        mInfoList = infoList;
        setLayoutResource(R.layout.info_preference);
        setEnabled(false);
    }

    @Override
    public void onBindViewHolder(final PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ViewGroup container = (ViewGroup) holder.findViewById(R.id.item_container);
        container.removeAllViews();
        for (Pair<CharSequence, CharSequence> info : mInfoList) {
            View view = LayoutInflater.from(getContext()).inflate(R.layout.info_preference_item,
                    container, false);
            ((TextView) view.findViewById(R.id.info_item_title)).setText(info.first);
            ((TextView) view.findViewById(R.id.info_item_summary)).setText(info.second);
            container.addView(view);
        }
    }
}
