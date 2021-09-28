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

import static com.android.documentsui.base.State.MODE_GRID;

import android.content.Context;
import android.database.Cursor;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.documentsui.R;
import com.android.documentsui.base.State.ViewMode;

/**
 * RecyclerView.ViewHolder class that displays at the top of the directory list when there
 * are more information from the Provider.
 * Used by {@link DirectoryAddonsAdapter}.
 */
final class HeaderMessageDocumentHolder extends MessageHolder {
    private final View mRoot;
    private final ImageView mIcon;
    private final TextView mTitle;
    private final TextView mSubtitle;
    private final TextView mTextView;
    private final View mActionView;
    private final Button mDismissButton;
    private final Button mActionButton;
    private Message mMessage;

    public HeaderMessageDocumentHolder(Context context, ViewGroup parent) {
        super(context, parent, R.layout.item_doc_header_message);

        mRoot = itemView.findViewById(R.id.item_root);
        mIcon = (ImageView) itemView.findViewById(R.id.message_icon);
        mTitle = itemView.findViewById(R.id.message_title);
        mSubtitle = itemView.findViewById(R.id.message_subtitle);
        mTextView = (TextView) itemView.findViewById(R.id.message_textview);
        mActionView = (View) itemView.findViewById(R.id.action_view);
        mActionButton = (Button) itemView.findViewById(R.id.action_button);
        mDismissButton = (Button) itemView.findViewById(R.id.dismiss_button);
    }

    public void bind(Message message) {
        mMessage = message;
        mDismissButton.setOnClickListener(this::onButtonClick);
        mActionButton.setOnClickListener(this::onButtonClick);
        bind(null, null);
    }

    /**
     * We set different padding on directory parent in different mode by
     * {@link DirectoryFragment#onViewModeChanged()}. To avoid the layout shifting when
     * display mode changed, we set the opposite padding on the item.
     */
    public void setPadding(@ViewMode int mode) {
        int padding = itemView.getResources().getDimensionPixelSize(mode == MODE_GRID
                ? R.dimen.list_container_padding : R.dimen.grid_container_padding);
        mRoot.setPadding(padding, 0, padding, 0);
    }

    private void onButtonClick(View button) {
        mMessage.runCallback();
    }

    @Override
    public void bind(Cursor cursor, String modelId) {
        if (mMessage.getTitleString() != null) {
            mTitle.setVisibility(View.VISIBLE);
            mSubtitle.setVisibility(View.VISIBLE);
            mTextView.setVisibility(View.GONE);
            mTitle.setText(mMessage.getTitleString());
            mSubtitle.setText(mMessage.getMessageString());
        } else {
            mTitle.setVisibility(View.GONE);
            mSubtitle.setVisibility(View.GONE);
            mTextView.setVisibility(View.VISIBLE);
            mTextView.setText(mMessage.getMessageString());
        }

        mIcon.setImageDrawable(mMessage.getIcon());

        if (mMessage.shouldKeep()) {
            mActionView.setVisibility(View.VISIBLE);
            mDismissButton.setVisibility(View.GONE);
            if (mMessage.getButtonString() != null) {
                mActionButton.setText(mMessage.getButtonString());
            }
        } else {
            mActionView.setVisibility(View.GONE);
            mDismissButton.setVisibility(View.VISIBLE);
            if (mMessage.getButtonString() != null) {
                mDismissButton.setText(mMessage.getButtonString());
            }
        }
    }
}