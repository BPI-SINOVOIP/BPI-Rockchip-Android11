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

package com.android.car.media;

import static com.android.car.apps.common.util.ViewUtils.showHideViewAnimated;

import android.car.content.pm.CarPackageManager;
import android.content.Context;
import android.support.v4.media.MediaBrowserCompat;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.apps.common.util.ViewUtils;
import com.android.car.apps.common.util.ViewUtils.ViewAnimEndListener;
import com.android.car.arch.common.FutureData;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.media.common.browse.MediaBrowserViewModelImpl;
import com.android.car.media.common.browse.MediaItemsRepository;
import com.android.car.media.common.browse.MediaItemsRepository.MediaItemsLiveData;
import com.android.car.media.common.source.MediaBrowserConnector.BrowsingState;
import com.android.car.media.common.source.MediaSource;
import com.android.car.media.widgets.AppBarController;
import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.toolbar.Toolbar;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Stack;

/**
 * Controls the views of the {@link MediaActivity}.
 * TODO: finish moving control code out of MediaActivity (b/179292809).
 */
public class MediaActivityController extends ViewControllerBase {

    private static final String TAG = "MediaActivityCtr";

    private final MediaItemsRepository mMediaItemsRepository;
    private final Callbacks mCallbacks;
    private final ViewGroup mBrowseArea;
    private Insets mCarUiInsets;
    private boolean mPlaybackControlsVisible;

    private final Map<MediaItemMetadata, BrowseViewController> mBrowseViewControllersByNode =
            new HashMap<>();

    // Controllers that should be destroyed once their view is hidden.
    private final Map<View, BrowseViewController> mBrowseViewControllersToDestroy = new HashMap<>();

    private final BrowseViewController mRootLoadingController;
    private final BrowseViewController mSearchResultsController;

    /**
     * Stores the reference to {@link MediaActivity.ViewModel#getBrowseStack}.
     * Updated in {@link #onMediaSourceChanged}.
     */
    private Stack<MediaItemMetadata> mBrowseStack;
    /**
     * Stores the reference to {@link MediaActivity.ViewModel#getSearchStack}.
     * Updated in {@link #onMediaSourceChanged}.
     */
    private Stack<MediaItemMetadata> mSearchStack;
    private final MediaActivity.ViewModel mViewModel;

    private int mRootBrowsableHint;
    private int mRootPlayableHint;
    private boolean mBrowseTreeHasChildren;
    private boolean mAcceptTabSelection = true;

    /**
     * Media items to display as tabs. If null, it means we haven't finished loading them yet. If
     * empty, it means there are no tabs to show
     */
    @Nullable
    private List<MediaItemMetadata> mTopItems;

    private final Observer<BrowsingState> mMediaBrowsingObserver =
            this::onMediaBrowsingStateChanged;

    /**
     * Callbacks (implemented by the hosting Activity)
     */
    public interface Callbacks {

        /** Invoked when the user clicks on a browsable item. */
        void onPlayableItemClicked(@NonNull MediaItemMetadata item);

        /** Called once the list of the root node's children has been loaded. */
        void onRootLoaded();

        /** Returns the activity. */
        FragmentActivity getActivity();
    }

    /**
     * Moves the user one level up in the browse/search tree. Returns whether that was possible.
     */
    private boolean navigateBack() {
        boolean result = false;
        if (!isAtTopStack()) {
            hideAndDestroyControllerForItem(getStack().pop());

            // Show the parent (if any)
            showCurrentNode(true);

            if (isAtTopStack() && mViewModel.isSearching()) {
                showSearchResults(true);
            }

            updateAppBar();
            result = true;
        }
        return result;
    }

    private void reopenSearch() {
        clearStack(mSearchStack);
        showSearchResults(true);
        updateAppBar();
    }

    private FragmentActivity getActivity() {
        return mCallbacks.getActivity();
    }

    /** Returns the browse or search stack. */
    private Stack<MediaItemMetadata> getStack() {
        return mViewModel.isSearching() ? mSearchStack : mBrowseStack;
    }

