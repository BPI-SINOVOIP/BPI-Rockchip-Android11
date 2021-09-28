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

package com.android.documentsui.dirlist;

import android.os.UserManager;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.documentsui.ActionHandler;
import com.android.documentsui.BaseActivity;
import com.android.documentsui.R;
import com.android.documentsui.UserIdManager;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.dirlist.AppsRowItemData.AppData;
import com.android.documentsui.dirlist.AppsRowItemData.RootData;
import com.android.documentsui.sidebar.AppItem;
import com.android.documentsui.sidebar.Item;
import com.android.documentsui.sidebar.RootItem;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A manager class stored apps row chip data list. Data will be synced by RootsFragment.
 */
public class AppsRowManager {

    private final ActionHandler mActionHandler;
    private final List<AppsRowItemData> mDataList;
    private final boolean mMaybeShowBadge;
    private final UserIdManager mUserIdManager;

    public AppsRowManager(ActionHandler handler, boolean maybeShowBadge,
            UserIdManager userIdManager) {
        mDataList = new ArrayList<>();
        mActionHandler = handler;
        mMaybeShowBadge = maybeShowBadge;
        mUserIdManager = userIdManager;
    }

    public List<AppsRowItemData> updateList(List<Item> itemList) {
        mDataList.clear();

        // If more than 1 item of the same package, show item summary (e.g. account id).
        Map<String, Integer> packageNameCount = new HashMap<>();
        for (Item item : itemList) {
            String packageName = item.getPackageName();
            int previousCount = packageNameCount.containsKey(packageName)
                    && !TextUtils.isEmpty(packageName)
                    ? packageNameCount.get(packageName) : 0;
            packageNameCount.put(packageName, previousCount + 1);
        }

        for (Item item : itemList) {
            boolean shouldShowSummary = packageNameCount.get(item.getPackageName()) > 1;
            if (item instanceof RootItem) {
                mDataList.add(new RootData((RootItem) item, mActionHandler, shouldShowSummary,
                        mMaybeShowBadge));
            } else {
                mDataList.add(new AppData((AppItem) item, mActionHandler, shouldShowSummary,
                        mMaybeShowBadge));
            }
        }
        return mDataList;
    }

    private boolean shouldShow(State state, boolean isSearchExpanded) {
        boolean isHiddenAction = state.action == State.ACTION_CREATE
                || state.action == State.ACTION_OPEN_TREE
                || state.action == State.ACTION_PICK_COPY_DESTINATION;
        boolean isSearchExpandedAcrossProfile = mUserIdManager.getUserIds().size() > 1
                && state.supportsCrossProfile()
                && isSearchExpanded;

        return state.stack.isRecents() && !isHiddenAction && mDataList.size() > 0
                && !isSearchExpandedAcrossProfile;
    }

    public void updateView(BaseActivity activity) {
        final View appsRowLayout = activity.findViewById(R.id.apps_row);

        if (!shouldShow(activity.getDisplayState(), activity.isSearchExpanded())) {
            appsRowLayout.setVisibility(View.GONE);
            return;
        }

        final LinearLayout appsGroup = activity.findViewById(R.id.apps_group);
        appsGroup.removeAllViews();

        final LayoutInflater inflater = activity.getLayoutInflater();
        final UserId selectedUser = activity.getSelectedUser();
        for (AppsRowItemData data : mDataList) {
            if (selectedUser.equals(data.getUserId())) {
                View item = inflater.inflate(R.layout.apps_item, appsGroup, false);
                bindView(item, data);
                appsGroup.addView(item);
            }
        }

        appsRowLayout.setVisibility(appsGroup.getChildCount() > 0 ? View.VISIBLE : View.GONE);
    }

    private void bindView(View view, AppsRowItemData data) {
        final ImageView app_icon = view.findViewById(R.id.app_icon);
        final TextView title = view.findViewById(android.R.id.title);
        final TextView summary = view.findViewById(R.id.summary);

        app_icon.setImageDrawable(data.getIconDrawable(view.getContext()));
        title.setText(data.getTitle());
        title.setContentDescription(
                data.getUserId().getUserBadgedLabel(view.getContext(), data.getTitle()));
        summary.setText(data.getSummary());
        summary.setVisibility(data.getSummary() != null ? View.VISIBLE : View.GONE);
        view.setOnClickListener(v -> data.onClicked());
    }
}
