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

package com.android.documentsui.sidebar;

import static androidx.core.util.Preconditions.checkNotNull;

import com.android.documentsui.base.UserId;

import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.Lists;
import com.google.common.collect.Multimap;

import java.util.Collection;
import java.util.Collections;
import java.util.List;

/**
 * A builder to build a list of root items to be displayed on the {@link RootsFragment}. The list
 * should contain roots that the were added to this builder, except for those which support
 * cross-profile.
 *
 * <p>If the root supports cross-profile, the list would only contain the root of the
 * selected user.
 *
 * <p>If no root of the selected user was added but that of the other user was added,
 * a dummy root of that root for the selected user will be generated.
 *
 * <p>The builder group the roots using {@link Item#stringId} as key.
 *
 * <p>For example, if these items were added to the builder: itemA[0], itemA[10], itemB[0],
 * itemC[10], itemX[0],itemY[10] where root itemX, itemY do not support cross profile.
 *
 * <p>When the selected user is user 0, {@link #getList()} returns itemA[0], itemB[0],
 * dummyC[0], itemX[0], itemY[10].
 *
 * <p>When the selected user is user 10, {@link #getList()} returns itemA[10], dummyB[10],
 * itemC[10], itemX[0], itemY[10].
 */
class RootItemListBuilder {
    private final UserId mSelectedUser;
    private final List<UserId> mUserIds;
    private final Multimap<String, RootItem> mItems = ArrayListMultimap.create();

    RootItemListBuilder(UserId selectedUser, List<UserId> userIds) {
        mSelectedUser = checkNotNull(selectedUser);
        mUserIds = userIds;
    }

    public void add(RootItem item) {
        mItems.put(item.stringId, item);
    }

    /**
     * Returns a lists of root items generated from the provided root items.
     */
    public List<RootItem> getList() {
        if (mUserIds.size() < 2) {
            // If we only have one user, simply return everything that has added to this builder.
            return Lists.newArrayList(mItems.values());
        }
        List<RootItem> result = Lists.newArrayList();
        // Iterates through items by item.stringId.
        for (Collection<RootItem> items : mItems.asMap().values()) {
            result.addAll(getRootItemForSelectedUser(items));
        }
        return result;
    }

    private Collection<RootItem> getRootItemForSelectedUser(Collection<RootItem> items) {
        RootItem testRootItem = items.iterator().next();
        if (!testRootItem.root.supportsCrossProfile()) {
            // If the root does not support cross-profile, return the entire list.
            return items;
        }

        // If the root supports cross-profile, we return the added root or create a dummy root if
        // it was not added for the selected user.
        for (RootItem item : items) {
            if (item.userId.equals(mSelectedUser)) {
                // If the item has and return the item if it is already in the items list.
                return Collections.singletonList(item);
            }
        }

        return Collections.singletonList(RootItem.createDummyItem(testRootItem, mSelectedUser));
    }
}