    /**
     * @return whether the user is at the top of the browsing stack.
     */
    private boolean isAtTopStack() {
        if (mViewModel.isSearching()) {
            return mSearchStack.isEmpty();
        } else {
            // The mBrowseStack stack includes the tab...
            return mBrowseStack.size() <= 1;
        }
    }

    private void clearMediaSource() {
        showSearchMode(false);
        for (BrowseViewController controller : mBrowseViewControllersByNode.values()) {
            controller.destroy();
        }
        mBrowseViewControllersByNode.clear();
        mBrowseTreeHasChildren = false;
    }

    private void updateSearchQuery(@Nullable String query) {
        mMediaItemsRepository.setSearchQuery(query);
    }

    /**
     * Clears search state, removes any UI elements from previous results.
     */
    @Override
    void onMediaSourceChanged(@Nullable MediaSource mediaSource) {
        super.onMediaSourceChanged(mediaSource);

        updateTabs((mediaSource != null) ? null : new ArrayList<>());

        mSearchStack = mViewModel.getSearchStack();
        mBrowseStack = mViewModel.getBrowseStack();

        updateAppBar();
    }

    private void onMediaBrowsingStateChanged(BrowsingState newBrowsingState) {
        switch (newBrowsingState.mConnectionStatus) {
            case CONNECTING:
                break;
            case CONNECTED:
                MediaBrowserCompat browser = newBrowsingState.mBrowser;
                mRootBrowsableHint = MediaBrowserViewModelImpl.getRootBrowsableHint(browser);
                mRootPlayableHint = MediaBrowserViewModelImpl.getRootPlayableHint(browser);

                boolean canSearch = MediaBrowserViewModelImpl.getSupportsSearch(browser);
                mAppBarController.setSearchSupported(canSearch);
                break;

            case DISCONNECTING:
            case REJECTED:
            case SUSPENDED:
                clearMediaSource();
                break;
        }

        MediaSource savedSource = mViewModel.getBrowsedMediaSource().getValue();
        MediaSource mediaSource = newBrowsingState.mMediaSource;
        if (Log.isLoggable(TAG, Log.INFO)) {
            Log.i(TAG, "MediaSource changed from " + savedSource + " to " + mediaSource);
        }

        mViewModel.saveBrowsedMediaSource(mediaSource);
        onMediaSourceChanged(mediaSource);
    }


    MediaActivityController(Callbacks callbacks, MediaItemsRepository mediaItemsRepo,
            CarPackageManager carPackageManager, ViewGroup container) {
        super(callbacks.getActivity(), carPackageManager, container, R.layout.fragment_browse);

        FragmentActivity activity = callbacks.getActivity();
        mCallbacks = callbacks;
        mMediaItemsRepository = mediaItemsRepo;
        mViewModel = ViewModelProviders.of(activity).get(MediaActivity.ViewModel.class);
        mSearchStack = mViewModel.getSearchStack();
        mBrowseStack = mViewModel.getBrowseStack();
        mBrowseArea = mContent.requireViewById(R.id.browse_content_area);

        MediaItemsLiveData rootMediaItems = mediaItemsRepo.getRootMediaItems();
        mRootLoadingController = BrowseViewController.newRootController(
                mBrowseCallbacks, mBrowseArea, rootMediaItems);
        mRootLoadingController.getContent().setAlpha(1f);

        mSearchResultsController = BrowseViewController.newSearchResultsController(
                mBrowseCallbacks, mBrowseArea, mMediaItemsRepository.getSearchMediaItems());

        boolean showingSearch = mViewModel.isShowingSearchResults();
        ViewUtils.setVisible(mSearchResultsController.getContent(), showingSearch);
        if (showingSearch) {
            mSearchResultsController.getContent().setAlpha(1f);
        }

        mAppBarController.setListener(mAppBarListener);
        mAppBarController.setSearchQuery(mViewModel.getSearchQuery());
        if (mAppBarController.canShowSearchResultsView()) {
            // TODO(b/180441965) eliminate the need to create a different view and use
            //     mSearchResultsController.getContent() instead.
            RecyclerView toolbarSearchResultsView = new RecyclerView(activity);
            mSearchResultsController.shareBrowseAdapterWith(toolbarSearchResultsView);

            ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            toolbarSearchResultsView.setLayoutParams(params);
            toolbarSearchResultsView.setLayoutManager(new LinearLayoutManager(activity));
            toolbarSearchResultsView.setBackground(
                    activity.getDrawable(R.drawable.car_ui_ime_wide_screen_background));

            mAppBarController.setSearchResultsView(toolbarSearchResultsView);
        }

        updateAppBar();

        // Observe forever ensures the caches are destroyed even while the activity isn't resumed.
        mediaItemsRepo.getBrowsingState().observeForever(mMediaBrowsingObserver);

        rootMediaItems.observe(activity, this::onRootMediaItemsUpdate);
        mViewModel.getMiniControlsVisible().observe(activity, this::onPlaybackControlsChanged);
    }

