/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.picker.individual;

import static com.android.wallpaper.picker.WallpaperPickerDelegate.PREVIEW_WALLPAPER_REQUEST_CODE;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.APPLY;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.EDIT;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.INFORMATION;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.ROTATION;

import android.app.Activity;
import android.app.ProgressDialog;
import android.app.WallpaperManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources.NotFoundException;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.Handler;
import android.service.wallpaper.WallpaperService;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.widget.ContentLoadingProgressBar;
import androidx.fragment.app.DialogFragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import com.android.wallpaper.R;
import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.Asset.DrawableLoadedListener;
import com.android.wallpaper.model.Category;
import com.android.wallpaper.model.CategoryProvider;
import com.android.wallpaper.model.CategoryReceiver;
import com.android.wallpaper.model.LiveWallpaperInfo;
import com.android.wallpaper.model.WallpaperCategory;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.model.WallpaperReceiver;
import com.android.wallpaper.model.WallpaperRotationInitializer;
import com.android.wallpaper.model.WallpaperRotationInitializer.Listener;
import com.android.wallpaper.model.WallpaperRotationInitializer.NetworkPreference;
import com.android.wallpaper.module.FormFactorChecker;
import com.android.wallpaper.module.FormFactorChecker.FormFactor;
import com.android.wallpaper.module.Injector;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.PackageStatusNotifier;
import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.module.WallpaperChangedNotifier;
import com.android.wallpaper.module.WallpaperPersister;
import com.android.wallpaper.module.WallpaperPersister.Destination;
import com.android.wallpaper.module.WallpaperPreferences;
import com.android.wallpaper.module.WallpaperSetter;
import com.android.wallpaper.picker.BaseActivity;
import com.android.wallpaper.picker.BottomActionBarFragment;
import com.android.wallpaper.picker.CurrentWallpaperBottomSheetPresenter;
import com.android.wallpaper.picker.FragmentTransactionChecker;
import com.android.wallpaper.picker.MyPhotosStarter.MyPhotosStarterProvider;
import com.android.wallpaper.picker.PreviewActivity;
import com.android.wallpaper.picker.RotationStarter;
import com.android.wallpaper.picker.SetWallpaperDialogFragment;
import com.android.wallpaper.picker.SetWallpaperErrorDialogFragment;
import com.android.wallpaper.picker.StartRotationDialogFragment;
import com.android.wallpaper.picker.StartRotationErrorDialogFragment;
import com.android.wallpaper.picker.WallpaperInfoHelper;
import com.android.wallpaper.picker.WallpapersUiContainer;
import com.android.wallpaper.picker.individual.SetIndividualHolder.OnSetListener;
import com.android.wallpaper.util.DiskBasedLogger;
import com.android.wallpaper.util.SizeCalculator;
import com.android.wallpaper.widget.BottomActionBar;
import com.android.wallpaper.widget.WallpaperInfoView;
import com.android.wallpaper.widget.WallpaperPickerRecyclerViewAccessibilityDelegate;
import com.android.wallpaper.widget.WallpaperPickerRecyclerViewAccessibilityDelegate.BottomSheetHost;

import com.bumptech.glide.Glide;
import com.bumptech.glide.MemoryCategory;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Optional;
import java.util.Random;

/**
 * Displays the Main UI for picking an individual wallpaper image.
 */
