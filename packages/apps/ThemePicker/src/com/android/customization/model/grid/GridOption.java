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
package com.android.customization.model.grid;

import android.content.Context;
import android.graphics.PorterDuff.Mode;
import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.android.customization.model.CustomizationManager;
import com.android.customization.model.CustomizationOption;
import com.android.customization.widget.GridTileDrawable;
import com.android.wallpaper.R;

/**
 * Represents a grid layout option available in the current launcher.
 */
public class GridOption implements CustomizationOption<GridOption>, Parcelable {
    public static final Creator<GridOption> CREATOR = new Creator<GridOption>() {
        @Override
        public GridOption createFromParcel(Parcel in) {
            return new GridOption(in);
        }

        @Override
        public GridOption[] newArray(int size) {
            return new GridOption[size];
        }
    };

    private final String mTitle;
    private final boolean mIsCurrent;
    private final String mIconShapePath;
    private final GridTileDrawable mTileDrawable;
    public final String name;
    public final int rows;
    public final int cols;
    public final Uri previewImageUri;
    public final int previewPagesCount;

    public GridOption(String title, String name, boolean isCurrent, int rows, int cols,
            Uri previewImageUri, int previewPagesCount, String iconShapePath) {
        mTitle = title;
        mIsCurrent = isCurrent;
        mIconShapePath = iconShapePath;
        mTileDrawable = new GridTileDrawable(rows, cols, mIconShapePath);
        this.name = name;
        this.rows = rows;
        this.cols = cols;
        this.previewImageUri = previewImageUri;
        this.previewPagesCount = previewPagesCount;
    }

    protected GridOption(Parcel in) {
        mTitle = in.readString();
        mIsCurrent = in.readByte() != 0;
        mIconShapePath = in.readString();
        name = in.readString();
        rows = in.readInt();
        cols = in.readInt();
        previewImageUri = in.readParcelable(Uri.class.getClassLoader());
        previewPagesCount = in.readInt();
        mTileDrawable = new GridTileDrawable(rows, cols, mIconShapePath);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public void bindThumbnailTile(View view) {
        Context context = view.getContext();

        mTileDrawable.setColorFilter(context.getResources().getColor(
                R.color.material_grey500, null), Mode.ADD);
        ((ImageView) view.findViewById(R.id.grid_option_thumbnail))
                .setImageDrawable(mTileDrawable);
    }

    @Override
    public boolean isActive(CustomizationManager<GridOption> manager) {
        return mIsCurrent;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }

        if (obj instanceof GridOption) {
            GridOption other = (GridOption) obj;
            return TextUtils.equals(this.name, other.name)
                    && this.cols == other.cols
                    && this.rows == other.rows;
        }
        return false;
    }

    @Override
    public int getLayoutResId() {
        return R.layout.grid_option;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int i) {
        parcel.writeString(mTitle);
        parcel.writeByte((byte) (mIsCurrent ? 1 : 0));
        parcel.writeString(mIconShapePath);
        parcel.writeString(name);
        parcel.writeInt(rows);
        parcel.writeInt(cols);
        parcel.writeParcelable(previewImageUri, i);
        parcel.writeInt(previewPagesCount);
    }
}
