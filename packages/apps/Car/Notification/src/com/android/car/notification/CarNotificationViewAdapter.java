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
package com.android.car.notification;

import android.annotation.NonNull;
import android.app.Notification;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.notification.template.CarNotificationBaseViewHolder;
import com.android.car.notification.template.CarNotificationFooterViewHolder;
import com.android.car.notification.template.CarNotificationHeaderViewHolder;
import com.android.car.notification.template.GroupNotificationViewHolder;
import com.android.car.notification.template.GroupSummaryNotificationViewHolder;
import com.android.car.notification.template.MessageNotificationViewHolder;
import com.android.car.ui.recyclerview.ContentLimitingAdapter;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Notification data adapter that binds a notification to the corresponding view.
 */
public class CarNotificationViewAdapter extends ContentLimitingAdapter<RecyclerView.ViewHolder>
        implements PreprocessingManager.CallStateListener {
    private static final String TAG = "CarNotificationAdapter";

    private final Context mContext;
    private final LayoutInflater mInflater;
    private final int mMaxNumberGroupChildrenShown;
    private final boolean mIsGroupNotificationAdapter;

    // book keeping expanded notification groups
    private final List<String> mExpandedNotifications = new ArrayList<>();
    private final CarNotificationItemController mNotificationItemController;

    private List<NotificationGroup> mNotifications = new ArrayList<>();
    private LinearLayoutManager mLayoutManager;
    private RecyclerView.RecycledViewPool mViewPool;
    private CarUxRestrictions mCarUxRestrictions;
    private NotificationClickHandlerFactory mClickHandlerFactory;
    private NotificationDataManager mNotificationDataManager;
    private boolean mIsInCall;
    // Suppress binding views to child notifications in the process of being removed.
    private Set<AlertEntry> mChildNotificationsBeingCleared = new HashSet<>();
    private boolean mHasHeaderAndFooter;
    private int mMaxItems = ContentLimitingAdapter.UNLIMITED;

    /**
     * Constructor for a notification adapter.
     * Can be used both by the root notification list view, or a grouped notification view.
     *
     * @param context the context for resources and inflating views
     * @param isGroupNotificationAdapter true if this adapter is used by a grouped notification view
     * @param notificationItemController shared logic to control notification items.
     */
    public CarNotificationViewAdapter(Context context, boolean isGroupNotificationAdapter,
            @Nullable CarNotificationItemController notificationItemController) {
        mContext = context;
        mInflater = LayoutInflater.from(context);
        mMaxNumberGroupChildrenShown =
                mContext.getResources().getInteger(R.integer.max_group_children_number);
        mIsGroupNotificationAdapter = isGroupNotificationAdapter;
        mNotificationItemController = notificationItemController;
        setHasStableIds(true);
        if (!mIsGroupNotificationAdapter) {
            mViewPool = new RecyclerView.RecycledViewPool();
        }

        PreprocessingManager.getInstance(context).addCallStateListener(this::onCallStateChanged);
    }

    @Override
    public void onAttachedToRecyclerView(@NonNull RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);
        mLayoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
    }

    @Override
    public void onDetachedFromRecyclerView(@NonNull RecyclerView recyclerView) {
        super.onDetachedFromRecyclerView(recyclerView);
        mLayoutManager = null;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolderImpl(@NonNull ViewGroup parent, int viewType) {
        RecyclerView.ViewHolder viewHolder;
        View view;
        switch (viewType) {
            case NotificationViewType.HEADER:
                view = mInflater.inflate(R.layout.notification_header_template, parent, false);
                viewHolder = new CarNotificationHeaderViewHolder(mContext, view,
                        mNotificationItemController);
                break;
            case NotificationViewType.FOOTER:
                view = mInflater.inflate(R.layout.notification_footer_template, parent, false);
                viewHolder = new CarNotificationFooterViewHolder(mContext, view,
                        mNotificationItemController);
                break;
            default:
                CarNotificationTypeItem carNotificationTypeItem = CarNotificationTypeItem.of(
                        viewType);
                view = mInflater.inflate(
                        carNotificationTypeItem.getNotificationCenterTemplate(), parent, false);
                viewHolder = carNotificationTypeItem.getViewHolder(view, mClickHandlerFactory);
        }

        return viewHolder;
    }

    @Override
    public void onBindViewHolderImpl(RecyclerView.ViewHolder holder, int position) {
        NotificationGroup notificationGroup = mNotifications.get(position);
        int viewType = holder.getItemViewType();
        switch (viewType) {
            case NotificationViewType.HEADER:
                ((CarNotificationHeaderViewHolder) holder).bind(hasNotifications());
                return;
            case NotificationViewType.FOOTER:
                ((CarNotificationFooterViewHolder) holder).bind(hasNotifications());
                return;
            case NotificationViewType.GROUP_EXPANDED:
                ((GroupNotificationViewHolder) holder)
                        .bind(notificationGroup, this, /* isExpanded= */ true);
                return;
            case NotificationViewType.GROUP_COLLAPSED:
                ((GroupNotificationViewHolder) holder)
                        .bind(notificationGroup, this, /* isExpanded= */ false);
                return;
            case NotificationViewType.GROUP_SUMMARY:
                ((CarNotificationBaseViewHolder) holder).setHideDismissButton(true);
                ((GroupSummaryNotificationViewHolder) holder).bind(notificationGroup);
                return;
        }

        CarNotificationTypeItem carNotificationTypeItem = CarNotificationTypeItem.of(viewType);
        AlertEntry alertEntry = notificationGroup.getSingleNotification();

        if (shouldRestrictMessagePreview() && (viewType == NotificationViewType.MESSAGE
                || viewType == NotificationViewType.MESSAGE_IN_GROUP)) {
            ((MessageNotificationViewHolder) holder)
                    .bindRestricted(alertEntry, /* isInGroup= */ false, /* isHeadsUp= */false);
        } else {
            carNotificationTypeItem.bind(alertEntry, false, (CarNotificationBaseViewHolder) holder);
        }
    }

    @Override
    public int getItemViewTypeImpl(int position) {
        NotificationGroup notificationGroup = mNotifications.get(position);

        if (notificationGroup.isHeader()) {
            return NotificationViewType.HEADER;
        }

        if (notificationGroup.isFooter()) {
            return NotificationViewType.FOOTER;
        }

        if (notificationGroup.isGroup()) {
            if (mExpandedNotifications.contains(notificationGroup.getGroupKey())) {
                return NotificationViewType.GROUP_EXPANDED;
            } else {
                return NotificationViewType.GROUP_COLLAPSED;
            }
        } else if (mExpandedNotifications.contains(notificationGroup.getGroupKey())) {
            // when there are 2 notifications left in the expanded notification and one of them is
            // removed at that time the item type changes from group to normal and hence the
            // notification should be removed from expanded notifications.
            setExpanded(notificationGroup.getGroupKey(), false);
        }

        Notification notification =
                notificationGroup.getSingleNotification().getNotification();
        Bundle extras = notification.extras;

        String category = notification.category;
        if (category != null) {
            switch (category) {
                case Notification.CATEGORY_CALL:
                    return NotificationViewType.CALL;
                case Notification.CATEGORY_CAR_EMERGENCY:
                    return NotificationViewType.CAR_EMERGENCY;
                case Notification.CATEGORY_CAR_WARNING:
                    return NotificationViewType.CAR_WARNING;
                case Notification.CATEGORY_CAR_INFORMATION:
                    return mIsGroupNotificationAdapter
                            ? NotificationViewType.CAR_INFORMATION_IN_GROUP
                            : NotificationViewType.CAR_INFORMATION;
                case Notification.CATEGORY_MESSAGE:
                    return mIsGroupNotificationAdapter
                            ? NotificationViewType.MESSAGE_IN_GROUP : NotificationViewType.MESSAGE;
                default:
                    break;
            }
        }

        // progress
        int progressMax = extras.getInt(Notification.EXTRA_PROGRESS_MAX);
        boolean isIndeterminate = extras.getBoolean(
                Notification.EXTRA_PROGRESS_INDETERMINATE);
        boolean hasValidProgress = isIndeterminate || progressMax != 0;
        boolean isProgress = extras.containsKey(Notification.EXTRA_PROGRESS)
                && extras.containsKey(Notification.EXTRA_PROGRESS_MAX)
                && hasValidProgress
                && !notification.hasCompletedProgress();
        if (isProgress) {
            return mIsGroupNotificationAdapter
                    ? NotificationViewType.PROGRESS_IN_GROUP : NotificationViewType.PROGRESS;
        }

        // inbox
        boolean isInbox = extras.containsKey(Notification.EXTRA_TITLE_BIG)
                && extras.containsKey(Notification.EXTRA_SUMMARY_TEXT);
        if (isInbox) {
            return mIsGroupNotificationAdapter
                    ? NotificationViewType.INBOX_IN_GROUP : NotificationViewType.INBOX;
        }

        // group summary
        boolean isGroupSummary = notificationGroup.getChildTitles() != null;
        if (isGroupSummary) {
            return NotificationViewType.GROUP_SUMMARY;
        }

        // the big text and big picture styles are fallen back to basic template in car
        // i.e. setting the big text and big picture does not have an effect
        boolean isBigText = extras.containsKey(Notification.EXTRA_BIG_TEXT);
        if (isBigText) {
            Log.i(TAG, "Big text style is not supported as a car notification");
        }
        boolean isBigPicture = extras.containsKey(Notification.EXTRA_PICTURE);
        if (isBigPicture) {
            Log.i(TAG, "Big picture style is not supported as a car notification");
        }

        // basic, big text, big picture
        return mIsGroupNotificationAdapter
                ? NotificationViewType.BASIC_IN_GROUP : NotificationViewType.BASIC;
    }

    @Override
    public int getUnrestrictedItemCount() {
        return mNotifications.size();
    }

    @Override
    public void setMaxItems(int maxItems) {
        if (maxItems == ContentLimitingAdapter.UNLIMITED || !mHasHeaderAndFooter) {
            mMaxItems = maxItems;
        } else {
            // Adding one so the notification header doesn't count toward the limit.
            mMaxItems = maxItems + 1;
        }
        super.setMaxItems(mMaxItems);
    }

    @Override
    protected int getScrollToPositionWhenRestricted() {
        if (mLayoutManager == null) {
            return -1;
        }
        int firstItem = mLayoutManager.findFirstVisibleItemPosition();
        if (firstItem >= getItemCount() - 1) {
            return getItemCount() - 1;
        }
        return -1;
    }

    @Override
    public long getItemId(int position) {
        NotificationGroup notificationGroup = mNotifications.get(position);
        if (notificationGroup.isHeader()) {
            return 0;
        }

        if (notificationGroup.isFooter()) {
            return 1;
        }

        return notificationGroup.isGroup()
                ? notificationGroup.getGroupKey().hashCode()
                : notificationGroup.getSingleNotification().getKey().hashCode();
    }

    /**
     * Set the expansion state of a group notification given its group key.
     *
     * @param groupKey the unique identifier of a {@link NotificationGroup}
     * @param isExpanded whether the group notification should be expanded.
     */
    public void setExpanded(String groupKey, boolean isExpanded) {
        if (isExpanded(groupKey) == isExpanded) {
            return;
        }

        if (isExpanded) {
            mExpandedNotifications.add(groupKey);
        } else {
            mExpandedNotifications.remove(groupKey);
        }
    }

    /**
     * Collapses all expanded groups.
     */
    public void collapseAllGroups() {
        if (!mExpandedNotifications.isEmpty()) {
            mExpandedNotifications.clear();
        }
    }

    /**
     * Returns whether the notification is expanded given its group key.
     */
    boolean isExpanded(String groupKey) {
        return mExpandedNotifications.contains(groupKey);
    }

    /**
     * Gets the current {@link CarUxRestrictions}.
     */
    public CarUxRestrictions getCarUxRestrictions() {
        return mCarUxRestrictions;
    }

    /**
     * Updates notifications and update views.
     *
     * @param setRecyclerViewListHeaderAndFooter sets the header and footer on the entire list of
     * items within the recycler view. This is NOT the header/footer for the grouped notifications.
     */
    public void setNotifications(List<NotificationGroup> notifications,
            boolean setRecyclerViewListHeaderAndFooter) {

        notifications.removeIf(notificationGroup ->
                mChildNotificationsBeingCleared.contains(notificationGroup.getSingleNotification())
        );

        List<NotificationGroup> notificationGroupList = new ArrayList<>(notifications);

        if (setRecyclerViewListHeaderAndFooter) {
            // add header as the first item of the list.
            notificationGroupList.add(0, createNotificationHeader());
            // add footer as the last item of the list.
            notificationGroupList.add(createNotificationFooter());
            mHasHeaderAndFooter = true;
        } else {
            mHasHeaderAndFooter = false;
        }

        DiffUtil.DiffResult diffResult =
                DiffUtil.calculateDiff(
                        new CarNotificationDiff(mContext, mNotifications, notificationGroupList, mMaxItems),
                        /* detectMoves= */ false);
        mNotifications = notificationGroupList;
        updateUnderlyingDataChanged(getUnrestrictedItemCount(), /* newAnchorIndex= */ 0);
        diffResult.dispatchUpdatesTo(this);
    }
    /**
     * Sets child notifications of the group notification that is in the process of being cleared.
     * This prevents these child notifications from appearing briefly while the clearing process is
     * running.
     *
     * <p>NOTE: To reset mChildNotificationsBeingCleared, pass an empty Set instead of null.</p>
     *
     * @param notificationsBeingCleared
     */
    protected void setChildNotificationsBeingCleared(@NonNull Set notificationsBeingCleared) {
        mChildNotificationsBeingCleared = notificationsBeingCleared;
    }

    /**
     * Notification list has header and footer by default. Therefore the min number of items in the
     * adapter will always be two. If there are any notifications present the size will be more than
     * two.
     */
    private boolean hasNotifications() {
        return getItemCount() > 2;
    }

    private NotificationGroup createNotificationHeader() {
        NotificationGroup notificationGroupWithHeader = new NotificationGroup();
        notificationGroupWithHeader.setHeader(true);
        notificationGroupWithHeader.setGroupKey("notification_header");
        return notificationGroupWithHeader;
    }

    private NotificationGroup createNotificationFooter() {
        NotificationGroup notificationGroupWithFooter = new NotificationGroup();
        notificationGroupWithFooter.setFooter(true);
        notificationGroupWithFooter.setGroupKey("notification_footer");
        return notificationGroupWithFooter;
    }

    /** Implementation of {@link PreprocessingManager.CallStateListener} **/
    @Override
    public void onCallStateChanged(boolean isInCall) {
        if (isInCall != mIsInCall) {
            mIsInCall = isInCall;
            notifyDataSetChanged();
        }
    }

    /**
     * Sets the current {@link CarUxRestrictions}.
     */
    public void setCarUxRestrictions(CarUxRestrictions carUxRestrictions) {
        Log.d(TAG, "setCarUxRestrictions");
        mCarUxRestrictions = carUxRestrictions;
        notifyDataSetChanged();
    }

    /**
     * Helper method that determines whether a notification is a messaging notification and
     * should have restricted content (no message preview).
     */
    private boolean shouldRestrictMessagePreview() {
        return mCarUxRestrictions != null && (mCarUxRestrictions.getActiveRestrictions()
                & CarUxRestrictions.UX_RESTRICTIONS_NO_TEXT_MESSAGE) != 0;
    }

    /**
     * Get root recycler view's view pool so that the child recycler view can share the same
     * view pool with the parent.
     */
    public RecyclerView.RecycledViewPool getViewPool() {
        if (mIsGroupNotificationAdapter) {
            // currently only support one level of expansion.
            throw new IllegalStateException("CarNotificationViewAdapter is a child adapter; "
                    + "its view pool should not be reused.");
        }
        return mViewPool;
    }

    /**
     * Sets the NotificationClickHandlerFactory that allows for a hook to run a block off code
     * when  the notification is clicked. This is useful to dismiss a screen after
     * a notification list clicked.
     */
    public void setClickHandlerFactory(NotificationClickHandlerFactory clickHandlerFactory) {
        mClickHandlerFactory = clickHandlerFactory;
    }

    /**
     * Sets NotificationDataManager that handles additional states for notifications such as "seen",
     * and muting a messaging type notification.
     *
     * @param notificationDataManager An instance of NotificationDataManager.
     */
    public void setNotificationDataManager(NotificationDataManager notificationDataManager) {
        mNotificationDataManager = notificationDataManager;
    }

    /**
     * Set notification groups as seen.
     *
     * @param start Initial adapter position of the notification groups.
     * @param end Final adapter position of the notification groups.
     */
    /* package */ void setNotificationsAsSeen(int start, int end) {
        start = Math.max(start, 0);
        end = Math.min(end, mNotifications.size() - 1);

        if (mNotificationDataManager != null) {
            List<AlertEntry> notifications = new ArrayList();
            for (int i = start; i <= end; i++) {
                notifications.addAll(mNotifications.get(i).getChildNotifications());
            }
            mNotificationDataManager.setNotificationsAsSeen(notifications);
        }
    }

    @Override
    public int getConfigurationId() {
        return R.id.notification_list_uxr_config;
    }
}
