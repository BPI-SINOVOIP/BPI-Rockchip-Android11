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
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import com.google.android.car.diagnostictools.utils.SelectableAdapter;

import java.util.List;

/** Adapter for RecyclerView in DTCListActivity which displays DTCs */
public class DTCAdapter extends SelectableAdapter<DTC, RowViewHolder> {

    private List<DTC> mDtcs;
    private Context mContext;

    DTCAdapter(List<DTC> inputDTCs, Context context) {
        mDtcs = inputDTCs;
        mContext = context;
    }

    @NonNull
    @Override
    public RowViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        View v = inflater.inflate(R.layout.row_layout, parent, false);

        return new RowViewHolder(v, true, false, false);
    }

    @Override
    public void onBindViewHolder(@NonNull RowViewHolder holder, int position) {
        final DTC refDTC = mDtcs.get(position);
        final RowViewHolder finalHolder = holder;
        holder.setFields(refDTC.getCode(), refDTC.getDescription(), "", false);
        refDTC.setColor(finalHolder);
        holder.layout.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        if (hasSelected()) {
                            toggleSelect(refDTC);
                            refDTC.setColor(finalHolder);
                        } else {
                            Intent intent = new Intent(mContext, DTCDetailActivity.class);
                            intent.putExtra("dtc", refDTC);
                            mContext.startActivity(intent);
                        }
                    }
                });
        holder.layout.setOnLongClickListener(
                new View.OnLongClickListener() {
                    @Override
                    public boolean onLongClick(View view) {
                        toggleSelect(refDTC);
                        refDTC.setColor(finalHolder);
                        return true;
                    }
                });
    }

    @Override
    public int getItemCount() {
        return mDtcs.size();
    }

    /**
     * Overrides method getBaseList of SelectableAdapter to allow selection of elements
     *
     * @return base list of DTCs
     */
    @Override
    protected List<DTC> getBaseList() {
        return mDtcs;
    }
}
