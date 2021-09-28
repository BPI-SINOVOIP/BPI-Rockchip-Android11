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
package com.android.car.rotaryplayground;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import java.util.Collections;
import java.util.List;

/**
 * The adapter that populates the example rotary grid view with strings.
 */
final class RotaryGridAdapter extends RecyclerView.Adapter<RotaryGridAdapter.ItemViewHolder> {

    private final String[] mData;
    private final Context mContext;
    private final LayoutInflater mInflater;

    RotaryGridAdapter(Context context, String[] data) {
        this.mContext = context;
        this.mInflater = LayoutInflater.from(context);
        this.mData = data;
    }

    @Override
    public ItemViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view = mInflater.inflate(
                R.layout.rotary_grid_item, parent, /* attachToRoot= */ false);
        return new ItemViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ItemViewHolder holder, int position) {
        holder.setText(mData[position]);
    }

    @Override
    public int getItemCount() {
        return mData.length;
    }

    /** The viewholder class that contains the grid item data. */
    static class ItemViewHolder extends RecyclerView.ViewHolder {
        private TextView mItemText;

        ItemViewHolder(View view) {
            super(view);
            mItemText = itemView.findViewById(R.id.rotary_grid_item_text);
        }

        void setText(String text) {
            this.mItemText.setText(text);
        }
    }
}