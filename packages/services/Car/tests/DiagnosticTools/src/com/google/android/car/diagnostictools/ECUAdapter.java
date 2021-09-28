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

import android.content.Context;
import android.content.Intent;
import android.os.Parcelable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import com.google.android.car.diagnostictools.utils.SelectableAdapter;

import java.util.ArrayList;
import java.util.List;

/** Adapter for RecyclerView in ECUListActivity which displays ECUs */
public class ECUAdapter extends SelectableAdapter<ECU, RowViewHolder> {

    private List<ECU> mEcuOrderedList;
    private Context mContext;

    public ECUAdapter(List<ECU> ecusIn, Context context) {
        mEcuOrderedList = ecusIn;
        mContext = context;
        mEcuOrderedList.sort((ecu, t1) -> t1.getDtcs().size() - ecu.getDtcs().size());
    }

    @NonNull
    @Override
    public RowViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        View v = inflater.inflate(R.layout.row_layout, parent, false);

        return new RowViewHolder(v, true, false, true);
    }

    @Override
    public void onBindViewHolder(@NonNull RowViewHolder holder, final int position) {
        final ECU refECU = mEcuOrderedList.get(position);
        final RowViewHolder finalHolder = holder;
        final String name = refECU.getName();
        refECU.setColor(finalHolder);
        holder.setFields(
                refECU.getAddress(), name, String.format("%d DTCs", refECU.getDtcs().size()),
                false);
        holder.layout.setOnClickListener(
                view -> {
                    if (hasSelected()) {
                        toggleSelect(refECU);
                        refECU.setColor(finalHolder);
                    } else {
                        Intent intent = new Intent(mContext, DTCListActivity.class);
                        intent.putExtra("name", name);
                        intent.putParcelableArrayListExtra(
                                "dtcs", (ArrayList<? extends Parcelable>) refECU.getDtcs());
                        mContext.startActivity(intent);
                    }
                });
        holder.layout.setOnLongClickListener(
                view -> {
                    toggleSelect(refECU);
                    refECU.setColor(finalHolder);
                    return true;
                });
    }

    @Override
    public int getItemCount() {
        return mEcuOrderedList.size();
    }

    /**
     * Overrides method getBaseList of SelectableAdapter to allow selection of elements
     *
     * @return base list of DTCs
     */
    @Override
    protected List<ECU> getBaseList() {
        return mEcuOrderedList;
    }
}
