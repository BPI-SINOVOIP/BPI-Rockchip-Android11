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

package com.android.car.dialer.ui.activecall;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.telephony.common.CallDetail;

import java.util.ArrayList;
import java.util.List;

/**
 * Adapter for holding user profiles of a conference.
 */
public class ConferenceProfileAdapter extends RecyclerView.Adapter<ConferenceProfileViewHolder> {

    private Context mContext;
    private List<CallDetail> mConferenceList = new ArrayList<>();

    public ConferenceProfileAdapter(Context context) {
        mContext = context;
    }

    /**
     * Sets {@link #mConferenceList} based on live data.
     */
    public void setConferenceList(List<CallDetail> list) {
        mConferenceList.clear();
        if (list != null) {
            mConferenceList.addAll(list);
        }
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public ConferenceProfileViewHolder onCreateViewHolder(@NonNull ViewGroup viewGroup, int i) {
        View view = LayoutInflater.from(mContext).inflate(R.layout.user_profile_list_item,
                viewGroup, false);
        return new ConferenceProfileViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ConferenceProfileViewHolder conferenceProfileViewHolder,
            int i) {
        conferenceProfileViewHolder.bind(mConferenceList.get(i));
    }

    @Override
    public int getItemCount() {
        return mConferenceList.size();
    }
}
