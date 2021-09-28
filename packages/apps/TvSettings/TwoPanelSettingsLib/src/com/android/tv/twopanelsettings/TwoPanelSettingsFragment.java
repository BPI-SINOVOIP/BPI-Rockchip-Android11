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

package com.android.tv.twopanelsettings;

import android.animation.ObjectAnimator;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.content.ContentProviderClient;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.transition.Fade;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.HorizontalScrollView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.leanback.preference.LeanbackListPreferenceDialogFragment;
import androidx.leanback.preference.LeanbackPreferenceFragment;
import androidx.leanback.widget.OnChildViewHolderSelectedListener;
import androidx.leanback.widget.VerticalGridView;
import androidx.preference.ListPreference;
import androidx.preference.MultiSelectListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import androidx.preference.PreferenceGroupAdapter;
import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.RecyclerView;

import com.android.tv.twopanelsettings.slices.HasSliceUri;
import com.android.tv.twopanelsettings.slices.InfoFragment;
import com.android.tv.twopanelsettings.slices.SlicePreference;
import com.android.tv.twopanelsettings.slices.SlicesConstants;

import java.util.Set;

/**
 * This fragment provides containers for displaying two {@link LeanbackPreferenceFragment}.
 * The preference fragment on the left works as a main panel on which the user can operate.
 * The preference fragment on the right works as a preview panel for displaying the preview
 * information.
 */
