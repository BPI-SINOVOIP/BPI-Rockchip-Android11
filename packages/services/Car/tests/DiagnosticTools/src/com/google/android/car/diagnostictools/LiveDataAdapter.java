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

package com.google.android.car.diagnostictools;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

/** Adapter which displays live data */
public class LiveDataAdapter extends RecyclerView.Adapter<RowViewHolder> {

    private List<SensorDataWrapper> mLiveData;

    public LiveDataAdapter(List<SensorDataWrapper> initialData) {
        mLiveData = initialData;
    }

    public LiveDataAdapter() {
        mLiveData = new ArrayList<>();
    }

    @NonNull
    @Override
    public RowViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        View v = inflater.inflate(R.layout.row_layout, parent, false);

        return new RowViewHolder(v, false, false, true);
    }

    @Override
    public void onBindViewHolder(@NonNull RowViewHolder holder, int position) {
        SensorDataWrapper wrapper = mLiveData.get(position);
        holder.setFields(wrapper.mName, "", "" + wrapper.mNumber + " " + wrapper.mUnit, false);
    }

    @Override
    public int getItemCount() {
        return mLiveData.size();
    }

    /**
     * Takes in new data to update the RecyclerView with
     *
     * @param data New list of LiveDataWrappers to display
     */
    public void update(List<SensorDataWrapper> data) {
        mLiveData.clear();
        mLiveData.addAll(data);
        notifyDataSetChanged();
    }

    /** Wrapper which holds data about a DataFrame */
    public static class SensorDataWrapper {

        private String mName;
        private String mUnit;
        private String mNumber;

        public SensorDataWrapper(String name, String unit, float number) {
            this.mName = name;
            this.mUnit = unit;
            this.mNumber = String.valueOf(number);
        }

        public SensorDataWrapper(String name, String unit, String number) {
            this.mName = name;
            this.mUnit = unit;
            this.mNumber = number;
        }
    }
}
