/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.car.media;

import static com.android.car.apps.common.util.ViewUtils.removeFromParent;

import android.content.res.Resources;
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.apps.common.util.ViewUtils;
import com.android.car.arch.common.FutureData;
import com.android.car.media.browse.BrowseAdapter;
import com.android.car.media.browse.LimitedBrowseAdapter;
import com.android.car.media.common.GridSpacingItemDecoration;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.media.common.browse.MediaBrowserViewModelImpl;
import com.android.car.media.common.browse.MediaItemsRepository.MediaItemsLiveData;
import com.android.car.media.common.source.MediaSource;
import com.android.car.ui.FocusArea;
import com.android.car.ui.baselayout.Insets;
import com.android.car.uxr.LifeCycleObserverUxrContentLimiter;
import com.android.car.uxr.UxrContentLimiterImpl;

import java.util.Collection;
import java.util.List;

/**
 * A view controller that displays the media item children of a {@link MediaItemMetadata}.
 * The controller manages a recycler view where the items can be displayed as a list or a grid, as
 * well as an error icon and a message used to indicate loading and errors.
 * The content view is initialized with 0 alpha and needs to be animated or set to to full opacity
 * to become visible.
 */
public class BrowseViewController {
    private static final String TAG = "BrowseViewController";

    private final Callbacks mCallbacks;
    private final FocusArea mFocusArea;
    private final MediaItemMetadata mParentItem;
    private final MediaItemsLiveData mMediaItems;
    private final boolean mDisplayMediaItems;
    private final LifeCycleObserverUxrContentLimiter mUxrContentLimiter;
    private final View mContent;
    private final RecyclerView mBrowseList;
    private final ImageView mErrorIcon;
    private final TextView mMessage;
    private final LimitedBrowseAdapter mLimitedBrowseAdapter;

    private final int mFadeDuration;
    private final int mLoadingIndicatorDelay;

    private final boolean mSetFocusAreaHighlightBottom;

    private final Handler mHandler = new Handler();

    private final MediaActivity.ViewModel mViewModel;

    private final BrowseAdapter.Observer mBrowseAdapterObserver = new BrowseAdapter.Observer() {

        @Override
        protected void onPlayableItemClicked(@NonNull MediaItemMetadata item) {
            mCallbacks.onPlayableItemClicked(item);
        }

        @Override
        protected void onBrowsableItemClicked(@NonNull MediaItemMetadata item) {
            mCallbacks.onBrowsableItemClicked(item);
        }
    };

    /**
     * The bottom padding of the FocusArea highlight.
     */
    private int mFocusAreaHighlightBottomPadding;

    /**
     * Callbacks (implemented by the host)
     */
    public interface Callbacks {
        /**
         * Method invoked when the user clicks on a playable item
         *
         * @param item item to be played.
         */
        void onPlayableItemClicked(@NonNull MediaItemMetadata item);

        /** Invoked when the user clicks on a browsable item. */
        void onBrowsableItemClicked(@NonNull MediaItemMetadata item);

        /** Invoked when child nodes have been removed from this controller. */
        void onChildrenNodesRemoved(@NonNull BrowseViewController controller,
                @NonNull Collection<MediaItemMetadata> removedNodes);

        FragmentActivity getActivity();
    }

    private FragmentActivity getActivity() {
        return mCallbacks.getActivity();
    }

    /**
     * Creates a controller to display the children of the given parent {@link MediaItemMetadata}.
     * This parent node can have been obtained from the browse tree, or from browsing the search
     * results.
     */
    static BrowseViewController newBrowseController(Callbacks callbacks, ViewGroup container,
            @NonNull MediaItemMetadata parentItem, MediaItemsLiveData mediaItems,
            int rootBrowsableHint, int rootPlayableHint) {
        return new BrowseViewController(callbacks, container, parentItem, mediaItems,
                rootBrowsableHint, rootPlayableHint, true);
    }

    /** Creates a controller to display the top results of a search query (in a list). */
    static BrowseViewController newSearchResultsController(Callbacks callbacks, ViewGroup container,
            MediaItemsLiveData mediaItems) {
        return new BrowseViewController(callbacks, container, null, mediaItems, 0, 0, true);
    }

    /**
     * Creates a controller to "display" the children of the root: the children are actually hidden
     * since they are shown as tabs, and the controller is only used to display loading and error
     * messages.
     */
    static BrowseViewController newRootController(Callbacks callbacks, ViewGroup container,
            MediaItemsLiveData mediaItems) {
        return new BrowseViewController(callbacks, container, null, mediaItems, 0, 0, false);
    }


