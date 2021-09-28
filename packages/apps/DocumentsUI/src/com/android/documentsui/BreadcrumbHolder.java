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

package com.android.documentsui;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

/** Recycler view holding the horizontal breadcrumb. */
public final class BreadcrumbHolder extends RecyclerView.ViewHolder {

    protected TextView mTitle;
    protected ImageView mArrow;
    protected int mDefaultTextColor;
    protected boolean mLast;

    public BreadcrumbHolder(View itemView) {
        super(itemView);
        mTitle = itemView.findViewById(R.id.breadcrumb_text);
        mArrow = itemView.findViewById(R.id.breadcrumb_arrow);
        mDefaultTextColor = mTitle.getTextColors().getDefaultColor();
        mLast = false;
    }

    public boolean isLast() {
        return mLast;
    }

    public void setLast(boolean isLast) {
        mLast = isLast;
    }
}
