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
package com.android.wallpaper.picker;

import static com.android.wallpaper.widget.BottomActionBar.BottomAction.APPLY;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.content.res.Resources.NotFoundException;
import android.content.res.TypedArray;
import android.os.Bundle;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.CallSuper;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.core.widget.ContentLoadingProgressBar;
import androidx.fragment.app.FragmentActivity;

import com.android.wallpaper.R;
import com.android.wallpaper.model.LiveWallpaperInfo;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.Injector;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.module.WallpaperPersister.Destination;
import com.android.wallpaper.module.WallpaperPreferences;
import com.android.wallpaper.module.WallpaperSetter;
import com.android.wallpaper.widget.BottomActionBar;

import java.util.Date;
import java.util.List;

/**
 * Base Fragment to display the UI for previewing an individual wallpaper
 */
public abstract class PreviewFragment extends AppbarFragment implements
        SetWallpaperDialogFragment.Listener, SetWallpaperErrorDialogFragment.Listener,
        LoadWallpaperErrorDialogFragment.Listener {

    /**
     * User can view wallpaper and attributions in full screen, but "Set wallpaper" button is
     * hidden.
     */
    static final int MODE_VIEW_ONLY = 0;

    /**
     * User can view wallpaper and attributions in full screen and click "Set wallpaper" to set the
     * wallpaper with pan and crop position to the device.
     */
    static final int MODE_CROP_AND_SET_WALLPAPER = 1;

    /**
     * Possible preview modes for the fragment.
     */
    @IntDef({
            MODE_VIEW_ONLY,
            MODE_CROP_AND_SET_WALLPAPER})
    public @interface PreviewMode {
    }

    public static final String ARG_WALLPAPER = "wallpaper";
    public static final String ARG_PREVIEW_MODE = "preview_mode";
    public static final String ARG_VIEW_AS_HOME = "view_as_home";
    public static final String ARG_TESTING_MODE_ENABLED = "testing_mode_enabled";

    /**
     * Creates and returns new instance of {@link ImagePreviewFragment} with the provided wallpaper
     * set as an argument.
     */
    public static PreviewFragment newInstance(WallpaperInfo wallpaperInfo, @PreviewMode int mode,
            boolean viewAsHome, boolean testingModeEnabled) {
        Bundle args = new Bundle();
        args.putParcelable(ARG_WALLPAPER, wallpaperInfo);
        args.putInt(ARG_PREVIEW_MODE, mode);
        args.putBoolean(ARG_VIEW_AS_HOME, viewAsHome);
        args.putBoolean(ARG_TESTING_MODE_ENABLED, testingModeEnabled);

        PreviewFragment fragment = wallpaperInfo instanceof LiveWallpaperInfo
                ? new LivePreviewFragment() : new ImagePreviewFragment();
        fragment.setArguments(args);
        return fragment;
    }

    private static final String TAG_LOAD_WALLPAPER_ERROR_DIALOG_FRAGMENT =
            "load_wallpaper_error_dialog";
    private static final String TAG_SET_WALLPAPER_ERROR_DIALOG_FRAGMENT =
            "set_wallpaper_error_dialog";
    private static final int UNUSED_REQUEST_CODE = 1;
    private static final String TAG = "PreviewFragment";

    @PreviewMode
    protected int mPreviewMode;

    protected boolean mViewAsHome;

    /**
     * When true, enables a test mode of operation -- in which certain UI features are disabled to
     * allow for UI tests to run correctly. Works around issue in ProgressDialog currently where the
     * dialog constantly keeps the UI thread alive and blocks a test forever.
     */
    protected boolean mTestingModeEnabled;

    protected WallpaperInfo mWallpaper;
    protected WallpaperSetter mWallpaperSetter;
    protected UserEventLogger mUserEventLogger;
    protected BottomActionBar mBottomActionBar;
    protected ContentLoadingProgressBar mLoadingProgressBar;

    protected Intent mExploreIntent;
    protected CharSequence mActionLabel;

    /**
     * Staged error dialog fragments that were unable to be shown when the hosting activity didn't
     * allow committing fragment transactions.
     */
    private SetWallpaperErrorDialogFragment mStagedSetWallpaperErrorDialogFragment;
    private LoadWallpaperErrorDialogFragment mStagedLoadWallpaperErrorDialogFragment;

    protected static int getAttrColor(Context context, int attr) {
        TypedArray ta = context.obtainStyledAttributes(new int[]{attr});
        int colorAccent = ta.getColor(0, 0);
        ta.recycle();
        return colorAccent;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Context appContext = getContext().getApplicationContext();
        Injector injector = InjectorProvider.getInjector();

        mUserEventLogger = injector.getUserEventLogger(appContext);
        mWallpaper = getArguments().getParcelable(ARG_WALLPAPER);

        //noinspection ResourceType
        mPreviewMode = getArguments().getInt(ARG_PREVIEW_MODE);
        mViewAsHome = getArguments().getBoolean(ARG_VIEW_AS_HOME);
        mTestingModeEnabled = getArguments().getBoolean(ARG_TESTING_MODE_ENABLED);
        mWallpaperSetter = new WallpaperSetter(injector.getWallpaperPersister(appContext),
                injector.getPreferences(appContext), mUserEventLogger, mTestingModeEnabled);

        setHasOptionsMenu(true);

        Activity activity = getActivity();
        List<String> attributions = getAttributions(activity);
        if (attributions.size() > 0 && attributions.get(0) != null) {
            activity.setTitle(attributions.get(0));
        }
    }

    @Override
    @CallSuper
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(getLayoutResId(), container, false);
        setUpToolbar(view);

        mLoadingProgressBar = view.findViewById(getLoadingIndicatorResId());
        mLoadingProgressBar.show();
        return view;
    }

    @Override
    protected void onBottomActionBarReady(BottomActionBar bottomActionBar) {
        mBottomActionBar = bottomActionBar;
        // TODO: Extract the common code here.
    }

    protected List<String> getAttributions(Context context) {
        return mWallpaper.getAttributions(context);
    }

    @LayoutRes
    protected abstract int getLayoutResId();

    @IdRes
    protected abstract int getLoadingIndicatorResId();

    protected int getDeviceDefaultTheme() {
        return android.R.style.Theme_DeviceDefault;
    }

    @Override
    public void onResume() {
        super.onResume();

        WallpaperPreferences preferences =
                InjectorProvider.getInjector().getPreferences(getActivity());
        preferences.setLastAppActiveTimestamp(new Date().getTime());

        // Show the staged 'load wallpaper' or 'set wallpaper' error dialog fragments if there is
        // one that was unable to be shown earlier when this fragment's hosting activity didn't
        // allow committing fragment transactions.
        if (mStagedLoadWallpaperErrorDialogFragment != null) {
            mStagedLoadWallpaperErrorDialogFragment.show(
                    requireFragmentManager(), TAG_LOAD_WALLPAPER_ERROR_DIALOG_FRAGMENT);
            mStagedLoadWallpaperErrorDialogFragment = null;
        }
        if (mStagedSetWallpaperErrorDialogFragment != null) {
            mStagedSetWallpaperErrorDialogFragment.show(
                    requireFragmentManager(), TAG_SET_WALLPAPER_ERROR_DIALOG_FRAGMENT);
            mStagedSetWallpaperErrorDialogFragment = null;
        }
    }

    protected void setUpExploreIntentAndLabel(@Nullable Runnable callback) {
        Context context = getContext();
        if (context == null) {
            return;
        }

        WallpaperInfoHelper.loadExploreIntent(context, mWallpaper,
                (actionLabel, exploreIntent) -> {
                    mActionLabel = actionLabel;
                    mExploreIntent = exploreIntent;
                    if (callback != null) {
                        callback.run();
                    }
                }
        );
    }

    /**
     * Configure loading indicator with a MaterialProgressDrawable.
     */
    protected void setUpLoadingIndicator() {
        mLoadingProgressBar.setProgressTintList(ColorStateList.valueOf(getAttrColor(
                new ContextThemeWrapper(requireContext(), getDeviceDefaultTheme()),
                android.R.attr.colorAccent)));
        mLoadingProgressBar.show();
    }

    protected abstract boolean isLoaded();

    @Override
    public void onSet(int destination) {
        setCurrentWallpaper(destination);
    }

    @Override
    public void onDialogDismissed(boolean withItemSelected) {
        mBottomActionBar.deselectAction(APPLY);
    }

    @Override
    public void onClickTryAgain(@Destination int wallpaperDestination) {
        setCurrentWallpaper(wallpaperDestination);
    }

    @Override
    public void onClickOk() {
        FragmentActivity activity = getActivity();
        if (activity != null) {
            activity.finish();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mWallpaperSetter.cleanUp();
    }

    @Override
    public CharSequence getDefaultTitle() {
        return getContext().getString(R.string.preview);
    }

    protected void onSetWallpaperClicked(View button) {
        mWallpaperSetter.requestDestination(getActivity(), getFragmentManager(), this,
                mWallpaper instanceof LiveWallpaperInfo);
    }

    protected void onExploreClicked(View button) {
        if (getContext() == null) {
            return;
        }
        Context context = getContext();
        mUserEventLogger.logActionClicked(mWallpaper.getCollectionId(context),
                mWallpaper.getActionLabelRes(context));

        startActivity(mExploreIntent);
    }

    /**
     * Sets current wallpaper to the device based on current zoom and scroll state.
     *
     * @param destination The wallpaper destination i.e. home vs. lockscreen vs. both.
     */
    protected abstract void setCurrentWallpaper(@Destination int destination);

    protected void finishActivity(boolean success) {
        Activity activity = getActivity();
        if (activity == null) {
            return;
        }
        if (success) {
            try {
                Toast.makeText(activity,
                        R.string.wallpaper_set_successfully_message, Toast.LENGTH_SHORT).show();
            } catch (NotFoundException e) {
                Log.e(TAG, "Could not show toast " + e);
            }
            activity.setResult(Activity.RESULT_OK);
        }
        activity.overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
        activity.finish();
    }

    protected void showSetWallpaperErrorDialog(@Destination int wallpaperDestination) {
        SetWallpaperErrorDialogFragment newFragment = SetWallpaperErrorDialogFragment.newInstance(
                R.string.set_wallpaper_error_message, wallpaperDestination);
        newFragment.setTargetFragment(this, UNUSED_REQUEST_CODE);

        // Show 'set wallpaper' error dialog now if it's safe to commit fragment transactions,
        // otherwise stage it for later when the hosting activity is in a state to commit fragment
        // transactions.
        BasePreviewActivity activity = (BasePreviewActivity) requireActivity();
        if (activity.isSafeToCommitFragmentTransaction()) {
            newFragment.show(requireFragmentManager(), TAG_SET_WALLPAPER_ERROR_DIALOG_FRAGMENT);
        } else {
            mStagedSetWallpaperErrorDialogFragment = newFragment;
        }
    }

    /**
     * Shows 'load wallpaper' error dialog now or stage it to be shown when the hosting activity is
     * in a state that allows committing fragment transactions.
     */
    protected void showLoadWallpaperErrorDialog() {
        LoadWallpaperErrorDialogFragment dialogFragment =
                LoadWallpaperErrorDialogFragment.newInstance();
        dialogFragment.setTargetFragment(this, UNUSED_REQUEST_CODE);

        // Show 'load wallpaper' error dialog now or stage it to be shown when the hosting
        // activity is in a state that allows committing fragment transactions.
        BasePreviewActivity activity = (BasePreviewActivity) getActivity();
        if (activity != null && activity.isSafeToCommitFragmentTransaction()) {
            dialogFragment.show(requireFragmentManager(), TAG_LOAD_WALLPAPER_ERROR_DIALOG_FRAGMENT);
        } else {
            mStagedLoadWallpaperErrorDialogFragment = dialogFragment;
        }
    }

    /**
     * Returns whether layout direction is RTL (or false for LTR). Since native RTL layout support
     * was added in API 17, returns false for versions lower than 17.
     */
    protected boolean isRtl() {
        return getResources().getConfiguration().getLayoutDirection()
                    == View.LAYOUT_DIRECTION_RTL;
    }
}
