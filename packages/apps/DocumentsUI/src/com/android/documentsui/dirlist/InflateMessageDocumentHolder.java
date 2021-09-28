/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.documentsui.dirlist;

import android.content.Context;
import android.database.Cursor;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.android.documentsui.R;

/**
 * RecyclerView.ViewHolder class that displays a message when there are no contents
 * in the directory, whether due to no items, no search results or an error.
 * Used by {@link DirectoryAddonsAdapter}.
 */
final class InflateMessageDocumentHolder extends MessageHolder {
    public static final int LAYOUT_CROSS_PROFILE_ERROR = 1;

    private Message mMessage;

    private TextView mContentMessage;
    private ImageView mContentImage;

    private TextView mCrossProfileTitle;
    private TextView mCrossProfileMessage;
    private ImageView mCrossProfileImage;
    private TextView mCrossProfileButton;


    private View mContentView;
    private View mCrossProfileView;
    private View mCrossProfileContent;
    private ProgressBar mCrossProfileProgress;

    public InflateMessageDocumentHolder(Context context, ViewGroup parent) {
        super(context, parent, R.layout.item_doc_inflated_message);
        mContentView = itemView.findViewById(R.id.content);
        mCrossProfileView = itemView.findViewById(R.id.cross_profile);
        mCrossProfileContent = mCrossProfileView.findViewById(R.id.cross_profile_content);
        mCrossProfileProgress = mCrossProfileView.findViewById(R.id.cross_profile_progress);

        mContentMessage = mContentView.findViewById(R.id.message);
        mContentImage = mContentView.findViewById(R.id.artwork);

        mCrossProfileTitle = mCrossProfileView.findViewById(R.id.title);
        mCrossProfileMessage = mCrossProfileView.findViewById(R.id.message);
        mCrossProfileImage = mCrossProfileView.findViewById(R.id.artwork);
        mCrossProfileButton = mCrossProfileView.findViewById(R.id.button);
    }

    public void bind(Message message) {
        mMessage = message;
        bind(null, null);
    }

    @Override
    public void bind(Cursor cursor, String modelId) {
        if (mMessage.getLayout() == LAYOUT_CROSS_PROFILE_ERROR) {
            bindCrossProfileMessageView();
        } else {
            bindContentMessageView();
        }
    }

    private void onCrossProfileButtonClick(View button) {
        mCrossProfileContent.setVisibility(View.GONE);
        mCrossProfileProgress.setVisibility(View.VISIBLE);
        mMessage.runCallback();
    }

    private void bindContentMessageView() {
        mContentView.setVisibility(View.VISIBLE);
        mCrossProfileView.setVisibility(View.GONE);

        mContentMessage.setText(mMessage.getMessageString());
        mContentImage.setImageDrawable(mMessage.getIcon());
    }

    private void bindCrossProfileMessageView() {
        mContentView.setVisibility(View.GONE);
        mCrossProfileView.setVisibility(View.VISIBLE);
        mCrossProfileContent.setVisibility(View.VISIBLE);
        mCrossProfileProgress.setVisibility(View.GONE);

        mCrossProfileTitle.setText(mMessage.getTitleString());
        if (!TextUtils.isEmpty(mMessage.getMessageString())) {
            mCrossProfileMessage.setVisibility(View.VISIBLE);
            mCrossProfileMessage.setText(mMessage.getMessageString());
        } else {
            mCrossProfileMessage.setVisibility(View.GONE);
        }
        mCrossProfileImage.setImageDrawable(mMessage.getIcon());
        if (!TextUtils.isEmpty(mMessage.getButtonString())) {
            mCrossProfileButton.setVisibility(View.VISIBLE);
            mCrossProfileButton.setText(mMessage.getButtonString());
            mCrossProfileButton.setOnClickListener(this::onCrossProfileButtonClick);
        } else {
            mCrossProfileButton.setVisibility(View.GONE);
        }
    }
}
