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
package com.android.car.bugreport;

import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

/**
 * Shows bugreport title, status, status message and user action buttons. "Upload to Google" button
 * is enabled when the status is {@link Status#STATUS_PENDING_USER_ACTION}, "Move to USB" button is
 * enabled only when status is  {@link Status#STATUS_PENDING_USER_ACTION} and USB device is plugged
 * in.
 */
public class BugInfoAdapter extends RecyclerView.Adapter<BugInfoAdapter.BugInfoViewHolder> {
    static final int BUTTON_TYPE_UPLOAD = 0;
    static final int BUTTON_TYPE_MOVE = 1;
    static final int BUTTON_TYPE_ADD_AUDIO = 2;

    /** Provides a handler for click events*/
    interface ItemClickedListener {
        /**
         * Handles click events differently depending on provided buttonType and
         * uses additional information provided in metaBugReport.
         *
         * @param buttonType One of {@link #BUTTON_TYPE_UPLOAD}, {@link #BUTTON_TYPE_MOVE} or
         *                   {@link #BUTTON_TYPE_ADD_AUDIO}.
         * @param metaBugReport Selected bugreport.
         * @param holder ViewHolder of the clicked item.
         */
        void onItemClicked(int buttonType, MetaBugReport metaBugReport, BugInfoViewHolder holder);
    }

    /**
     * Reference to each bug report info views.
     */
    static class BugInfoViewHolder extends RecyclerView.ViewHolder {
        /** Title view */
        TextView mTitleView;

        /** Status View */
        TextView mStatusView;

        /** Message View */
        TextView mMessageView;

        /** Move Button */
        Button mMoveButton;

        /** Upload Button */
        Button mUploadButton;

        /** Add Audio Button */
        Button mAddAudioButton;

        BugInfoViewHolder(View v) {
            super(v);
            mTitleView = itemView.findViewById(R.id.bug_info_row_title);
            mStatusView = itemView.findViewById(R.id.bug_info_row_status);
            mMessageView = itemView.findViewById(R.id.bug_info_row_message);
            mMoveButton = itemView.findViewById(R.id.bug_info_move_button);
            mUploadButton = itemView.findViewById(R.id.bug_info_upload_button);
            mAddAudioButton = itemView.findViewById(R.id.bug_info_add_audio_button);
        }
    }

    private List<MetaBugReport> mDataset;
    private final ItemClickedListener mItemClickedListener;
    private final Config mConfig;

    BugInfoAdapter(ItemClickedListener itemClickedListener, Config config) {
        mItemClickedListener = itemClickedListener;
        mDataset = new ArrayList<>();
        mConfig = config;
        // Allow RecyclerView to efficiently update UI; getItemId() is implemented below.
        setHasStableIds(true);
    }

    @Override
    public BugInfoViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        // create a new view
        View v = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.bug_info_view, parent, false);
        return new BugInfoViewHolder(v);
    }

    @Override
    public void onBindViewHolder(BugInfoViewHolder holder, int position) {
        MetaBugReport bugreport = mDataset.get(position);
        holder.mTitleView.setText(bugreport.getTitle());
        holder.mStatusView.setText(Status.toString(bugreport.getStatus()));
        holder.mMessageView.setText(bugreport.getStatusMessage());
        if (bugreport.getStatusMessage().isEmpty()) {
            holder.mMessageView.setVisibility(View.GONE);
        } else {
            holder.mMessageView.setVisibility(View.VISIBLE);
        }
        boolean enableUserActionButtons =
                bugreport.getStatus() == Status.STATUS_PENDING_USER_ACTION.getValue()
                        || bugreport.getStatus() == Status.STATUS_MOVE_FAILED.getValue()
                        || bugreport.getStatus() == Status.STATUS_UPLOAD_FAILED.getValue();
        if (enableUserActionButtons) {
            holder.mMoveButton.setEnabled(true);
            holder.mMoveButton.setVisibility(View.VISIBLE);
            holder.mMoveButton.setOnClickListener(
                    view -> mItemClickedListener.onItemClicked(BUTTON_TYPE_MOVE, bugreport,
                            holder));
        } else {
            holder.mMoveButton.setEnabled(false);
            holder.mMoveButton.setVisibility(View.GONE);
        }
        // Enable the upload button only for userdebug/eng builds.
        if (enableUserActionButtons && Build.IS_DEBUGGABLE) {
            holder.mUploadButton.setText(R.string.bugreport_upload_gcs_button_text);
            holder.mUploadButton.setEnabled(true);
            holder.mUploadButton.setVisibility(View.VISIBLE);
            holder.mUploadButton.setOnClickListener(
                    view -> mItemClickedListener.onItemClicked(BUTTON_TYPE_UPLOAD, bugreport,
                            holder));
        } else {
            holder.mUploadButton.setVisibility(View.GONE);
            holder.mUploadButton.setEnabled(false);
        }
        if (bugreport.getStatus() == Status.STATUS_AUDIO_PENDING.getValue()) {
            if (mConfig.getAutoUpload()) {
                holder.mAddAudioButton.setText(R.string.bugreport_add_audio_upload_button_text);
            } else {
                holder.mAddAudioButton.setText(R.string.bugreport_add_audio_button_text);
            }
            holder.mAddAudioButton.setEnabled(true);
            holder.mAddAudioButton.setVisibility(View.VISIBLE);
            holder.mAddAudioButton.setOnClickListener(view ->
                    mItemClickedListener.onItemClicked(BUTTON_TYPE_ADD_AUDIO, bugreport, holder));
        } else {
            holder.mAddAudioButton.setEnabled(false);
            holder.mAddAudioButton.setVisibility(View.GONE);
        }
    }

    /** Sets dataSet; it copies the list, because it modifies it in this adapter. */
    void setDataset(List<MetaBugReport> bugReports) {
        mDataset = new ArrayList<>(bugReports);
        notifyDataSetChanged();
    }

    /** Update a bug report in the data set. */
    void updateBugReportInDataSet(MetaBugReport bugReport, int position) {
        if (position != RecyclerView.NO_POSITION) {
            mDataset.set(position, bugReport);
            notifyItemChanged(position);
        }
    }

    @Override
    public long getItemId(int position) {
        return mDataset.get(position).getId();
    }

    @Override
    public int getItemCount() {
        return mDataset.size();
    }
}
