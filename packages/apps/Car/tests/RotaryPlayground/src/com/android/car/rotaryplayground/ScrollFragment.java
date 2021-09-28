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
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Fragment to demo scrolling with CarRecyclerView. */
public class ScrollFragment extends Fragment {

    // Item types
    private static final int TYPE_BUTTONS = 1;
    private static final int TYPE_TEXT = 2;

    private View mScrollView;

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        mScrollView = inflater.inflate(R.layout.rotary_scroll, container, false);
        initView();
        return mScrollView;
    }

    private void initView() {
        List<ScrollListItem> items = new ArrayList<>();

        // List of height for items to be added to the demo.
        // 0 indicates focusable buttons inflated by R.layout.rotary_scroll.button.
        // Values > 0 indiciates non-focusable texts inflated by R.layout.rotary_scroll_text.
        final int[] itemHeights = getResources().getIntArray(R.array.scroll_item_heights);

        for (int height : itemHeights) {
            if (height < 0) {
                continue;
            }
            ScrollListItem item = (height == 0)
                    ? ScrollListItem.createButtonsItem()
                    : ScrollListItem.createTextItemWithHeight(height);
            items.add(item);
        }

        // Set adapter
        Context context = getContext();
        ScrollListItemAdapter adapter = new ScrollListItemAdapter(context, items);
        RecyclerView view = (RecyclerView) mScrollView.findViewById(R.id.rotary_scroll_view);
        view.setAdapter(adapter);
    }

    /** The adapter used by RotaryScroll to render ScrollListItems. */
    private static class ScrollListItemAdapter extends
            RecyclerView.Adapter<RecyclerView.ViewHolder> {

        final Context mContext;
        final List<ScrollListItem> mItems;

        ScrollListItemAdapter(Context context, List<ScrollListItem> items) {
            this.mContext = context;
            this.mItems = items;
        }

        @NonNull
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(
                @NonNull ViewGroup viewGroup, int viewType) {
            View view;
            switch (viewType) {
                case TYPE_BUTTONS:
                    view = LayoutInflater.from(mContext).inflate(
                            R.layout.rotary_scroll_button, viewGroup, false);
                    return new RecyclerView.ViewHolder(view) {};

                case TYPE_TEXT:
                    view = LayoutInflater.from(mContext).inflate(
                            R.layout.rotary_scroll_text, viewGroup, false);
                    return new ScrollTextHolder(view);

                default:
                    throw new IllegalArgumentException("Unexpected viewType: " + viewType);
            }
        }

        @Override
        public int getItemViewType(int position) {
            return mItems.get(position).getType();
        }

        @Override
        public void onBindViewHolder(@NonNull RecyclerView.ViewHolder viewHolder, int position) {
            if (getItemViewType(position) == TYPE_TEXT) {
                ((ScrollTextHolder) viewHolder).setHeight(mItems.get(position).getHeight());
            }
        }

        @Override
        public int getItemCount() {
            return mItems.size();
        }
    }

    /** A ViewHolder for non-focusable ScrollListItems */
    private static class ScrollTextHolder extends RecyclerView.ViewHolder {

        @NonNull
        final TextView mScrollTextView;

        ScrollTextHolder(@NonNull View itemView) {
            super(itemView);
            mScrollTextView = itemView.findViewById(R.id.scroll_text_view);
        }

        void setHeight(int height) {
            mScrollTextView.setHeight(height);
        }
    }

    private static class ScrollListItem {

        final int mHeight;
        final int mType;

        static ScrollListItem createTextItemWithHeight(int height) {
          return new ScrollListItem(TYPE_TEXT, height);
        }
        static ScrollListItem createButtonsItem() {
          return new ScrollListItem(TYPE_BUTTONS, 0);
        }

        ScrollListItem(int type, int height) {
            this.mType = type;
            this.mHeight = height;
        }

        int getHeight() {
            return mHeight;
        }

        int getType() {
            return mType;
        }
    }
}
