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

package com.android.car.dialer.ui.favorite;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.ui.common.DialerUtils;
import com.android.car.dialer.ui.common.OnItemClickedListener;
import com.android.car.dialer.ui.common.entity.Header;
import com.android.car.telephony.common.Contact;
import com.android.car.ui.recyclerview.DelegatingContentLimitingAdapter;

import java.util.Collections;
import java.util.List;

/**
 * Adapter class for binding favorite contacts.
 */
public class FavoriteAdapter extends RecyclerView.Adapter<FavoriteContactViewHolder> implements
        DelegatingContentLimitingAdapter.ContentLimiting {
    private static final String TAG = "CD.FavoriteAdapter";
    static final int TYPE_CONTACT = 0;
    static final int TYPE_HEADER = 1;
    static final int TYPE_ADD_FAVORITE = 2;

    private Integer mSortMethod;
    private LinearLayoutManager mLayoutManager;
    private int mLimitingAnchorIndex = 0;

    /**
     * Listener interface for when the add favorite button is clicked
     */
    public interface OnAddFavoriteClickedListener {
        /**
         * Called when the add favorite button is clicked
         */
        void onAddFavoriteClicked();
    }

    private List<Object> mFavoriteContacts = Collections.emptyList();
    private OnItemClickedListener<Contact> mListener;
    private OnAddFavoriteClickedListener mAddFavoriteListener;

    /**
     * Sets the favorite contact list.
     */
    public void setFavoriteContacts(List<Object> favoriteContacts) {
        L.d(TAG, "setFavoriteContacts %s", favoriteContacts);
        mFavoriteContacts = (favoriteContacts != null) ? favoriteContacts : Collections.emptyList();
        notifyDataSetChanged();
    }

    /**
     * Sets the sorting method for the list.
     */
    public void setSortMethod(Integer sortMethod) {
        mSortMethod = sortMethod;
    }

    @Override
    public int getItemCount() {
        return mFavoriteContacts.size();
    }

    @Override
    public int getItemViewType(int position) {
        Object item = mFavoriteContacts.get(position);
        if (item instanceof Contact) {
            return TYPE_CONTACT;
        } else if (item instanceof Header) {
            return TYPE_HEADER;
        } else {
            return TYPE_ADD_FAVORITE;
        }
    }

    @Override
    public FavoriteContactViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        int layoutId;
        if (viewType == TYPE_CONTACT) {
            layoutId = R.layout.favorite_contact_list_item;
        } else if (viewType == TYPE_HEADER) {
            layoutId = R.layout.header_item;
        } else {
            layoutId = R.layout.add_favorite_list_item;
        }

        View view = LayoutInflater.from(parent.getContext()).inflate(layoutId, parent, false);
        return new FavoriteContactViewHolder(view);
    }

    @Override
    public void onBindViewHolder(FavoriteContactViewHolder viewHolder, int position) {
        int itemViewType = getItemViewType(position);
        switch (itemViewType) {
            case TYPE_CONTACT:
                Contact contact = (Contact) mFavoriteContacts.get(position);
                viewHolder.bind(contact, mSortMethod);
                viewHolder.itemView.setOnClickListener(v -> onItemViewClicked(contact));
                break;
            case TYPE_HEADER:
                Header header = (Header) mFavoriteContacts.get(position);
                viewHolder.onBind(header);
                viewHolder.itemView.setOnClickListener(null);
                viewHolder.itemView.setFocusable(false);
                break;
            case TYPE_ADD_FAVORITE:
                viewHolder.itemView.setOnClickListener(v -> {
                    if (mAddFavoriteListener != null) {
                        mAddFavoriteListener.onAddFavoriteClicked();
                    }
                });
                break;
        }
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
    public int getScrollToPositionWhenRestricted() {
        // Not scrolling.
        return -1;
    }

    @Override
    public int computeAnchorIndexWhenRestricting() {
        mLimitingAnchorIndex = DialerUtils.getFirstVisibleItemPosition(mLayoutManager);
        return mLimitingAnchorIndex;
    }

    /**
     * Returns the previous list limiting anchor position.
     */
    public int getLastLimitingAnchorIndex() {
        return mLimitingAnchorIndex;
    }

    private void onItemViewClicked(Contact contact) {
        if (mListener != null) {
            mListener.onItemClicked(contact);
        }
    }

    /**
     * Sets a {@link OnItemClickedListener listener} which will be called when an item is clicked.
     */
    public void setOnListItemClickedListener(OnItemClickedListener<Contact> listener) {
        mListener = listener;
    }

    /**
     * Sets a {@link OnAddFavoriteClickedListener listener} which will be called when the "Add
     * favorite" button is clicked.
     */
    public void setOnAddFavoriteClickedListener(OnAddFavoriteClickedListener listener) {
        mAddFavoriteListener = listener;
    }
}