public class IndividualPickerFragment extends BottomActionBarFragment
        implements RotationStarter, StartRotationErrorDialogFragment.Listener,
        CurrentWallpaperBottomSheetPresenter.RefreshListener,
        SetWallpaperErrorDialogFragment.Listener, SetWallpaperDialogFragment.Listener,
        StartRotationDialogFragment.Listener {

    /**
     * Position of a special tile that doesn't belong to an individual wallpaper of the category,
     * such as "my photos" or "daily rotation".
     */
    static final int SPECIAL_FIXED_TILE_ADAPTER_POSITION = 0;
    static final String ARG_CATEGORY_COLLECTION_ID = "category_collection_id";

    private static final String TAG = "IndividualPickerFrgmnt";
    private static final int UNUSED_REQUEST_CODE = 1;
    private static final String TAG_START_ROTATION_DIALOG = "start_rotation_dialog";
    private static final String TAG_START_ROTATION_ERROR_DIALOG = "start_rotation_error_dialog";
    private static final String PROGRESS_DIALOG_NO_TITLE = null;
    private static final boolean PROGRESS_DIALOG_INDETERMINATE = true;
    private static final String TAG_SET_WALLPAPER_ERROR_DIALOG_FRAGMENT =
            "individual_set_wallpaper_error_dialog";
    private static final String KEY_NIGHT_MODE = "IndividualPickerFragment.NIGHT_MODE";

    /**
     * An interface for updating the thumbnail with the specific wallpaper.
     */
    public interface ThumbnailUpdater {
        /**
         * Updates the thumbnail with the specific wallpaper.
         */
        void updateThumbnail(WallpaperInfo wallpaperInfo);

        /**
         * Restores to the thumbnails of the wallpapers which were applied.
         */
        void restoreThumbnails();
    }

    /**
     * An interface for receiving the destination of the new applied wallpaper.
     */
    public interface WallpaperDestinationCallback {
        /**
         * Called when the destination of the wallpaper is set.
         *
         * @param destination the destination which a wallpaper may be set.
         *                    See {@link Destination} for more details.
         */
        void onDestinationSet(@Destination int destination);
    }

    /**
     * The listener which will be notified when the wallpaper is selected.
     */
    public interface WallpaperSelectedListener {
        /**
         * Called when the wallpaper is selected.
         *
         * @param position the position of the selected wallpaper
         */
        void onWallpaperSelected(int position);
    }

    /**
     * Interface to be implemented by a Fragment hosting a {@link IndividualPickerFragment}
     */
    public interface IndividualPickerFragmentHost {
        /**
         * Sets the title in the toolbar.
         */
        void setToolbarTitle(CharSequence title);

        /**
         * Moves to the previous fragment.
         */
        void moveToPreviousFragment();
    }

    WallpaperPersister mWallpaperPersister;
    WallpaperPreferences mWallpaperPreferences;
    WallpaperChangedNotifier mWallpaperChangedNotifier;
    RecyclerView mImageGrid;
    IndividualAdapter mAdapter;
    WallpaperCategory mCategory;
    WallpaperRotationInitializer mWallpaperRotationInitializer;
    List<WallpaperInfo> mWallpapers;
    Point mTileSizePx;
    WallpapersUiContainer mWallpapersUiContainer;
    @FormFactor
    int mFormFactor;
    PackageStatusNotifier mPackageStatusNotifier;

    Handler mHandler;
    Random mRandom;
    boolean mIsWallpapersReceived;

    WallpaperChangedNotifier.Listener mWallpaperChangedListener =
            new WallpaperChangedNotifier.Listener() {
        @Override
        public void onWallpaperChanged() {
            if (mFormFactor != FormFactorChecker.FORM_FACTOR_DESKTOP) {
                return;
            }

            ViewHolder selectedViewHolder = mImageGrid.findViewHolderForAdapterPosition(
                    mAdapter.mSelectedAdapterPosition);

            // Null remote ID => My Photos wallpaper, so deselect whatever was previously selected.
            if (mWallpaperPreferences.getHomeWallpaperRemoteId() == null) {
                if (selectedViewHolder instanceof SelectableHolder) {
                    ((SelectableHolder) selectedViewHolder).setSelectionState(
                            SelectableHolder.SELECTION_STATE_DESELECTED);
                }
            } else {
                mAdapter.updateSelectedTile(mAdapter.mPendingSelectedAdapterPosition);
            }
        }
    };
    PackageStatusNotifier.Listener mAppStatusListener;
    BottomActionBar mBottomActionBar;
    WallpaperInfoView mWallpaperInfoView;
    @Nullable WallpaperInfo mSelectedWallpaperInfo;

    private UserEventLogger mUserEventLogger;
    private ProgressDialog mProgressDialog;
    private boolean mTestingMode;
    private CurrentWallpaperBottomSheetPresenter mCurrentWallpaperBottomSheetPresenter;
    private SetIndividualHolder mPendingSetIndividualHolder;
    private ContentLoadingProgressBar mLoading;

    /**
     * Staged error dialog fragments that were unable to be shown when the activity didn't allow
     * committing fragment transactions.
     */
    private SetWallpaperErrorDialogFragment mStagedSetWallpaperErrorDialogFragment;
    private StartRotationErrorDialogFragment mStagedStartRotationErrorDialogFragment;

    private Runnable mCurrentWallpaperBottomSheetExpandedRunnable;

    /**
     * Whether {@code mUpdateDailyWallpaperThumbRunnable} has been run at least once in this
     * invocation of the fragment.
     */
    private boolean mWasUpdateRunnableRun;

    /**
     * A Runnable which regularly updates the thumbnail for the "Daily wallpapers" tile in desktop
     * mode.
     */
    private Runnable mUpdateDailyWallpaperThumbRunnable = new Runnable() {
        @Override
        public void run() {
            ViewHolder viewHolder = mImageGrid.findViewHolderForAdapterPosition(
                    SPECIAL_FIXED_TILE_ADAPTER_POSITION);
            if (viewHolder instanceof DesktopRotationHolder) {
                updateDesktopDailyRotationThumbnail((DesktopRotationHolder) viewHolder);
            } else { // viewHolder is null
                // If the rotation tile is unavailable (because user has scrolled down, causing the
                // ViewHolder to be recycled), schedule the update for some time later. Once user scrolls up
                // again, the ViewHolder will be re-bound and its thumbnail will be updated.
                mHandler.postDelayed(mUpdateDailyWallpaperThumbRunnable,
                        DesktopRotationHolder.CROSSFADE_DURATION_MILLIS
                                + DesktopRotationHolder.CROSSFADE_DURATION_PAUSE_MILLIS);
            }
        }
    };

    private WallpaperSetter mWallpaperSetter;
    private WallpaperInfo mAppliedWallpaperInfo;
    private WallpaperManager mWallpaperManager;
    private int mWallpaperDestination;
    private WallpaperSelectedListener mWallpaperSelectedListener;

    public static IndividualPickerFragment newInstance(String collectionId) {
        Bundle args = new Bundle();
        args.putString(ARG_CATEGORY_COLLECTION_ID, collectionId);

        IndividualPickerFragment fragment = new IndividualPickerFragment();
        fragment.setArguments(args);
        return fragment;
    }

    /**
     * Highlights the applied wallpaper (if it exists) according to the destination a wallpaper
     * would be set.
     *
     * @param wallpaperDestination the destination a wallpaper would be set.
     *                             It will be either {@link Destination#DEST_HOME_SCREEN}
     *                             or {@link Destination#DEST_LOCK_SCREEN}.
     */
    public void highlightAppliedWallpaper(@Destination int wallpaperDestination) {
        mWallpaperDestination = wallpaperDestination;
        if (mWallpapers != null) {
            refreshAppliedWallpaper();
        }
    }

    private void updateDesktopDailyRotationThumbnail(DesktopRotationHolder holder) {
        int wallpapersIndex = mRandom.nextInt(mWallpapers.size());
        Asset newThumbnailAsset = mWallpapers.get(wallpapersIndex).getThumbAsset(
                getActivity());
        holder.updateThumbnail(newThumbnailAsset, new DrawableLoadedListener() {
            @Override
            public void onDrawableLoaded() {
                if (getActivity() == null) {
                    return;
                }

                // Schedule the next update of the thumbnail.
                int delayMillis = DesktopRotationHolder.CROSSFADE_DURATION_MILLIS
                        + DesktopRotationHolder.CROSSFADE_DURATION_PAUSE_MILLIS;
                mHandler.postDelayed(mUpdateDailyWallpaperThumbRunnable, delayMillis);
            }
        });
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Injector injector = InjectorProvider.getInjector();
        Context appContext = getContext().getApplicationContext();
        mWallpaperPreferences = injector.getPreferences(appContext);

        mWallpaperChangedNotifier = WallpaperChangedNotifier.getInstance();
        mWallpaperChangedNotifier.registerListener(mWallpaperChangedListener);

        mWallpaperManager = WallpaperManager.getInstance(appContext);

        mFormFactor = injector.getFormFactorChecker(appContext).getFormFactor();

        mPackageStatusNotifier = injector.getPackageStatusNotifier(appContext);

        mUserEventLogger = injector.getUserEventLogger(appContext);

        mWallpaperPersister = injector.getWallpaperPersister(appContext);
        mWallpaperSetter = new WallpaperSetter(
                mWallpaperPersister,
                injector.getPreferences(appContext),
                injector.getUserEventLogger(appContext),
                false);

        mWallpapers = new ArrayList<>();
        mRandom = new Random();
        mHandler = new Handler();

        // Clear Glide's cache if night-mode changed to ensure thumbnails are reloaded
        if (savedInstanceState != null && (savedInstanceState.getInt(KEY_NIGHT_MODE)
                != (getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK))) {
            Glide.get(getContext()).clearMemory();
        }

        CategoryProvider categoryProvider = injector.getCategoryProvider(appContext);
        categoryProvider.fetchCategories(new CategoryReceiver() {
            @Override
            public void onCategoryReceived(Category category) {
                // Do nothing.
            }

            @Override
            public void doneFetchingCategories() {
                Category category = categoryProvider.getCategory(
                        getArguments().getString(ARG_CATEGORY_COLLECTION_ID));
                if (category != null && !(category instanceof WallpaperCategory)) {
                    return;
                }
                mCategory = (WallpaperCategory) category;
                if (mCategory == null) {
                    DiskBasedLogger.e(TAG, "Failed to find the category.", getContext());

                    // The absence of this category in the CategoryProvider indicates a broken
                    // state, see b/38030129. Hence, finish the activity and return.
                    getIndividualPickerFragmentHost().moveToPreviousFragment();
                    Toast.makeText(getContext(), R.string.collection_not_exist_msg,
                            Toast.LENGTH_SHORT).show();
                    return;
                }
                onCategoryLoaded();
            }
        }, false);
    }


    protected void onCategoryLoaded() {
        if (getIndividualPickerFragmentHost() == null) {
            return;
        }
        getIndividualPickerFragmentHost().setToolbarTitle(mCategory.getTitle());
        mWallpaperRotationInitializer = mCategory.getWallpaperRotationInitializer();
        // Avoids the "rotation" action is not shown correctly
        // in a rare case : onCategoryLoaded() is called after onBottomActionBarReady().
        if (isRotationEnabled() && mBottomActionBar != null
                && !mBottomActionBar.areActionsShown(ROTATION)) {
            mBottomActionBar.showActions(ROTATION);
        }
        fetchWallpapers(false);

        if (mCategory.supportsThirdParty()) {
            mAppStatusListener = (packageName, status) -> {
                if (status != PackageStatusNotifier.PackageStatus.REMOVED ||
                        mCategory.containsThirdParty(packageName)) {
                    fetchWallpapers(true);
                }
            };
            mPackageStatusNotifier.addListener(mAppStatusListener,
                    WallpaperService.SERVICE_INTERFACE);
        }

        maybeSetUpImageGrid();
    }

    void fetchWallpapers(boolean forceReload) {
        mWallpapers.clear();
        mIsWallpapersReceived = false;
        updateLoading();
        mCategory.fetchWallpapers(getActivity().getApplicationContext(), new WallpaperReceiver() {
            @Override
            public void onWallpapersReceived(List<WallpaperInfo> wallpapers) {
                mIsWallpapersReceived = true;
                updateLoading();
                for (WallpaperInfo wallpaper : wallpapers) {
                    mWallpapers.add(wallpaper);
                }

                // Wallpapers may load after the adapter is initialized, in which case we have
                // to explicitly notify that the data set has changed.
                if (mAdapter != null) {
                    mAdapter.notifyDataSetChanged();
                }

                if (mWallpapersUiContainer != null) {
                    mWallpapersUiContainer.onWallpapersReady();
                } else {
                    if (wallpapers.isEmpty()) {
                        // If there are no more wallpapers and we're on phone, just finish the
                        // Activity.
                        Activity activity = getActivity();
                        if (activity != null
                                && mFormFactor == FormFactorChecker.FORM_FACTOR_MOBILE) {
                            activity.finish();
                        }
                    }
                }
            }
        }, forceReload);
    }

    void updateLoading() {
        if (mLoading == null) {
            return;
        }

        if (mIsWallpapersReceived) {
            mLoading.hide();
        } else {
            mLoading.show();
        }
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(KEY_NIGHT_MODE,
                getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_individual_picker, container, false);

        mTileSizePx = SizeCalculator.getIndividualTileSize(getActivity());

        mImageGrid = (RecyclerView) view.findViewById(R.id.wallpaper_grid);
        if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
            int gridPaddingPx = getResources().getDimensionPixelSize(R.dimen.grid_padding_desktop);
            updateImageGridPadding(false /* addExtraBottomSpace */);
            mImageGrid.setScrollBarSize(gridPaddingPx);
        }
        mImageGrid.addItemDecoration(new GridPaddingDecoration(
                getResources().getDimensionPixelSize(R.dimen.grid_padding)));
        mImageGrid.setAccessibilityDelegateCompat(
                new WallpaperPickerRecyclerViewAccessibilityDelegate(
                        mImageGrid, (BottomSheetHost) getParentFragment(), getNumColumns()));
        mLoading = view.findViewById(R.id.loading_indicator);
        updateLoading();
        maybeSetUpImageGrid();
        setUpBottomSheet();
        return view;
    }

    @Override
    public void onClickTryAgain(@Destination int unused) {
        if (mPendingSetIndividualHolder != null) {
            mPendingSetIndividualHolder.setWallpaper();
        }
    }

    void updateImageGridPadding(boolean addExtraBottomSpace) {
        int gridPaddingPx = getResources().getDimensionPixelSize(R.dimen.grid_padding_desktop);
        int bottomSheetHeightPx = getResources().getDimensionPixelSize(
                R.dimen.current_wallpaper_bottom_sheet_layout_height);
        int paddingBottomPx = addExtraBottomSpace ? bottomSheetHeightPx : 0;
        // Only left and top may be set in order for the GridMarginDecoration to work properly.
        mImageGrid.setPadding(
                gridPaddingPx, gridPaddingPx, 0, paddingBottomPx);
    }

    private IndividualPickerFragmentHost getIndividualPickerFragmentHost() {
        return (IndividualPickerFragmentHost) getParentFragment();
    }

    private void maybeSetUpImageGrid() {
        // Skip if mImageGrid been initialized yet
        if (mImageGrid == null) {
            return;
        }
        // Skip if category hasn't loaded yet
        if (mCategory == null) {
            return;
        }
        // Skip if the adapter was already created
        if (mAdapter != null) {
            return;
        }
        setUpImageGrid();
    }

    /**
     * Create the adapter and assign it to mImageGrid.
     * Both mImageGrid and mCategory are guaranteed to not be null when this method is called.
     */
    void setUpImageGrid() {
        mAdapter = new IndividualAdapter(mWallpapers);
        mImageGrid.setAdapter(mAdapter);
        mImageGrid.setLayoutManager(new GridLayoutManager(getActivity(), getNumColumns()));
    }

    /**
     * Enables and populates the "Currently set" wallpaper BottomSheet.
     */
    void setUpBottomSheet() {
        mImageGrid.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, final int dy) {
                if (mCurrentWallpaperBottomSheetPresenter == null) {
                    return;
                }

                if (mCurrentWallpaperBottomSheetExpandedRunnable != null) {
                    mHandler.removeCallbacks(mCurrentWallpaperBottomSheetExpandedRunnable);
                }
                mCurrentWallpaperBottomSheetExpandedRunnable = new Runnable() {
                    @Override
                    public void run() {
                        if (dy > 0) {
                            mCurrentWallpaperBottomSheetPresenter.setCurrentWallpapersExpanded(false);
                        } else {
                            mCurrentWallpaperBottomSheetPresenter.setCurrentWallpapersExpanded(true);
                        }
                    }
                };
                mHandler.postDelayed(mCurrentWallpaperBottomSheetExpandedRunnable, 100);
            }
        });
    }

    @Override
    protected void onBottomActionBarReady(BottomActionBar bottomActionBar) {
        mBottomActionBar = bottomActionBar;
        if (isRotationEnabled()) {
            mBottomActionBar.showActionsOnly(ROTATION);
        }
        mBottomActionBar.setActionClickListener(ROTATION, unused -> {
            DialogFragment startRotationDialogFragment = new StartRotationDialogFragment();
            startRotationDialogFragment.setTargetFragment(
                    IndividualPickerFragment.this, UNUSED_REQUEST_CODE);
            startRotationDialogFragment.show(getFragmentManager(), TAG_START_ROTATION_DIALOG);
        });
        mBottomActionBar.setActionClickListener(APPLY, unused -> {
            mBottomActionBar.disableActions();
            mWallpaperSetter.requestDestination(getActivity(), getFragmentManager(), this,
                    mSelectedWallpaperInfo instanceof LiveWallpaperInfo);
        });

        mWallpaperInfoView = (WallpaperInfoView) LayoutInflater.from(getContext())
                .inflate(R.layout.wallpaper_info_view, /* root= */ null);
        mBottomActionBar.attachViewToBottomSheetAndBindAction(mWallpaperInfoView, INFORMATION);
        mBottomActionBar.setActionClickListener(EDIT, unused -> {
            mWallpaperPersister.setWallpaperInfoInPreview(mSelectedWallpaperInfo);
            mSelectedWallpaperInfo.showPreview(getActivity(),
                    new PreviewActivity.PreviewActivityIntentFactory(),
                    PREVIEW_WALLPAPER_REQUEST_CODE);
        });
        mBottomActionBar.show();
    }

    @Override
    public void onResume() {
        super.onResume();

        WallpaperPreferences preferences = InjectorProvider.getInjector()
                .getPreferences(getActivity());
        preferences.setLastAppActiveTimestamp(new Date().getTime());

        // Reset Glide memory settings to a "normal" level of usage since it may have been lowered in
        // PreviewFragment.
        Glide.get(getActivity()).setMemoryCategory(MemoryCategory.NORMAL);

        // Show the staged 'start rotation' error dialog fragment if there is one that was unable to be
        // shown earlier when this fragment's hosting activity didn't allow committing fragment
        // transactions.
        if (mStagedStartRotationErrorDialogFragment != null) {
            mStagedStartRotationErrorDialogFragment.show(
                    getFragmentManager(), TAG_START_ROTATION_ERROR_DIALOG);
            mStagedStartRotationErrorDialogFragment = null;
        }

        // Show the staged 'load wallpaper' or 'set wallpaper' error dialog fragments if there is one
        // that was unable to be shown earlier when this fragment's hosting activity didn't allow
        // committing fragment transactions.
        if (mStagedSetWallpaperErrorDialogFragment != null) {
            mStagedSetWallpaperErrorDialogFragment.show(
                    getFragmentManager(), TAG_SET_WALLPAPER_ERROR_DIALOG_FRAGMENT);
            mStagedSetWallpaperErrorDialogFragment = null;
        }

        if (shouldShowRotationTile() && mWasUpdateRunnableRun && !mWallpapers.isEmpty()) {
            // Must be resuming from a previously stopped state, so re-schedule the update of the
            // daily wallpapers tile thumbnail.
            mUpdateDailyWallpaperThumbRunnable.run();
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        mHandler.removeCallbacks(mUpdateDailyWallpaperThumbRunnable);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mProgressDialog != null) {
            mProgressDialog.dismiss();
        }
        mWallpaperChangedNotifier.unregisterListener(mWallpaperChangedListener);
        if (mAppStatusListener != null) {
            mPackageStatusNotifier.removeListener(mAppStatusListener);
        }
        mWallpaperSetter.cleanUp();
    }

    @Override
    public void onStartRotationDialogDismiss(@NonNull DialogInterface dialog) {
        // TODO(b/159310028): Refactor fragment layer to make it able to restore from config change.
        // This is to handle config change with StartRotationDialog popup,  the StartRotationDialog
        // still holds a reference to the destroyed Fragment and is calling
        // onStartRotationDialogDismissed on that destroyed Fragment.
        if (mBottomActionBar != null) {
            mBottomActionBar.deselectAction(ROTATION);
        }
    }

    @Override
    public void retryStartRotation(@NetworkPreference int networkPreference) {
        startRotation(networkPreference);
    }

    @Override
    public boolean onBackPressed() {
        if (mSelectedWallpaperInfo != null) {
            onWallpaperSelected(null, 0);
            return true;
        }
        return false;
    }

    public void setCurrentWallpaperBottomSheetPresenter(
            CurrentWallpaperBottomSheetPresenter presenter) {
        mCurrentWallpaperBottomSheetPresenter = presenter;
    }

    public void setWallpapersUiContainer(WallpapersUiContainer uiContainer) {
        mWallpapersUiContainer = uiContainer;
    }

    public void setOnWallpaperSelectedListener(
            WallpaperSelectedListener wallpaperSelectedListener) {
        mWallpaperSelectedListener = wallpaperSelectedListener;
    }

    /**
     * Resizes the layout's height.
     */
    public void resizeLayout(int height) {
        mImageGrid.getLayoutParams().height = height;
        mImageGrid.requestLayout();
    }

    /**
     * Scrolls to the specific item.
     *
     * @param position the position of the item
     */
    public void scrollToPosition(int position) {
        ((GridLayoutManager) mImageGrid.getLayoutManager())
                .scrollToPositionWithOffset(position, /* offset= */ 0);
    }

    /**
     * Enable a test mode of operation -- in which certain UI features are disabled to allow for
     * UI tests to run correctly. Works around issue in ProgressDialog currently where the dialog
     * constantly keeps the UI thread alive and blocks a test forever.
     *
     * @param testingMode
     */
    void setTestingMode(boolean testingMode) {
        mTestingMode = testingMode;
    }

    @Override
    public void startRotation(@NetworkPreference final int networkPreference) {
        if (!isRotationEnabled()) {
            Log.e(TAG, "Rotation is not enabled for this category " + mCategory.getTitle());
            return;
        }

        // ProgressDialog endlessly updates the UI thread, keeping it from going idle which therefore
        // causes Espresso to hang once the dialog is shown.
        if (mFormFactor == FormFactorChecker.FORM_FACTOR_MOBILE && !mTestingMode) {
            int themeResId;
            if (VERSION.SDK_INT < VERSION_CODES.LOLLIPOP) {
                themeResId = R.style.ProgressDialogThemePreL;
            } else {
                themeResId = R.style.LightDialogTheme;
            }
            mProgressDialog = new ProgressDialog(getActivity(), themeResId);

            mProgressDialog.setTitle(PROGRESS_DIALOG_NO_TITLE);
            mProgressDialog.setMessage(
                    getResources().getString(R.string.start_rotation_progress_message));
            mProgressDialog.setIndeterminate(PROGRESS_DIALOG_INDETERMINATE);
            mProgressDialog.show();
        }

        if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
            mAdapter.mPendingSelectedAdapterPosition = SPECIAL_FIXED_TILE_ADAPTER_POSITION;
        }

        final Context appContext = getActivity().getApplicationContext();

        mWallpaperRotationInitializer.setFirstWallpaperInRotation(
                appContext,
                networkPreference,
                new Listener() {
                    @Override
                    public void onFirstWallpaperInRotationSet() {
                        if (mProgressDialog != null) {
                            mProgressDialog.dismiss();
                        }

                        // The fragment may be detached from its containing activity if the user exits the
                        // app before the first wallpaper image in rotation finishes downloading.
                        Activity activity = getActivity();


                        if (mWallpaperRotationInitializer.startRotation(appContext)) {
                            if (activity != null
                                    && mFormFactor == FormFactorChecker.FORM_FACTOR_MOBILE) {
                                try {
                                    Toast.makeText(getActivity(),
                                            R.string.wallpaper_set_successfully_message,
                                            Toast.LENGTH_SHORT).show();
                                } catch (NotFoundException e) {
                                    Log.e(TAG, "Could not show toast " + e);
                                }

                                activity.setResult(Activity.RESULT_OK);
                                activity.finish();
                            } else if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
                                mAdapter.updateSelectedTile(SPECIAL_FIXED_TILE_ADAPTER_POSITION);
                            }
                        } else { // Failed to start rotation.
                            showStartRotationErrorDialog(networkPreference);

                            if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
                                DesktopRotationHolder rotationViewHolder =
                                        (DesktopRotationHolder)
                                                mImageGrid.findViewHolderForAdapterPosition(
                                                SPECIAL_FIXED_TILE_ADAPTER_POSITION);
                                rotationViewHolder.setSelectionState(
                                        SelectableHolder.SELECTION_STATE_DESELECTED);
                            }
                        }
                    }

                    @Override
                    public void onError() {
                        if (mProgressDialog != null) {
                            mProgressDialog.dismiss();
                        }

                        showStartRotationErrorDialog(networkPreference);

                        if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
                            DesktopRotationHolder rotationViewHolder =
                                    (DesktopRotationHolder) mImageGrid.findViewHolderForAdapterPosition(
                                            SPECIAL_FIXED_TILE_ADAPTER_POSITION);
                            rotationViewHolder.setSelectionState(SelectableHolder.SELECTION_STATE_DESELECTED);
                        }
                    }
                });
    }

    private void showStartRotationErrorDialog(@NetworkPreference int networkPreference) {
        FragmentTransactionChecker activity = (FragmentTransactionChecker) getActivity();
        if (activity != null) {
            StartRotationErrorDialogFragment startRotationErrorDialogFragment =
                    StartRotationErrorDialogFragment.newInstance(networkPreference);
            startRotationErrorDialogFragment.setTargetFragment(
                    IndividualPickerFragment.this, UNUSED_REQUEST_CODE);

            if (activity.isSafeToCommitFragmentTransaction()) {
                startRotationErrorDialogFragment.show(
                        getFragmentManager(), TAG_START_ROTATION_ERROR_DIALOG);
            } else {
                mStagedStartRotationErrorDialogFragment = startRotationErrorDialogFragment;
            }
        }
    }

    int getNumColumns() {
        Activity activity = getActivity();
        return activity == null ? 1 : SizeCalculator.getNumIndividualColumns(activity);
    }

    /**
     * Returns whether rotation is enabled for this category.
     */
    boolean isRotationEnabled() {
        return mWallpaperRotationInitializer != null;
    }

    @Override
    public void onCurrentWallpaperRefreshed() {
        mCurrentWallpaperBottomSheetPresenter.setCurrentWallpapersExpanded(true);
    }

    @Override
    public void onSet(int destination) {
        if (mSelectedWallpaperInfo == null) {
            Log.e(TAG, "Unable to set wallpaper since the selected wallpaper info is null");
            return;
        }

        mWallpaperPersister.setWallpaperInfoInPreview(mSelectedWallpaperInfo);
        if (mSelectedWallpaperInfo instanceof LiveWallpaperInfo) {
            mWallpaperSetter.setCurrentWallpaper(getActivity(), mSelectedWallpaperInfo, null,
                    destination, 0, null, mSetWallpaperCallback);
        } else {
            mWallpaperSetter.setCurrentWallpaper(
                    getActivity(), mSelectedWallpaperInfo, destination, mSetWallpaperCallback);
        }
        onWallpaperDestinationSet(destination);
    }

    private WallpaperPersister.SetWallpaperCallback mSetWallpaperCallback =
            new WallpaperPersister.SetWallpaperCallback() {
                @Override
                public void onSuccess(WallpaperInfo wallpaperInfo) {
                    mWallpaperPersister.onLiveWallpaperSet();
                    Toast.makeText(getActivity(), R.string.wallpaper_set_successfully_message,
                            Toast.LENGTH_SHORT).show();
                    getActivity().overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
                    getActivity().finish();
                }

                @Override
                public void onError(@Nullable Throwable throwable) {
                    Log.e(TAG, "Can't apply the wallpaper.");
                    mBottomActionBar.enableActions();
                }
            };

    @Override
    public void onDialogDismissed(boolean withItemSelected) {
        if (!withItemSelected) {
            mBottomActionBar.enableActions();
        }
    }

    /**
     * Shows a "set wallpaper" error dialog with a failure message and button to try again.
     */
    private void showSetWallpaperErrorDialog() {
        SetWallpaperErrorDialogFragment dialogFragment = SetWallpaperErrorDialogFragment.newInstance(
                R.string.set_wallpaper_error_message, WallpaperPersister.DEST_BOTH);
        dialogFragment.setTargetFragment(this, UNUSED_REQUEST_CODE);

        if (((BaseActivity) getActivity()).isSafeToCommitFragmentTransaction()) {
            dialogFragment.show(getFragmentManager(), TAG_SET_WALLPAPER_ERROR_DIALOG_FRAGMENT);
        } else {
            mStagedSetWallpaperErrorDialogFragment = dialogFragment;
        }
    }

    void updateBottomActions(boolean hasWallpaperSelected) {
        if (hasWallpaperSelected) {
            mBottomActionBar.showActionsOnly(INFORMATION, EDIT, APPLY);
        } else {
            mBottomActionBar.showActionsOnly(ROTATION);
        }
    }

    private void updateThumbnail(WallpaperInfo selectedWallpaperInfo) {
        ThumbnailUpdater thumbnailUpdater = (ThumbnailUpdater) getParentFragment();
        if (thumbnailUpdater == null) {
            return;
        }

        if (selectedWallpaperInfo != null) {
            thumbnailUpdater.updateThumbnail(selectedWallpaperInfo);
        } else {
            thumbnailUpdater.restoreThumbnails();
        }
    }

    private void onWallpaperDestinationSet(int destination) {
        WallpaperDestinationCallback wallpaperDestinationCallback =
                (WallpaperDestinationCallback) getParentFragment();
        if (wallpaperDestinationCallback == null) {
            return;
        }

        wallpaperDestinationCallback.onDestinationSet(destination);
    }

    void onWallpaperSelected(@Nullable WallpaperInfo newSelectedWallpaperInfo,
                                     int position) {
        if (mSelectedWallpaperInfo == newSelectedWallpaperInfo) {
            return;
        }
        // Update current wallpaper.
        updateActivatedStatus(mSelectedWallpaperInfo == null
                ? mAppliedWallpaperInfo : mSelectedWallpaperInfo, false);
        // Update new selected wallpaper.
        updateActivatedStatus(newSelectedWallpaperInfo == null
                ? mAppliedWallpaperInfo : newSelectedWallpaperInfo, true);

        mSelectedWallpaperInfo = newSelectedWallpaperInfo;
        updateBottomActions(mSelectedWallpaperInfo != null);
        updateThumbnail(mSelectedWallpaperInfo);
        // Populate wallpaper info into view.
        if (mSelectedWallpaperInfo != null && mWallpaperInfoView != null) {
            WallpaperInfoHelper.loadExploreIntent(
                    getContext(),
                    mSelectedWallpaperInfo,
                    (actionLabel, exploreIntent) ->
                            mWallpaperInfoView.populateWallpaperInfo(
                                    mSelectedWallpaperInfo, actionLabel, exploreIntent,
                                    v -> onExploreClicked(exploreIntent)));
        }

        if (mWallpaperSelectedListener != null) {
            mWallpaperSelectedListener.onWallpaperSelected(position);
        }
    }

    private void onExploreClicked(Intent exploreIntent) {
        if (getContext() == null) {
            return;
        }
        Context context = getContext();
        mUserEventLogger.logActionClicked(mSelectedWallpaperInfo.getCollectionId(context),
                mSelectedWallpaperInfo.getActionLabelRes(context));

        startActivity(exploreIntent);
    }

    private void updateActivatedStatus(WallpaperInfo wallpaperInfo, boolean isActivated) {
        if (wallpaperInfo == null) {
            return;
        }
        int index = mWallpapers.indexOf(wallpaperInfo);
        index = (shouldShowRotationTile() || mCategory.supportsCustomPhotos())
                ? index + 1 : index;
        ViewHolder holder = mImageGrid.findViewHolderForAdapterPosition(index);
        if (holder != null) {
            holder.itemView.setActivated(isActivated);
        } else {
            // Item is not visible, make sure the item is re-bound when it becomes visible.
            mAdapter.notifyItemChanged(index);
        }
    }

    private void updateAppliedStatus(WallpaperInfo wallpaperInfo, boolean isApplied) {
        if (wallpaperInfo == null) {
            return;
        }
        int index = mWallpapers.indexOf(wallpaperInfo);
        index = (shouldShowRotationTile() || mCategory.supportsCustomPhotos())
                ? index + 1 : index;
        ViewHolder holder = mImageGrid.findViewHolderForAdapterPosition(index);
        if (holder != null) {
            holder.itemView.findViewById(R.id.check_circle)
                    .setVisibility(isApplied ? View.VISIBLE : View.GONE);
        } else {
            // Item is not visible, make sure the item is re-bound when it becomes visible.
            mAdapter.notifyItemChanged(index);
        }
    }

    private void refreshAppliedWallpaper() {
        // Clear the check mark and blue border(if it shows) of the old applied wallpaper.
        showCheckMarkAndBorderForAppliedWallpaper(false);

        // Update to the new applied wallpaper.
        String appliedWallpaperId = getAppliedWallpaperId();
        Optional<WallpaperInfo> wallpaperInfoOptional = mWallpapers
                .stream()
                .filter(wallpaper -> wallpaper.getWallpaperId() != null)
                .filter(wallpaper -> wallpaper.getWallpaperId().equals(appliedWallpaperId))
                .findFirst();
        mAppliedWallpaperInfo = wallpaperInfoOptional.orElse(null);

        // Set the check mark and blue border(if user doesn't select) of the new applied wallpaper.
        showCheckMarkAndBorderForAppliedWallpaper(true);
    }

    private String getAppliedWallpaperId() {
        WallpaperPreferences prefs =
                InjectorProvider.getInjector().getPreferences(getContext());
        android.app.WallpaperInfo wallpaperInfo = mWallpaperManager.getWallpaperInfo();
        boolean isDestinationBoth =
                mWallpaperManager.getWallpaperId(WallpaperManager.FLAG_LOCK) < 0;

        if (isDestinationBoth || mWallpaperDestination == WallpaperPersister.DEST_HOME_SCREEN) {
            return wallpaperInfo != null
                    ? wallpaperInfo.getServiceName() : prefs.getHomeWallpaperRemoteId();
        } else {
            return prefs.getLockWallpaperRemoteId();
        }
    }

    private void showCheckMarkAndBorderForAppliedWallpaper(boolean show) {
        updateAppliedStatus(mAppliedWallpaperInfo, show);
        if (mSelectedWallpaperInfo == null) {
            updateActivatedStatus(mAppliedWallpaperInfo, show);
        }
    }

    private boolean shouldShowRotationTile() {
        return mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP && isRotationEnabled();
    }

    /**
     * RecyclerView Adapter subclass for the wallpaper tiles in the RecyclerView.
     */
    class IndividualAdapter extends RecyclerView.Adapter<ViewHolder> {
        static final int ITEM_VIEW_TYPE_ROTATION = 1;
        static final int ITEM_VIEW_TYPE_INDIVIDUAL_WALLPAPER = 2;
        static final int ITEM_VIEW_TYPE_MY_PHOTOS = 3;

        private final List<WallpaperInfo> mWallpapers;

        private int mPendingSelectedAdapterPosition;
        private int mSelectedAdapterPosition;

        IndividualAdapter(List<WallpaperInfo> wallpapers) {
            mWallpapers = wallpapers;
            mPendingSelectedAdapterPosition = -1;
            mSelectedAdapterPosition = -1;
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            switch (viewType) {
                case ITEM_VIEW_TYPE_ROTATION:
                    return createRotationHolder(parent);
                case ITEM_VIEW_TYPE_INDIVIDUAL_WALLPAPER:
                    return createIndividualHolder(parent);
                case ITEM_VIEW_TYPE_MY_PHOTOS:
                    return createMyPhotosHolder(parent);
                default:
                    Log.e(TAG, "Unsupported viewType " + viewType + " in IndividualAdapter");
                    return null;
            }
        }

        @Override
        public int getItemViewType(int position) {
            if (shouldShowRotationTile() && position == SPECIAL_FIXED_TILE_ADAPTER_POSITION) {
                return ITEM_VIEW_TYPE_ROTATION;
            }

            // A category cannot have both a "start rotation" tile and a "my photos" tile.
            if (mCategory.supportsCustomPhotos()
                    && !isRotationEnabled()
                    && position == SPECIAL_FIXED_TILE_ADAPTER_POSITION) {
                return ITEM_VIEW_TYPE_MY_PHOTOS;
            }

            return ITEM_VIEW_TYPE_INDIVIDUAL_WALLPAPER;
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            int viewType = getItemViewType(position);

            switch (viewType) {
                case ITEM_VIEW_TYPE_ROTATION:
                    onBindRotationHolder(holder, position);
                    break;
                case ITEM_VIEW_TYPE_INDIVIDUAL_WALLPAPER:
                    onBindIndividualHolder(holder, position);
                    break;
                case ITEM_VIEW_TYPE_MY_PHOTOS:
                    ((MyPhotosViewHolder) holder).bind();
                    break;
                default:
                    Log.e(TAG, "Unsupported viewType " + viewType + " in IndividualAdapter");
            }
        }

        @Override
        public int getItemCount() {
            return (shouldShowRotationTile() || mCategory.supportsCustomPhotos())
                    ? mWallpapers.size() + 1
                    : mWallpapers.size();
        }

        private ViewHolder createRotationHolder(ViewGroup parent) {
            LayoutInflater layoutInflater = LayoutInflater.from(getActivity());
            View view = layoutInflater.inflate(R.layout.grid_item_rotation_desktop, parent, false);
            SelectionAnimator selectionAnimator =
                    new CheckmarkSelectionAnimator(getActivity(), view);
            return new DesktopRotationHolder(getActivity(), mTileSizePx.y, view, selectionAnimator,
                    IndividualPickerFragment.this);
        }

        private ViewHolder createIndividualHolder(ViewGroup parent) {
            LayoutInflater layoutInflater = LayoutInflater.from(getActivity());
            View view = layoutInflater.inflate(R.layout.grid_item_image, parent, false);

            if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
                SelectionAnimator selectionAnimator =
                        new CheckmarkSelectionAnimator(getActivity(), view);
                return new SetIndividualHolder(
                        getActivity(), mTileSizePx.y, view,
                        selectionAnimator,
                        new OnSetListener() {
                            @Override
                            public void onPendingWallpaperSet(int adapterPosition) {
                                // Deselect and hide loading indicator for any previously pending tile.
                                if (mPendingSelectedAdapterPosition != -1) {
                                    ViewHolder oldViewHolder = mImageGrid.findViewHolderForAdapterPosition(
                                            mPendingSelectedAdapterPosition);
                                    if (oldViewHolder instanceof SelectableHolder) {
                                        ((SelectableHolder) oldViewHolder).setSelectionState(
                                                SelectableHolder.SELECTION_STATE_DESELECTED);
                                    }
                                }

                                if (mSelectedAdapterPosition != -1) {
                                    ViewHolder oldViewHolder = mImageGrid.findViewHolderForAdapterPosition(
                                            mSelectedAdapterPosition);
                                    if (oldViewHolder instanceof SelectableHolder) {
                                        ((SelectableHolder) oldViewHolder).setSelectionState(
                                                SelectableHolder.SELECTION_STATE_DESELECTED);
                                    }
                                }

                                mPendingSelectedAdapterPosition = adapterPosition;
                            }

                            @Override
                            public void onWallpaperSet(int adapterPosition) {
                                // No-op -- UI handles a new wallpaper being set by reacting to the
                                // WallpaperChangedNotifier.
                            }

                            @Override
                            public void onWallpaperSetFailed(SetIndividualHolder holder) {
                                showSetWallpaperErrorDialog();
                                mPendingSetIndividualHolder = holder;
                            }
                        });
            } else { // MOBILE
                return new PreviewIndividualHolder(getActivity(), mTileSizePx.y, view);
            }
        }

        private ViewHolder createMyPhotosHolder(ViewGroup parent) {
            LayoutInflater layoutInflater = LayoutInflater.from(getActivity());
            View view = layoutInflater.inflate(R.layout.grid_item_my_photos, parent, false);

            return new MyPhotosViewHolder(getActivity(),
                    ((MyPhotosStarterProvider) getActivity()).getMyPhotosStarter(),
                    mTileSizePx.y, view);
        }

        /**
         * Marks the tile at the given position as selected with a visual indication. Also updates the
         * "currently selected" BottomSheet to reflect the newly selected tile.
         */
        private void updateSelectedTile(int newlySelectedPosition) {
            // Prevent multiple spinners from appearing with a user tapping several tiles in rapid
            // succession.
            if (mPendingSelectedAdapterPosition == mSelectedAdapterPosition) {
                return;
            }

            if (mCurrentWallpaperBottomSheetPresenter != null) {
                mCurrentWallpaperBottomSheetPresenter.refreshCurrentWallpapers(
                        IndividualPickerFragment.this);

                if (mCurrentWallpaperBottomSheetExpandedRunnable != null) {
                    mHandler.removeCallbacks(mCurrentWallpaperBottomSheetExpandedRunnable);
                }
                mCurrentWallpaperBottomSheetExpandedRunnable = new Runnable() {
                    @Override
                    public void run() {
                        mCurrentWallpaperBottomSheetPresenter.setCurrentWallpapersExpanded(true);
                    }
                };
                mHandler.postDelayed(mCurrentWallpaperBottomSheetExpandedRunnable, 100);
            }

            // User may have switched to another category, thus detaching this fragment, so check here.
            // NOTE: We do this check after updating the current wallpaper BottomSheet so that the update
            // still occurs in the UI after the user selects that other category.
            if (getActivity() == null) {
                return;
            }

            // Update the newly selected wallpaper ViewHolder and the old one so that if
            // selection UI state applies (desktop UI), it is updated.
            if (mSelectedAdapterPosition >= 0) {
                ViewHolder oldViewHolder = mImageGrid.findViewHolderForAdapterPosition(
                        mSelectedAdapterPosition);
                if (oldViewHolder instanceof SelectableHolder) {
                    ((SelectableHolder) oldViewHolder).setSelectionState(
                            SelectableHolder.SELECTION_STATE_DESELECTED);
                }
            }

            // Animate selection of newly selected tile.
            ViewHolder newViewHolder = mImageGrid
                    .findViewHolderForAdapterPosition(newlySelectedPosition);
            if (newViewHolder instanceof SelectableHolder) {
                ((SelectableHolder) newViewHolder).setSelectionState(
                        SelectableHolder.SELECTION_STATE_SELECTED);
            }

            mSelectedAdapterPosition = newlySelectedPosition;

            // If the tile was in the last row of the grid, add space below it so the user can scroll down
            // and up to see the BottomSheet without it fully overlapping the newly selected tile.
            int spanCount = ((GridLayoutManager) mImageGrid.getLayoutManager()).getSpanCount();
            int numRows = (int) Math.ceil((float) getItemCount() / spanCount);
            int rowOfNewlySelectedTile = newlySelectedPosition / spanCount;
            boolean isInLastRow = rowOfNewlySelectedTile == numRows - 1;

            updateImageGridPadding(isInLastRow /* addExtraBottomSpace */);
        }

        void onBindRotationHolder(ViewHolder holder, int position) {
            if (mFormFactor == FormFactorChecker.FORM_FACTOR_DESKTOP) {
                String collectionId = mCategory.getCollectionId();
                ((DesktopRotationHolder) holder).bind(collectionId);

                if (mWallpaperPreferences.getWallpaperPresentationMode()
                        == WallpaperPreferences.PRESENTATION_MODE_ROTATING
                        && collectionId.equals(mWallpaperPreferences.getHomeWallpaperCollectionId())) {
                    mSelectedAdapterPosition = position;
                }

                if (!mWasUpdateRunnableRun && !mWallpapers.isEmpty()) {
                    updateDesktopDailyRotationThumbnail((DesktopRotationHolder) holder);
                    mWasUpdateRunnableRun = true;
                }
            }
        }

        void onBindIndividualHolder(ViewHolder holder, int position) {
            int wallpaperIndex = (shouldShowRotationTile() || mCategory.supportsCustomPhotos())
                    ? position - 1 : position;
            WallpaperInfo wallpaper = mWallpapers.get(wallpaperIndex);
            ((IndividualHolder) holder).bindWallpaper(wallpaper);
            String appliedWallpaperId = getAppliedWallpaperId();
            boolean isWallpaperApplied = wallpaper.getWallpaperId().equals(appliedWallpaperId);
            boolean isWallpaperSelected = wallpaper.equals(mSelectedWallpaperInfo);
            boolean hasUserSelectedWallpaper = mSelectedWallpaperInfo != null;

            if (isWallpaperApplied) {
                mSelectedAdapterPosition = position;
                mAppliedWallpaperInfo = wallpaper;
            }

            holder.itemView.setActivated(
                    (isWallpaperApplied && !hasUserSelectedWallpaper) || isWallpaperSelected);
            holder.itemView.findViewById(R.id.check_circle).setVisibility(
                    isWallpaperApplied ? View.VISIBLE : View.GONE);
        }
    }

    private class GridPaddingDecoration extends RecyclerView.ItemDecoration {

        private int mPadding;

        GridPaddingDecoration(int padding) {
            mPadding = padding;
        }

        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent,
                                   RecyclerView.State state) {
            int position = parent.getChildAdapterPosition(view);
            if (position >= 0) {
                outRect.left = mPadding;
                outRect.right = mPadding;
            }
        }
    }
}
