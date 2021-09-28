/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.car.dialer.ui.calllog;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.ui.common.DialerUtils;
import com.android.car.dialer.ui.common.entity.HeaderViewHolder;
import com.android.car.dialer.ui.common.entity.UiCallLog;
import com.android.car.telephony.common.Contact;
import com.android.car.ui.recyclerview.ContentLimitingAdapter;

import java.util.ArrayList;
import java.util.List;

/** Adapter for call history list. */
public class CallLogAdapter extends ContentLimitingAdapter {

    private static final String TAG = "CD.CallLogAdapter";

    private Integer mSortMethod;

    /** IntDef for the different groups of calllog lists separated by time periods. */
    @IntDef({
            EntryType.TYPE_HEADER,
            EntryType.TYPE_CALLLOG,
    })
    private @interface EntryType {
        /** Entry typre is header. */
        int TYPE_HEADER = 1;

        /** Entry type is calllog. */
        int TYPE_CALLLOG = 2;
    }

    public interface OnShowContactDetailListener {
        void onShowContactDetail(Contact contact);
    }

    private List<Object> mUiCallLogs = new ArrayList<>();
    private Context mContext;
    private CallLogAdapter.OnShowContactDetailListener mOnShowContactDetailListener;
    private LinearLayoutManager mLayoutManager;
    private int mLimitingAnchorIndex = 0;

    public CallLogAdapter(Context context,
            CallLogAdapter.OnShowContactDetailListener onShowContactDetailListener) {
        mContext = context;
        mOnShowContactDetailListener = onShowContactDetailListener;
    }

    /**
     * Sets calllogs.
     */
    public void setUiCallLogs(@NonNull List<Object> uiCallLogs) {
        L.d(TAG, "setUiCallLogs: %d", uiCallLogs.size());
        mUiCallLogs.clear();
        mUiCallLogs.addAll(uiCallLogs);

        // Update the data set size change along with the old anchor point.
        // The anchor point won't take effect if content is not limited.
        updateUnderlyingDataChanged(uiCallLogs.size(),
                DialerUtils.validateListLimitingAnchor(uiCallLogs.size(), mLimitingAnchorIndex));
        notifyDataSetChanged();
    }

    /**
     * Sets the sorting method for the list.
     */
    public void setSortMethod(Integer sortMethod) {
        mSortMethod = sortMethod;
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolderImpl(
            @NonNull ViewGroup parent, int viewType) {
        if (viewType == EntryType.TYPE_CALLLOG) {
            View rootView = LayoutInflater.from(mContext)
                    .inflate(R.layout.call_history_list_item, parent, false);
            return new CallLogViewHolder(rootView, mOnShowContactDetailListener);
        }

        View rootView = LayoutInflater.from(mContext)
                .inflate(R.layout.header_item, parent, false);
        return new HeaderViewHolder(rootView);
    }

    @Override
    public void onBindViewHolderImpl(
            @NonNull RecyclerView.ViewHolder holder, int position) {
        if (holder instanceof CallLogViewHolder) {
            ((CallLogViewHolder) holder).bind((UiCallLog) mUiCallLogs.get(position), mSortMethod);
        } else {
            ((HeaderViewHolder) holder).setHeaderTitle((String) mUiCallLogs.get(position));
        }
    }

    @Override
    @EntryType
    public int getItemViewTypeImpl(int position) {
        if (mUiCallLogs.get(position) instanceof UiCallLog) {
            return EntryType.TYPE_CALLLOG;
        } else {
            return EntryType.TYPE_HEADER;
        }
    }

    @Override
    public void onViewRecycledImpl(@NonNull RecyclerView.ViewHolder holder) {
        if (holder instanceof CallLogViewHolder) {
            ((CallLogViewHolder) holder).recycle();
        }
    }

    @Override
    public int getUnrestrictedItemCount() {
        return mUiCallLogs.size();
    }

    @Override
    public int getConfigurationId() {
        return R.id.call_log_list_uxr_config;
    }

    @Override
    public void onAttachedToRecyclerView(@NonNull RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);
        mLayoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
    }

    @Override
    public void onDetachedFromRecyclerView(@NonNull RecyclerView recyclerView) {
        mLayoutManager = null;
        super.onDetachedFromRecyclerView(recyclerView);
    }

    @Override
    public int computeAnchorIndexWhenRestricting() {
        mLimitingAnchorIndex = DialerUtils.getFirstVisibleItemPosition(mLayoutManager);
        return mLimitingAnchorIndex;
    }
}