    private BrowseViewController(Callbacks callbacks, ViewGroup container,
            @Nullable MediaItemMetadata parentItem, MediaItemsLiveData mediaItems,
            int rootBrowsableHint, int rootPlayableHint, boolean displayMediaItems) {
        mCallbacks = callbacks;
        mParentItem = parentItem;
        mMediaItems = mediaItems;
        mDisplayMediaItems = displayMediaItems;

        LayoutInflater inflater = LayoutInflater.from(container.getContext());
        mContent = inflater.inflate(R.layout.browse_node, container, false);
        mContent.setAlpha(0f);
        container.addView(mContent);

        mLoadingIndicatorDelay = mContent.getContext().getResources()
                .getInteger(R.integer.progress_indicator_delay);
        mSetFocusAreaHighlightBottom = mContent.getContext().getResources().getBoolean(
                R.bool.set_browse_list_focus_area_highlight_above_minimized_control_bar);

        mFocusArea = mContent.findViewById(R.id.focus_area);
        mBrowseList = mContent.findViewById(R.id.browse_list);
        mErrorIcon = mContent.findViewById(R.id.error_icon);
        mMessage = mContent.findViewById(R.id.error_message);
        mFadeDuration = mContent.getContext().getResources().getInteger(
                R.integer.new_album_art_fade_in_duration);


        FragmentActivity activity = callbacks.getActivity();
        mViewModel = ViewModelProviders.of(activity).get(MediaActivity.ViewModel.class);

        mBrowseList.addItemDecoration(new GridSpacingItemDecoration(
                activity.getResources().getDimensionPixelSize(R.dimen.grid_item_spacing)));

        GridLayoutManager manager = (GridLayoutManager) mBrowseList.getLayoutManager();
        BrowseAdapter browseAdapter = new BrowseAdapter(mBrowseList.getContext());
        mLimitedBrowseAdapter = new LimitedBrowseAdapter(browseAdapter, manager,
                mBrowseAdapterObserver);
        mBrowseList.setAdapter(mLimitedBrowseAdapter);

        mUxrContentLimiter = new LifeCycleObserverUxrContentLimiter(
                new UxrContentLimiterImpl(activity, R.xml.uxr_config));
        mUxrContentLimiter.setAdapter(mLimitedBrowseAdapter);
        activity.getLifecycle().addObserver(mUxrContentLimiter);

        browseAdapter.setRootBrowsableViewType(rootBrowsableHint);
        browseAdapter.setRootPlayableViewType(rootPlayableHint);

        mMediaItems.observe(activity, mItemsObserver);
    }

    public MediaItemMetadata getParentItem() {
        return mParentItem;
    }

    /** Shares the browse adapter with the given view... #local-hack. */
    public void shareBrowseAdapterWith(RecyclerView view) {
        view.setAdapter(mLimitedBrowseAdapter);
    }

    private final Observer<FutureData<List<MediaItemMetadata>>> mItemsObserver =
            this::onItemsUpdate;

    View getContent() {
        return mContent;
    }

    String getDebugInfo() {
        StringBuilder log = new StringBuilder();
        log.append("[");
        log.append((mParentItem != null) ? mParentItem.getTitle() : "Root");
        log.append("]");
        FutureData<List<MediaItemMetadata>> children = mMediaItems.getValue();
        if (children == null) {
            log.append(" null future data");
        } else if (children.isLoading()) {
            log.append(" loading");
        } else if (children.getData() == null) {
            log.append(" null list");
        } else {
            List<MediaItemMetadata> nodes = children.getData();
            log.append(" ");
            log.append(nodes.size());
            log.append(" {");
            if (nodes.size() > 0) {
                log.append(nodes.get(0).getTitle().toString());
            }
            if (nodes.size() > 1) {
                log.append(", ");
                log.append(nodes.get(1).getTitle().toString());
            }
            if (nodes.size() > 2) {
                log.append(", ...");
            }
            log.append(" }");
        }
        return log.toString();
    }

    void destroy() {
        mCallbacks.getActivity().getLifecycle().removeObserver(mUxrContentLimiter);
        mMediaItems.removeObserver(mItemsObserver);
        removeFromParent(mContent);
    }

    private Runnable mLoadingIndicatorRunnable = new Runnable() {
        @Override
        public void run() {
            mMessage.setText(R.string.browser_loading);
            ViewUtils.showViewAnimated(mMessage, mFadeDuration);
        }
    };

    private void startLoadingIndicator() {
        // Display the indicator after a certain time, to avoid flashing the indicator constantly,
        // even when performance is acceptable.
        mHandler.postDelayed(mLoadingIndicatorRunnable, mLoadingIndicatorDelay);
    }

