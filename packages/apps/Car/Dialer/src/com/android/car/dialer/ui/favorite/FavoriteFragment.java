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

package com.android.car.dialer.ui.favorite;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.Constants;
import com.android.car.dialer.R;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.ui.common.DialerListBaseFragment;
import com.android.car.dialer.ui.common.DialerUtils;
import com.android.car.telephony.common.Contact;
import com.android.car.ui.recyclerview.DelegatingContentLimitingAdapter;

/**
 * Contains a list of favorite contacts.
 */
public class FavoriteFragment extends DialerListBaseFragment {

    private DelegatingContentLimitingAdapter<FavoriteContactViewHolder>
            mContentLimitingAdapter;

    /**
     * Constructs a new {@link FavoriteFragment}
     */
    public static FavoriteFragment newInstance() {
        return new FavoriteFragment();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        getRecyclerView().addItemDecoration(new ItemSpacingDecoration());
        getRecyclerView().setItemAnimator(null);

        FavoriteAdapter favoriteAdapter = new FavoriteAdapter();
        favoriteAdapter.setOnAddFavoriteClickedListener(this::onAddFavoriteClicked);

        FavoriteViewModel favoriteViewModel = ViewModelProviders.of(getActivity()).get(
                FavoriteViewModel.class);
        favoriteAdapter.setOnListItemClickedListener(this::onItemClicked);
        favoriteViewModel.getFavoriteContacts().observe(this, contacts -> {
            if (contacts.isLoading()) {
                showLoading();
            } else if (contacts.getData() == null) {
                showEmpty(Constants.INVALID_RES_ID, R.string.favorites_empty,
                        R.string.no_favorites_added, R.string.add_favorite_button,
                        v -> onAddFavoriteClicked(), true);
            } else {
                favoriteAdapter.setFavoriteContacts(contacts.getData());
                int contactCount = contacts.getData().size();
                mContentLimitingAdapter.updateUnderlyingDataChanged(contactCount,
                        DialerUtils.validateListLimitingAnchor(
                                contactCount,
                                favoriteAdapter.getLastLimitingAnchorIndex()));
                showContent();
            }
        });
        favoriteViewModel.getSortOrderLiveData().observe(this,
                v -> favoriteAdapter.setSortMethod(v));

        mContentLimitingAdapter =
                new DelegatingContentLimitingAdapter<>(
                        favoriteAdapter,
                        R.id.favorite_list_uxr_config);
        getRecyclerView().setAdapter(mContentLimitingAdapter);
        getUxrContentLimiter().setAdapter(mContentLimitingAdapter);
    }

    @NonNull
    @Override
    protected RecyclerView.LayoutManager createLayoutManager() {
        return new FavoriteLayoutManager(getContext());
    }

    private void onItemClicked(Contact contact) {
        DialerUtils.promptForPrimaryNumber(getContext(), contact, (phoneNumber, always) ->
                UiCallManager.get().placeCall(phoneNumber.getRawNumber()));
    }

    private void onAddFavoriteClicked() {
        pushContentFragment(AddFavoriteFragment.newInstance(), null);
    }

    private class ItemSpacingDecoration extends RecyclerView.ItemDecoration {

        @Override
        public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
                @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
            super.getItemOffsets(outRect, view, parent, state);
            Resources resources = FavoriteFragment.this.getContext().getResources();
            int numColumns = resources.getInteger(R.integer.favorite_fragment_grid_column);
            int leftPadding =
                    resources.getDimensionPixelOffset(R.dimen.favorite_card_space_horizontal);
            int verticalPadding =
                    resources.getDimensionPixelOffset(R.dimen.favorite_card_space_vertical);

            if (parent.getChildAdapterPosition(view) % numColumns == 0) {
                leftPadding = 0;
            }

            outRect.set(leftPadding, verticalPadding, 0, verticalPadding);
        }
    }

    private class FavoriteLayoutManager extends GridLayoutManager {
        private FavoriteLayoutManager(Context context) {
            super(context,
                    context.getResources().getInteger(R.integer.favorite_fragment_grid_column));
            SpanSizeLookup spanSizeLookup = new SpanSizeLookup() {
                @Override
                public int getSpanSize(int position) {
                    if (mContentLimitingAdapter.getItemViewType(position)
                            == FavoriteAdapter.TYPE_HEADER
                            || mContentLimitingAdapter.getItemViewType(position)
                            == mContentLimitingAdapter.getScrollingLimitedMessageViewType()) {
                        return getSpanCount();
                    }
                    return 1;
                }
            };
            setSpanSizeLookup(spanSizeLookup);
        }
    }
}