public abstract class TwoPanelSettingsFragment extends Fragment implements
        PreferenceFragment.OnPreferenceStartFragmentCallback,
        PreferenceFragment.OnPreferenceStartScreenCallback,
        PreferenceFragment.OnPreferenceDisplayDialogCallback {
    private static final String TAG = "TwoPanelSettingsFragment";
    private static final boolean DEBUG = false;
    private static final String PREVIEW_FRAGMENT_TAG =
            "com.android.tv.settings.TwoPanelSettingsFragment.PREVIEW_FRAGMENT";
    private static final String PREFERENCE_FRAGMENT_TAG =
            "com.android.tv.settings.TwoPanelSettingsFragment.PREFERENCE_FRAGMENT";
    private static final String EXTRA_PREF_PANEL_IDX =
            "com.android.tv.twopanelsettings.PREF_PANEL_IDX";
    private static final int[] frameResIds =
            {R.id.frame1, R.id.frame2, R.id.frame3, R.id.frame4, R.id.frame5, R.id.frame6,
                    R.id.frame7, R.id.frame8, R.id.frame9, R.id.frame10};
    private static final int[] frameResOverlayIds =
            {R.id.frame1_overlay, R.id.frame2_overlay, R.id.frame3_overlay, R.id.frame4_overlay,
            R.id.frame5_overlay, R.id.frame6_overlay, R.id.frame7_overlay, R.id.frame8_overlay,
            R.id.frame9_overlay, R.id.frame10_overlay};
    private static final long PANEL_ANIMATION_MS = 400;
    private static final long PANEL_ANIMATION_DELAY_MS = 200;

    private int mMaxScrollX;
    private final RootViewOnKeyListener mRootViewOnKeyListener = new RootViewOnKeyListener();
    private int mPrefPanelIdx;
    private HorizontalScrollView mScrollView;
    private Handler mHandler;
    private boolean mIsNavigatingBack;

    private OnChildViewHolderSelectedListener mOnChildViewHolderSelectedListener =
            new OnChildViewHolderSelectedListener() {
                @Override
                public void onChildViewHolderSelected(RecyclerView parent,
                        RecyclerView.ViewHolder child, int position, int subposition) {
                    if (child == null) {
                        return;
                    }
                    int adapterPosition = child.getAdapterPosition();
                    PreferenceGroupAdapter preferenceGroupAdapter =
                            (PreferenceGroupAdapter) parent.getAdapter();
                    Preference preference = preferenceGroupAdapter.getItem(adapterPosition);
                    onPreferenceFocused(preference);
                }

                @Override
                public void onChildViewHolderSelectedAndPositioned(RecyclerView parent,
                        RecyclerView.ViewHolder child, int position, int subposition) {
                }
            };

    private OnGlobalLayoutListener mOnGlobalLayoutListener = new OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            getView().getViewTreeObserver().removeOnGlobalLayoutListener(mOnGlobalLayoutListener);
            moveToPanel(mPrefPanelIdx, false);
        }
    };

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        final View v = inflater.inflate(R.layout.two_panel_settings_fragment, container, false);
        mScrollView = v.findViewById(R.id.scrollview);
        mHandler = new Handler();
        if (savedInstanceState != null) {
            mPrefPanelIdx = savedInstanceState.getInt(EXTRA_PREF_PANEL_IDX, mPrefPanelIdx);
            // Move to correct panel once global layout finishes.
            v.getViewTreeObserver().addOnGlobalLayoutListener(mOnGlobalLayoutListener);
        }
        mMaxScrollX = computeMaxRightScroll();
        return v;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        outState.putInt(EXTRA_PREF_PANEL_IDX, mPrefPanelIdx);
        super.onSaveInstanceState(outState);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        if (savedInstanceState == null) {
            onPreferenceStartInitialScreen();
        }
    }

    /** Extend this method to provide the initial screen **/
    public abstract void onPreferenceStartInitialScreen();

    private boolean shouldDisplay(String fragment) {
        try {
            return LeanbackPreferenceFragment.class.isAssignableFrom(Class.forName(fragment))
                    || InfoFragment.class.isAssignableFrom(Class.forName(fragment));
        } catch (ClassNotFoundException e) {
            throw new RuntimeException("Fragment class not found.", e);
        }
    }

    @Override
    public boolean onPreferenceStartFragment(PreferenceFragment caller, Preference pref) {
        if (DEBUG) {
            Log.d(TAG, "onPreferenceStartFragment " + pref.getTitle());
        }
        if (pref.getFragment() == null) {
            return false;
        }
        Fragment preview = getChildFragmentManager().findFragmentById(
                frameResIds[mPrefPanelIdx + 1]);
        if (preview != null && !(preview instanceof DummyFragment)) {
            if (!(preview instanceof InfoFragment)) {
                navigateToPreviewFragment();
            }
        } else {
            // If there is no corresponding slice provider, thus the corresponding fragment is not
            // created, return false to check the intent of the SlicePreference.
            if (pref instanceof SlicePreference) {
                return false;
            }
            startImmersiveFragment(Fragment.instantiate(getActivity(), pref.getFragment(),
                    pref.getExtras()));
        }
        return true;
    }

    /** Navigate back to the previous fragment **/
    public void navigateBack() {
        back(false);
    }

    /** Navigate into current preview fragment */
    public void navigateToPreviewFragment() {
        Fragment previewFragment = getChildFragmentManager().findFragmentById(
                frameResIds[mPrefPanelIdx + 1]);
        if (previewFragment instanceof NavigationCallback) {
            ((NavigationCallback) previewFragment).onNavigateToPreview();
        }
        if (previewFragment == null || previewFragment instanceof DummyFragment) {
            return;
        }
        if (DEBUG) {
            Log.d(TAG, "navigateToPreviewFragment");
        }
        if (mPrefPanelIdx + 1 >= frameResIds.length) {
            Log.w(TAG, "Maximum level of depth reached.");
            return;
        }
        Fragment initialPreviewFragment = getInitialPreviewFragment(previewFragment);
        if (initialPreviewFragment == null) {
            initialPreviewFragment = new DummyFragment();
        }
        initialPreviewFragment.setExitTransition(null);

        mPrefPanelIdx++;

        Fragment fragment = getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
        addOrRemovePreferenceFocusedListener(fragment, true);

        final FragmentTransaction transaction = getChildFragmentManager().beginTransaction();
        transaction.replace(frameResIds[mPrefPanelIdx + 1], initialPreviewFragment,
                PREVIEW_FRAGMENT_TAG);
        transaction.commit();

        moveToPanel(mPrefPanelIdx, true);
        removeFragmentAndAddToBackStack(mPrefPanelIdx - 1);
    }

    private void addOrRemovePreferenceFocusedListener(Fragment fragment, boolean isAddingListener) {
        if (fragment == null || !(fragment instanceof LeanbackPreferenceFragment)) {
            return;
        }
        LeanbackPreferenceFragment leanbackPreferenceFragment =
                (LeanbackPreferenceFragment) fragment;
        VerticalGridView listView = (VerticalGridView) leanbackPreferenceFragment.getListView();
        if (listView != null) {
            if (isAddingListener) {
                listView.setOnChildViewHolderSelectedListener(mOnChildViewHolderSelectedListener);
            } else {
                listView.setOnChildViewHolderSelectedListener(null);
            }
        }
    }

    /**
     * Displays left panel preference fragment to the user.
     *
     * @param fragment Fragment instance to be added.
     */
    public void startPreferenceFragment(@NonNull Fragment fragment) {
        if (DEBUG) {
            Log.d(TAG, "startPreferenceFragment");
        }
        FragmentTransaction transaction = getChildFragmentManager().beginTransaction();
        transaction.add(frameResIds[mPrefPanelIdx], fragment, PREFERENCE_FRAGMENT_TAG);
        transaction.commitNow();

        Fragment initialPreviewFragment = getInitialPreviewFragment(fragment);
        if (initialPreviewFragment == null) {
            initialPreviewFragment = new DummyFragment();
        }
        initialPreviewFragment.setExitTransition(null);

        transaction = getChildFragmentManager().beginTransaction();
        transaction.add(frameResIds[mPrefPanelIdx + 1], initialPreviewFragment,
                initialPreviewFragment.getClass().toString());
        transaction.commit();
    }

    @Override
    public boolean onPreferenceDisplayDialog(@NonNull PreferenceFragment caller, Preference pref) {
        if (DEBUG) {
            Log.d(TAG, "PreferenceDisplayDialog");
        }
        if (caller == null) {
            throw new IllegalArgumentException("Cannot display dialog for preference " + pref
                    + ", Caller must not be null!");
        }
        Fragment preview = getChildFragmentManager().findFragmentById(
                frameResIds[mPrefPanelIdx + 1]);
        if (preview != null && !(preview instanceof DummyFragment)) {
            if (preview instanceof NavigationCallback) {
                ((NavigationCallback) preview).onNavigateToPreview();
            }
            mPrefPanelIdx++;
            moveToPanel(mPrefPanelIdx, true);
            removeFragmentAndAddToBackStack(mPrefPanelIdx - 1);
            return true;
        }
        return false;
    }

    private boolean equalArguments(Bundle a, Bundle b) {
        if (a == null && b == null) {
            return true;
        }
        if (a == null || b == null) {
            return false;
        }
        Set<String> aks = a.keySet();
        Set<String> bks = b.keySet();
        if (a.size() != b.size()) {
            return false;
        }
        if (!aks.containsAll(bks)) {
            return false;
        }
        for (String key : aks) {
            if (a.get(key) == null && b.get(key) == null) {
                continue;
            }
            if (a.get(key) == null || b.get(key) == null) {
                return false;
            }
            if (!a.get(key).equals(b.get(key))) {
                return false;
            }
        }
        return true;
    }

    /** Callback from SliceFragment **/
    public interface SliceFragmentCallback {
        /** Triggered when preference is focused **/
        void onPreferenceFocused(Preference preference);
    }

    private boolean onPreferenceFocused(Preference pref) {
        if (DEBUG) {
            Log.d(TAG, "onPreferenceFocused " + pref.getTitle());
        }
        final Fragment prefFragment =
                getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
        if (prefFragment instanceof SliceFragmentCallback) {
            ((SliceFragmentCallback) prefFragment).onPreferenceFocused(pref);
        }
        Fragment previewFragment = null;
        try {
            previewFragment = onCreatePreviewFragment(prefFragment, pref);
        } catch (Exception e) {
            Log.w(TAG, "Cannot instantiate the fragment from preference: " + pref, e);
        }
        if (previewFragment == null) {
            previewFragment = new DummyFragment();
        } else {
            previewFragment.setTargetFragment(prefFragment, 0);
        }

        final Fragment existingPreviewFragment =
                getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx + 1]);
        if (existingPreviewFragment != null
                && existingPreviewFragment.getClass().equals(previewFragment.getClass())
                && equalArguments(existingPreviewFragment.getArguments(),
                previewFragment.getArguments())) {
            if (isRTL() && mScrollView.getScrollX() == 0 && mPrefPanelIdx == 0) {
                // For RTL we need to reclaim focus to the correct scroll position if a pref
                // launches a new activity because the horizontal scroll goes back to 0.
                getView().getViewTreeObserver().addOnGlobalLayoutListener(mOnGlobalLayoutListener);
            }
            return true;
        }

        // If the existing preview fragment is recreated when the activity is recreated, the
        // animation would fall back to "slide left", in this case, we need to set the exit
        // transition.
        if (existingPreviewFragment != null) {
            existingPreviewFragment.setExitTransition(null);
        }
        previewFragment.setEnterTransition(new Fade());
        previewFragment.setExitTransition(null);

        final FragmentTransaction transaction = getChildFragmentManager().beginTransaction();
        transaction.setCustomAnimations(android.R.animator.fade_in, android.R.animator.fade_out);
        transaction.replace(frameResIds[mPrefPanelIdx + 1], previewFragment);
        transaction.commit();

        // Some fragments may steal focus on creation. Reclaim focus on main fragment.
        getView().getViewTreeObserver().addOnGlobalLayoutListener(mOnGlobalLayoutListener);
        return true;
    }

    private boolean isRTL() {
        return getResources().getConfiguration().getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
    }

    @Override
    public void onResume() {
        if (DEBUG) {
            Log.d(TAG, "onResume");
        }
        super.onResume();
        // Trap back button presses
        final TwoPanelSettingsRootView rootView = (TwoPanelSettingsRootView) getView();
        if (rootView != null) {
            rootView.setOnBackKeyListener(mRootViewOnKeyListener);
        }
    }

    @Override
    public void onPause() {
        if (DEBUG) {
            Log.d(TAG, "onPause");
        }
        super.onPause();
        final TwoPanelSettingsRootView rootView = (TwoPanelSettingsRootView) getView();
        if (rootView != null) {
            rootView.setOnBackKeyListener(null);
        }
    }

    /**
     * Displays a fragment to the user, temporarily replacing the contents of this fragment.
     *
     * @param fragment Fragment instance to be added.
     */
    public void startImmersiveFragment(@NonNull Fragment fragment) {
        if (DEBUG) {
            Log.d(TAG, "Starting immersive fragment.");
        }
        final FragmentTransaction transaction = getChildFragmentManager().beginTransaction();
        Fragment target = getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
        fragment.setTargetFragment(target, 0);
        transaction
                .add(R.id.two_panel_fragment_container, fragment)
                .remove(target)
                .addToBackStack(null)
                .commit();
    }

    public static class DummyFragment extends Fragment {
        @Override
        public @Nullable
        View onCreateView(LayoutInflater inflater, ViewGroup container,
                Bundle savedInstanceState) {
            return inflater.inflate(R.layout.dummy_fragment, container, false);
        }
    }

    /**
     * Implement this if fragment needs to handle DPAD_LEFT & DPAD_RIGHT itself in some cases
     **/
    public interface NavigationCallback {

        /**
         * Returns true if the fragment is in the state that can navigate back on receiving a
         * navigation DPAD key. When true, TwoPanelSettings will initiate a back operation on
         * receiving a left key. This method doesn't apply to back key: back key always initiates a
         * back operation.
         */
        boolean canNavigateBackOnDPAD();

        /**
         * Callback when navigating to preview screen
         */
        void onNavigateToPreview();

        /**
         * Callback when returning to previous screen
         */
        void onNavigateBack();
    }

    /**
     * Implement this if the component (typically a Fragment) is preview-able and would like to get
     * some lifecycle-like callback(s) when the component becomes the main panel.
     */
    public interface PreviewableComponentCallback {

        /**
         * Lifecycle-like callback when the component becomes main panel from the preview panel. For
         * Fragment, this will be invoked right after the preview fragment sliding into the main
         * panel.
         *
         * @param forward means whether the component arrives at main panel when users are
         *    navigating forwards (deeper into the TvSettings tree).
         */
        void onArriveAtMainPanel(boolean forward);
    }

    private class RootViewOnKeyListener implements View.OnKeyListener {

        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            Fragment prefFragment =
                    getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
            if (event.getAction() == KeyEvent.ACTION_DOWN && keyCode == KeyEvent.KEYCODE_BACK) {
                return back(true);
            }

            if (event.getAction() == KeyEvent.ACTION_DOWN
                    && ((!isRTL() && keyCode == KeyEvent.KEYCODE_DPAD_LEFT)
                    || (isRTL() && keyCode == KeyEvent.KEYCODE_DPAD_RIGHT))) {
                if (prefFragment instanceof NavigationCallback
                        && !((NavigationCallback) prefFragment).canNavigateBackOnDPAD()) {
                    return false;
                }
                return back(false);
            }

            if (event.getAction() == KeyEvent.ACTION_DOWN
                    && ((!isRTL() && keyCode == KeyEvent.KEYCODE_DPAD_RIGHT)
                    || (isRTL() && keyCode == KeyEvent.KEYCODE_DPAD_LEFT))) {
                if (shouldPerformClick()) {
                    v.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN,
                            KeyEvent.KEYCODE_DPAD_CENTER));
                    v.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP,
                            KeyEvent.KEYCODE_DPAD_CENTER));
                } else {
                    Fragment previewFragment = getChildFragmentManager()
                            .findFragmentById(frameResIds[mPrefPanelIdx + 1]);
                    if (!(previewFragment instanceof InfoFragment)) {
                        navigateToPreviewFragment();
                    }
                }
                return true;
            }
            return false;
        }
    }

    private boolean shouldPerformClick() {
        Fragment prefFragment =
                getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
        Preference preference = getChosenPreference(prefFragment);
        if (preference == null) {
            return false;
        }
        // This is for the case when a preference has preview but once user navigate to
        // see the preview, settings actually launch an intent to start external activity.
        if (preference.getIntent() != null  && !TextUtils.isEmpty(preference.getFragment())) {
            return true;
        }
        if (preference instanceof SlicePreference
                && ((SlicePreference) preference).getSliceAction() != null
                && ((SlicePreference) preference).getUri() != null) {
            return true;
        }

        return false;
    }

    private boolean back(boolean isKeyBackPressed) {
        if (mIsNavigatingBack) {
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    if (DEBUG) {
                        Log.d(TAG, "Navigating back is deferred.");
                    }
                    back(isKeyBackPressed);
                }
            }, PANEL_ANIMATION_DELAY_MS + PANEL_ANIMATION_DELAY_MS);
            return true;
        }
        if (DEBUG) {
            Log.d(TAG, "Going back one level.");
        }

        final Fragment immersiveFragment =
                getChildFragmentManager().findFragmentById(R.id.two_panel_fragment_container);
        if (immersiveFragment != null) {
            getChildFragmentManager().popBackStack();
            moveToPanel(mPrefPanelIdx, false);
            return true;
        }

        if (mPrefPanelIdx < 1) {
            // Disallow the user to use "dpad left" to finish activity in the first screen
            if (isKeyBackPressed) {
                getActivity().finish();
            }
            return true;
        }

        mIsNavigatingBack = true;
        Fragment preferenceFragment =
                getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
        addOrRemovePreferenceFocusedListener(preferenceFragment, false);
        getChildFragmentManager().popBackStack();

        mPrefPanelIdx--;

        mHandler.postDelayed(() -> {
            moveToPanel(mPrefPanelIdx, true);
        }, PANEL_ANIMATION_DELAY_MS);

        mHandler.postDelayed(() -> {
            removeFragment(mPrefPanelIdx + 2);
            mIsNavigatingBack = false;
            Fragment previewFragment =
                    getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx + 1]);
            if (previewFragment instanceof NavigationCallback) {
                ((NavigationCallback) previewFragment).onNavigateBack();
            }
        }, PANEL_ANIMATION_DELAY_MS + PANEL_ANIMATION_DELAY_MS);
        return true;
    }

    private void removeFragment(int index) {
        Fragment fragment = getChildFragmentManager().findFragmentById(frameResIds[index]);
        if (fragment != null) {
            getChildFragmentManager().beginTransaction().remove(fragment).commit();
        }
    }

    private void removeFragmentAndAddToBackStack(int index) {
        if (index < 0) {
            return;
        }
        Fragment removePanel = getChildFragmentManager().findFragmentById(frameResIds[index]);
        if (removePanel != null) {
            removePanel.setExitTransition(new Fade());
            getChildFragmentManager().beginTransaction().remove(removePanel)
                    .addToBackStack("remove " + removePanel.getClass().getName()).commit();
        }
    }

    /** For RTL layout, we need to know the right edge from where the panels start scrolling. */
    private int computeMaxRightScroll() {
        int scrollViewWidth = getResources().getDimensionPixelSize(R.dimen.tp_settings_panes_width);
        int panelWidth = getResources().getDimensionPixelSize(
                R.dimen.tp_settings_preference_pane_width);
        int panelPadding = getResources().getDimensionPixelSize(
                R.dimen.preference_pane_extra_padding_start) * 2;
        int result = frameResIds.length * panelWidth - scrollViewWidth + panelPadding;
        return result < 0 ? 0 : result;
    }

    /** Scrolls such that the panel with given index is the main panel shown on the left. */
    private void moveToPanel(final int index, boolean smoothScroll) {
        mHandler.post(() -> {
            if (DEBUG) {
                Log.d(TAG, "Moving to panel " + index);
            }
            if (!isAdded()) {
                return;
            }
            Fragment fragmentToBecomeMainPanel =
                    getChildFragmentManager().findFragmentById(frameResIds[index]);
            Fragment fragmentToBecomePreviewPanel =
                    getChildFragmentManager().findFragmentById(frameResIds[index + 1]);
            // Positive value means that the panel is scrolling to right (navigate forward for LTR
            // or navigate backwards for RTL) and vice versa; 0 means that this is likely invoked
            // by GlobalLayoutListener and there's no actual sliding.
            int distanceToScrollToRight;
            int panelWidth = getResources().getDimensionPixelSize(
                    R.dimen.tp_settings_preference_pane_width);
            View scrollToPanelOverlay = getView().findViewById(frameResOverlayIds[index]);
            View previewPanelOverlay = getView().findViewById(frameResOverlayIds[index + 1]);
            boolean scrollsToPreview =
                    isRTL() ? mScrollView.getScrollX() >= mMaxScrollX - panelWidth * index
                            : mScrollView.getScrollX() <= panelWidth * index;
            boolean hasPreviewFragment = fragmentToBecomePreviewPanel != null
                    && !(fragmentToBecomePreviewPanel instanceof DummyFragment);
            if (smoothScroll) {
                int animationEnd = isRTL() ? mMaxScrollX - panelWidth * index : panelWidth * index;
                distanceToScrollToRight = animationEnd - mScrollView.getScrollX();
                // Slide animation
                ObjectAnimator slideAnim = ObjectAnimator.ofInt(mScrollView, "scrollX",
                        mScrollView.getScrollX(), animationEnd);
                slideAnim.setAutoCancel(true);
                slideAnim.setDuration(PANEL_ANIMATION_MS);
                slideAnim.start();
                // Color animation
                if (scrollsToPreview) {
                    previewPanelOverlay.setAlpha(hasPreviewFragment ? 1f : 0f);
                    ObjectAnimator colorAnim = ObjectAnimator.ofFloat(scrollToPanelOverlay, "alpha",
                            scrollToPanelOverlay.getAlpha(), 0f);
                    colorAnim.setAutoCancel(true);
                    colorAnim.setDuration(PANEL_ANIMATION_MS);
                    colorAnim.start();
                } else {
                    scrollToPanelOverlay.setAlpha(0f);
                    ObjectAnimator colorAnim = ObjectAnimator.ofFloat(previewPanelOverlay, "alpha",
                            previewPanelOverlay.getAlpha(), hasPreviewFragment ? 1f : 0f);
                    colorAnim.setAutoCancel(true);
                    colorAnim.setDuration(PANEL_ANIMATION_MS);
                    colorAnim.start();
                }
            } else {
                int scrollToX = isRTL() ? mMaxScrollX - panelWidth * index : panelWidth * index;
                distanceToScrollToRight = scrollToX - mScrollView.getScrollX();
                mScrollView.scrollTo(scrollToX, 0);
                scrollToPanelOverlay.setAlpha(0f);
                previewPanelOverlay.setAlpha(hasPreviewFragment ? 1f : 0f);
            }
            if (fragmentToBecomeMainPanel != null && fragmentToBecomeMainPanel.getView() != null) {
                fragmentToBecomeMainPanel.getView().requestFocus();
                for (int resId : frameResIds) {
                    Fragment f = getChildFragmentManager().findFragmentById(resId);
                    if (f != null) {
                        View view = f.getView();
                        if (view != null) {
                            view.setImportantForAccessibility(
                                    f == fragmentToBecomeMainPanel
                                            ? View.IMPORTANT_FOR_ACCESSIBILITY_YES
                                            : View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
                        }
                    }
                }
                if (fragmentToBecomeMainPanel instanceof PreviewableComponentCallback) {
                    if (distanceToScrollToRight > 0) {
                        ((PreviewableComponentCallback) fragmentToBecomeMainPanel)
                                .onArriveAtMainPanel(!isRTL());
                    } else if (distanceToScrollToRight < 0) {
                        ((PreviewableComponentCallback) fragmentToBecomeMainPanel)
                                .onArriveAtMainPanel(isRTL());
                    } // distanceToScrollToRight being 0 means no actual panel sliding; thus noop.
                }
            }
        });
    }

    private Fragment getInitialPreviewFragment(Fragment fragment) {
        if (!(fragment instanceof LeanbackPreferenceFragment)) {
            return null;
        }

        LeanbackPreferenceFragment leanbackPreferenceFragment =
                (LeanbackPreferenceFragment) fragment;
        if (leanbackPreferenceFragment.getListView() == null) {
            return null;
        }

        VerticalGridView listView = (VerticalGridView) leanbackPreferenceFragment.getListView();
        int position = listView.getSelectedPosition();
        PreferenceGroupAdapter adapter =
                (PreferenceGroupAdapter) (leanbackPreferenceFragment.getListView().getAdapter());
        Preference chosenPreference = adapter.getItem(position);
        // Find the first focusable preference if cannot find the selected preference
        if (chosenPreference == null || (listView.findViewHolderForPosition(position) != null
                && !listView.findViewHolderForPosition(position).itemView.hasFocusable())) {
            chosenPreference = null;
            for (int i = 0; i < listView.getChildCount(); i++) {
                View view = listView.getChildAt(i);
                if (view.hasFocusable()) {
                    PreferenceViewHolder viewHolder =
                            (PreferenceViewHolder) listView.getChildViewHolder(view);
                    chosenPreference = adapter.getItem(viewHolder.getAdapterPosition());
                    break;
                }
            }
        }

        if (chosenPreference == null) {
            return null;
        }
        return onCreatePreviewFragment(fragment, chosenPreference);
    }

    /**
     * Refocus the current selected preference. When a preference is selected and its InfoFragment
     * slice data changes. We need to call this method to make sure InfoFragment updates in time.
     */
    public void refocusPreference(Fragment fragment) {
        if (!isFragmentInTheMainPanel(fragment)) {
            return;
        }
        Preference chosenPreference = getChosenPreference(fragment);
        try {
            if (chosenPreference != null && chosenPreference.getFragment() != null
                    && InfoFragment.class.isAssignableFrom(
                    Class.forName(chosenPreference.getFragment()))) {
                onPreferenceFocused(chosenPreference);
            }
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
    }

    private static Preference getChosenPreference(Fragment fragment) {
        if (!(fragment instanceof LeanbackPreferenceFragment)) {
            return null;
        }

        LeanbackPreferenceFragment leanbackPreferenceFragment =
                (LeanbackPreferenceFragment) fragment;
        if (leanbackPreferenceFragment.getListView() == null) {
            return null;
        }

        VerticalGridView listView = (VerticalGridView) leanbackPreferenceFragment.getListView();
        int position = listView.getSelectedPosition();
        PreferenceGroupAdapter adapter =
                (PreferenceGroupAdapter) (leanbackPreferenceFragment.getListView().getAdapter());
        Preference chosenPreference = adapter.getItem(position);
        return chosenPreference;
    }

    /** Creates preview preference fragment. */
    public Fragment onCreatePreviewFragment(Fragment caller, Preference preference) {
        if (preference.getFragment() != null) {
            if (!shouldDisplay(preference.getFragment())) {
                return null;
            }
            if (preference instanceof HasSliceUri) {
                HasSliceUri slicePref = (HasSliceUri) preference;
                if (slicePref.getUri() == null || !isUriValid(slicePref.getUri())) {
                    return null;
                }
                Bundle b = preference.getExtras();
                b.putString(SlicesConstants.TAG_TARGET_URI, slicePref.getUri());
                b.putCharSequence(SlicesConstants.TAG_SCREEN_TITLE, preference.getTitle());
            }
            return Fragment.instantiate(getActivity(), preference.getFragment(),
                    preference.getExtras());
        } else {
            Fragment f = null;
            if (preference instanceof ListPreference) {
                f = TwoPanelListPreferenceDialogFragment.newInstanceSingle(preference.getKey());
            } else if (preference instanceof MultiSelectListPreference) {
                f = LeanbackListPreferenceDialogFragment.newInstanceMulti(preference.getKey());
            }
            if (f != null && caller != null) {
                f.setTargetFragment(caller, 0);
            }
            return f;
        }
    }

    private boolean isUriValid(String uri) {
        if (uri == null) {
            return false;
        }
        ContentProviderClient client =
                getContext().getContentResolver().acquireContentProviderClient(Uri.parse(uri));
        if (client != null) {
            client.close();
            return true;
        } else {
            return false;
        }
    }

    /** Add focus listener to the child fragment **/
    public void addListenerForFragment(Fragment fragment) {
        if (isFragmentInTheMainPanel(fragment)) {
            addOrRemovePreferenceFocusedListener(fragment, true);
        }
    }

    /** Remove focus listener from the child fragment **/
    public void removeListenerForFragment(Fragment fragment) {
        addOrRemovePreferenceFocusedListener(fragment, false);
    }

    /** Check if fragment is in the main panel **/
    public boolean isFragmentInTheMainPanel(Fragment fragment) {
        return fragment == getChildFragmentManager().findFragmentById(frameResIds[mPrefPanelIdx]);
    }
}