    private void stopLoadingIndicator() {
        mHandler.removeCallbacks(mLoadingIndicatorRunnable);
        ViewUtils.hideViewAnimated(mMessage, mFadeDuration);
    }

    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        int leftPadding = mBrowseList.getPaddingLeft();
        int rightPadding = mBrowseList.getPaddingRight();
        int bottomPadding = mBrowseList.getPaddingBottom();
        mBrowseList.setPadding(leftPadding, insets.getTop(), rightPadding, bottomPadding);
        if (bottomPadding > mFocusAreaHighlightBottomPadding) {
            mFocusAreaHighlightBottomPadding = bottomPadding;
        }
        mFocusArea.setHighlightPadding(
                leftPadding, insets.getTop(), rightPadding, mFocusAreaHighlightBottomPadding);
        mFocusArea.setBoundsOffset(leftPadding, insets.getTop(), rightPadding, bottomPadding);
    }

    void onPlaybackControlsChanged(boolean visible) {
        int leftPadding = mBrowseList.getPaddingLeft();
        int topPadding = mBrowseList.getPaddingTop();
        int rightPadding = mBrowseList.getPaddingRight();
        Resources res = getActivity().getResources();
        int bottomPadding = visible
                ? res.getDimensionPixelOffset(R.dimen.browse_fragment_bottom_padding) : 0;
        mBrowseList.setPadding(leftPadding, topPadding, rightPadding, bottomPadding);
        int highlightBottomPadding = mSetFocusAreaHighlightBottom ? bottomPadding : 0;
        if (highlightBottomPadding > mFocusAreaHighlightBottomPadding) {
            mFocusAreaHighlightBottomPadding = highlightBottomPadding;
        }
        mFocusArea.setHighlightPadding(
                leftPadding, topPadding, rightPadding, mFocusAreaHighlightBottomPadding);
        // Set the bottom offset to bottomPadding regardless of mSetFocusAreaHighlightBottom so that
        // RotaryService can find the correct target when the user nudges the rotary controller.
        mFocusArea.setBoundsOffset(leftPadding, topPadding, rightPadding, bottomPadding);

        ViewGroup.MarginLayoutParams messageLayout =
                (ViewGroup.MarginLayoutParams) mMessage.getLayoutParams();
        messageLayout.bottomMargin = bottomPadding;
        mMessage.setLayoutParams(messageLayout);
    }

    private String getErrorMessage() {
        if (/*root*/ !mDisplayMediaItems) {
            MediaSource mediaSource = mViewModel.getBrowsedMediaSource().getValue();
            return getActivity().getString(
                    R.string.cannot_connect_to_app,
                    mediaSource != null
                            ? mediaSource.getDisplayName()
                            : getActivity().getString(
                                    R.string.unknown_media_provider_name));
        } else {
            return getActivity().getString(R.string.unknown_error);
        }
    }

    private void onItemsUpdate(@Nullable FutureData<List<MediaItemMetadata>> futureData) {
        if (futureData == null || futureData.isLoading()) {
            ViewUtils.hideViewAnimated(mErrorIcon, 0);
            ViewUtils.hideViewAnimated(mMessage, 0);

            // TODO(b/139759881) build a jank-free animation of the transition.
            mBrowseList.setAlpha(0f);
            mLimitedBrowseAdapter.submitItems(null, null);

            if (futureData != null) {
                startLoadingIndicator();
            }
            return;
        }

        stopLoadingIndicator();

        List<MediaItemMetadata> items = MediaBrowserViewModelImpl.filterItems(
                /*root*/ !mDisplayMediaItems, futureData.getData());
        if (mDisplayMediaItems) {
            mLimitedBrowseAdapter.submitItems(mParentItem, items);

            List<MediaItemMetadata> lastNodes =
                    MediaBrowserViewModelImpl.selectBrowseableItems(futureData.getPastData());
            Collection<MediaItemMetadata> removedNodes =
                    MediaBrowserViewModelImpl.computeRemovedItems(lastNodes, items);
            if (!removedNodes.isEmpty()) {
                mCallbacks.onChildrenNodesRemoved(this, removedNodes);
            }
        }

        int duration = mFadeDuration;
        if (items == null) {
            mMessage.setText(getErrorMessage());
            ViewUtils.hideViewAnimated(mBrowseList, duration);
            ViewUtils.showViewAnimated(mMessage, duration);
            ViewUtils.showViewAnimated(mErrorIcon, duration);
        } else if (items.isEmpty()) {
            mMessage.setText(R.string.nothing_to_play);
            ViewUtils.hideViewAnimated(mBrowseList, duration);
            ViewUtils.hideViewAnimated(mErrorIcon, duration);
            ViewUtils.showViewAnimated(mMessage, duration);
        } else {
            ViewUtils.showViewAnimated(mBrowseList, duration);
            ViewUtils.hideViewAnimated(mErrorIcon, duration);
            ViewUtils.hideViewAnimated(mMessage, duration);
        }

        if (Log.isLoggable(TAG, Log.VERBOSE)) {
            Log.v(TAG, "onItemsUpdate " + getDebugInfo());
        }
    }
}