    void onDestroy() {
        mMediaItemsRepository.getBrowsingState().removeObserver(mMediaBrowsingObserver);
    }

    private AppBarController.AppBarListener mAppBarListener = new BasicAppBarListener() {
        @Override
        public void onTabSelected(MediaItemMetadata item) {
            if (mAcceptTabSelection && (item != null) && (item != mViewModel.getSelectedTab())) {
                clearStack(mBrowseStack);
                mBrowseStack.push(item);
                showCurrentNode(true);
            }
        }

        @Override
        public void onSearchSelection() {
            if (mViewModel.isSearching()) {
                reopenSearch();
            } else {
                showSearchMode(true);
                updateAppBar();
            }
        }

        @Override
        public void onSearch(String query) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "onSearch: " + query);
            }
            mViewModel.setSearchQuery(query);
            updateSearchQuery(query);
        }
    };

    private final BrowseViewController.Callbacks mBrowseCallbacks =
            new BrowseViewController.Callbacks() {
        @Override
        public void onPlayableItemClicked(@NonNull MediaItemMetadata item) {
            hideKeyboard();
            mCallbacks.onPlayableItemClicked(item);
        }

        @Override
        public void onBrowsableItemClicked(@NonNull MediaItemMetadata item) {
            hideKeyboard();
            navigateInto(item);
        }

        @Override
            public void onChildrenNodesRemoved(@NonNull BrowseViewController controller,
                    @NonNull Collection<MediaItemMetadata> removedNodes) {

            if (mBrowseStack.contains(controller.getParentItem())) {
                for (MediaItemMetadata node : removedNodes) {
                    int indexOfNode = mBrowseStack.indexOf(node);
                    if (indexOfNode >= 0) {
                        clearStack(mBrowseStack.subList(indexOfNode, mBrowseStack.size()));
                        if (!mViewModel.isShowingSearchResults()) {
                            showCurrentNode(true);
                            updateAppBar();
                        }
                        break; // The stack contains at most one of the removed nodes.
                    }
                }
            }
        }

        @Override
        public FragmentActivity getActivity() {
            return mCallbacks.getActivity();
        }
    };

    private final ViewAnimEndListener mViewAnimEndListener = view -> {
        BrowseViewController toDestroy = mBrowseViewControllersToDestroy.remove(view);
        if (toDestroy != null) {
            toDestroy.destroy();
        }
    };

    boolean onBackPressed() {
        boolean success = navigateBack();
        if (!success && mViewModel.isSearching()) {
            showSearchMode(false);
            updateAppBar();
            return true;
        }
        return success;
    }

    boolean browseTreeHasChildren() {
        return mBrowseTreeHasChildren;
    }

    private void navigateInto(@NonNull MediaItemMetadata item) {
        showSearchResults(false);

        // Hide the current node (parent)
        showCurrentNode(false);

        // Make item the current node
        getStack().push(item);

        // Show the current node (item)
        showCurrentNode(true);

        updateAppBar();
    }

    private BrowseViewController getControllerForItem(@NonNull MediaItemMetadata item) {
        BrowseViewController controller = mBrowseViewControllersByNode.get(item);
        if (controller == null) {
            controller = BrowseViewController.newBrowseController(mBrowseCallbacks, mBrowseArea,
                    item, mMediaItemsRepository.getMediaChildren(item.getId()), mRootBrowsableHint,
                    mRootPlayableHint);

            if (mCarUiInsets != null) {
                controller.onCarUiInsetsChanged(mCarUiInsets);
            }
            controller.onPlaybackControlsChanged(mPlaybackControlsVisible);

            mBrowseViewControllersByNode.put(item, controller);
        }
        return controller;
    }

    private void showCurrentNode(boolean show) {
        MediaItemMetadata currentNode = getCurrentMediaItem();
        if (currentNode == null) {
            return;
        }
        // Only create a controller to show it.
        BrowseViewController controller = show ? getControllerForItem(currentNode) :
                mBrowseViewControllersByNode.get(currentNode);

        if (controller != null) {
            showHideViewAnimated(show, controller.getContent(), mFadeDuration,
                    mViewAnimEndListener);
        }
    }

    private void showSearchResults(boolean show) {
        if (mViewModel.isShowingSearchResults() != show) {
            mViewModel.setShowingSearchResults(show);
            showHideViewAnimated(show, mSearchResultsController.getContent(), mFadeDuration, null);
        }
    }

    private void showSearchMode(boolean show) {
        if (mViewModel.isSearching() != show) {
            if (show) {
                showCurrentNode(false);
            }

            mViewModel.setSearching(show);
            showSearchResults(show);

            if (!show) {
                showCurrentNode(true);
            }
        }
    }

    /**
     * @return the current item being displayed
     */
    @Nullable
    private MediaItemMetadata getCurrentMediaItem() {
        Stack<MediaItemMetadata> stack = getStack();
        return stack.isEmpty() ? null : stack.lastElement();
    }

    @Override
    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        mCarUiInsets = insets;
        for (BrowseViewController controller : mBrowseViewControllersByNode.values()) {
            controller.onCarUiInsetsChanged(mCarUiInsets);
        }
        mRootLoadingController.onCarUiInsetsChanged(mCarUiInsets);
        mSearchResultsController.onCarUiInsetsChanged(mCarUiInsets);
    }

    void onPlaybackControlsChanged(boolean visible) {
        mPlaybackControlsVisible = visible;
        for (BrowseViewController controller : mBrowseViewControllersByNode.values()) {
            controller.onPlaybackControlsChanged(mPlaybackControlsVisible);
        }
        mRootLoadingController.onPlaybackControlsChanged(mPlaybackControlsVisible);
        mSearchResultsController.onPlaybackControlsChanged(mPlaybackControlsVisible);
    }

    private void hideKeyboard() {
        InputMethodManager in =
                (InputMethodManager) getActivity().getSystemService(Context.INPUT_METHOD_SERVICE);
        in.hideSoftInputFromWindow(mContent.getWindowToken(), 0);
    }

    private void hideAndDestroyControllerForItem(@Nullable MediaItemMetadata item) {
        if (item == null) {
            return;
        }
        BrowseViewController controller = mBrowseViewControllersByNode.get(item);
        if (controller == null) {
            return;
        }

        if (controller.getContent().getVisibility() == View.VISIBLE) {
            View view = controller.getContent();
            mBrowseViewControllersToDestroy.put(view, controller);
            showHideViewAnimated(false, view, mFadeDuration, mViewAnimEndListener);
        } else {
            controller.destroy();
        }
        mBrowseViewControllersByNode.remove(item);
    }

    /**
     * Clears the given stack (or a portion of a stack) and destroys the old controllers (after
     * their view is hidden).
     */
    private void clearStack(List<MediaItemMetadata> stack) {
        for (MediaItemMetadata item : stack) {
            hideAndDestroyControllerForItem(item);
        }
        stack.clear();
    }

    /**
     * Updates the tabs displayed on the app bar, based on the top level items on the browse tree.
     * If there is at least one browsable item, we show the browse content of that node. If there
     * are only playable items, then we show those items. If there are not items at all, we show the
     * empty message. If we receive null, we show the error message.
     *
     * @param items top level items, null if the items are still being loaded, or empty list if
     *              items couldn't be loaded.
     */
    private void updateTabs(@Nullable List<MediaItemMetadata> items) {
        if (Objects.equals(mTopItems, items)) {
            // When coming back to the app, the live data sends an update even if the list hasn't
            // changed. Updating the tabs then recreates the browse view, which produces jank
            // (b/131830876), and also resets the navigation to the top of the first tab...
            return;
        }
        mTopItems = items;

        if (mTopItems == null || mTopItems.isEmpty()) {
            mAppBarController.setItems(null);
            mAppBarController.setActiveItem(null);
            if (items != null) {
                // Only do this when not loading the tabs or we loose the saved one.
                clearStack(mBrowseStack);
            }
            updateAppBar();
            return;
        }

        MediaItemMetadata oldTab = mViewModel.getSelectedTab();
        MediaItemMetadata newTab = items.contains(oldTab) ? oldTab : items.get(0);

        try {
            mAcceptTabSelection = false;
            mAppBarController.setItems(mTopItems.size() == 1 ? null : mTopItems);
            mAppBarController.setActiveItem(newTab);

            if (oldTab != newTab) {
                // Tabs belong to the browse stack.
                clearStack(mBrowseStack);
                mBrowseStack.push(newTab);
            }

            if (!mViewModel.isShowingSearchResults()) {
                // Needed when coming back to an app after a config change or from another app,
                // or when the tab actually changes.
                showCurrentNode(true);
            }
        }  finally {
            mAcceptTabSelection = true;
        }
        updateAppBar();
    }

    private void updateAppBarTitle() {
        boolean isStacked = !isAtTopStack();

        final CharSequence title;
        if (isStacked) {
            // If not at top level, show the current item as title
            title = getCurrentMediaItem().getTitle();
        } else if (mTopItems == null) {
            // If still loading the tabs, force to show an empty bar.
            title = "";
        } else if (mTopItems.size() == 1) {
            // If we finished loading tabs and there is only one, use that as title.
            title = mTopItems.get(0).getTitle();
        } else {
            // Otherwise (no tabs or more than 1 tabs), show the current media source title.
            MediaSource mediaSource = mMediaSourceVM.getPrimaryMediaSource().getValue();
            title = getAppBarDefaultTitle(mediaSource);
        }

        mAppBarController.setTitle(title);
    }

    /**
     * Update elements of the appbar that change depending on where we are in the browse.
     */
    private void updateAppBar() {
        boolean isSearching = mViewModel.isSearching();
        boolean isStacked = !isAtTopStack();
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "App bar is in stacked state: " + isStacked);
        }
        Toolbar.State unstackedState = isSearching ? Toolbar.State.SEARCH : Toolbar.State.HOME;
        updateAppBarTitle();
        mAppBarController.setState(isStacked ? Toolbar.State.SUBPAGE : unstackedState);
        mAppBarController.showSearchIfSupported(!isSearching || isStacked);
    }

    private void onRootMediaItemsUpdate(FutureData<List<MediaItemMetadata>> data) {
        if (data.isLoading()) {
            if (Log.isLoggable(TAG, Log.INFO)) {
                Log.i(TAG, "Loading browse tree...");
            }
            mBrowseTreeHasChildren = false;
            updateTabs(null);
            return;
        }

        List<MediaItemMetadata> items =
                MediaBrowserViewModelImpl.filterItems(/*forRoot*/ true, data.getData());

        boolean browseTreeHasChildren = items != null && !items.isEmpty();
        if (Log.isLoggable(TAG, Log.INFO)) {
            Log.i(TAG, "Browse tree loaded, status (has children or not) changed: "
                    + mBrowseTreeHasChildren + " -> " + browseTreeHasChildren);
        }
        mBrowseTreeHasChildren = browseTreeHasChildren;
        mCallbacks.onRootLoaded();
        updateTabs(items != null ? items : new ArrayList<>());
    }

}
