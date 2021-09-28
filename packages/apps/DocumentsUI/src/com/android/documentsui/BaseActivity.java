/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.documentsui;

import static com.android.documentsui.base.Shared.EXTRA_BENCHMARK;
import static com.android.documentsui.base.SharedMinimal.DEBUG;
import static com.android.documentsui.base.State.MODE_GRID;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.MessageQueue.IdleHandler;
import android.preference.PreferenceManager;
import android.provider.DocumentsContract;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Checkable;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;

import com.android.documentsui.AbstractActionHandler.CommonAddons;
import com.android.documentsui.Injector.Injected;
import com.android.documentsui.NavigationViewManager.Breadcrumb;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.EventHandler;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.Shared;
import com.android.documentsui.base.State;
import com.android.documentsui.base.State.ViewMode;
import com.android.documentsui.base.UserId;
import com.android.documentsui.dirlist.AnimationView;
import com.android.documentsui.dirlist.AppsRowManager;
import com.android.documentsui.dirlist.DirectoryFragment;
import com.android.documentsui.prefs.LocalPreferences;
import com.android.documentsui.prefs.PreferencesMonitor;
import com.android.documentsui.queries.CommandInterceptor;
import com.android.documentsui.queries.SearchChipData;
import com.android.documentsui.queries.SearchFragment;
import com.android.documentsui.queries.SearchViewManager;
import com.android.documentsui.queries.SearchViewManager.SearchManagerListener;
import com.android.documentsui.roots.ProvidersCache;
import com.android.documentsui.sidebar.RootsFragment;
import com.android.documentsui.sorting.SortController;
import com.android.documentsui.sorting.SortModel;

import com.google.android.material.appbar.AppBarLayout;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import javax.annotation.Nullable;

public abstract class BaseActivity
        extends AppCompatActivity implements CommonAddons, NavigationViewManager.Environment {

    private static final String BENCHMARK_TESTING_PACKAGE = "com.android.documentsui.appperftests";

    protected SearchViewManager mSearchManager;
    protected AppsRowManager mAppsRowManager;
    protected UserIdManager mUserIdManager;
    protected State mState;

    @Injected
    protected Injector<?> mInjector;

    protected ProvidersCache mProviders;
    protected DocumentsAccess mDocs;
    protected DrawerController mDrawer;

    protected NavigationViewManager mNavigator;
    protected SortController mSortController;

    private final List<EventListener> mEventListeners = new ArrayList<>();
    private final String mTag;

    @LayoutRes
    private int mLayoutId;

    private RootsMonitor<BaseActivity> mRootsMonitor;

    private long mStartTime;
    private boolean mHasQueryContentFromIntent;

    private PreferencesMonitor mPreferencesMonitor;

    public BaseActivity(@LayoutRes int layoutId, String tag) {
        mLayoutId = layoutId;
        mTag = tag;
    }

    protected abstract void refreshDirectory(int anim);
    /** Allows sub-classes to include information in a newly created State instance. */
    protected abstract void includeState(State initialState);
    protected abstract void onDirectoryCreated(DocumentInfo doc);

    public abstract Injector<?> getInjector();

    @CallSuper
    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Record the time when onCreate is invoked for metric.
        mStartTime = new Date().getTime();

        // ToDo Create tool to check resource version before applyStyle for the theme
        // If version code is not match, we should reset overlay package to default,
        // in case Activity continueusly encounter resource not found exception
        getTheme().applyStyle(R.style.DocumentsDefaultTheme, false);

        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();

        addListenerForLaunchCompletion();

        setContentView(mLayoutId);

        setContainer();

        mInjector = getInjector();
        mState = getState(savedInstanceState);
        mDrawer = DrawerController.create(this, mInjector.config);
        Metrics.logActivityLaunch(mState, intent);

        mProviders = DocumentsApplication.getProvidersCache(this);
        mDocs = DocumentsAccess.create(this, mState);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        Breadcrumb breadcrumb = findViewById(R.id.horizontal_breadcrumb);
        assert(breadcrumb != null);
        View profileTabsContainer = findViewById(R.id.tabs_container);
        assert (profileTabsContainer != null);

        mNavigator = new NavigationViewManager(this, mDrawer, mState, this, breadcrumb,
                profileTabsContainer, DocumentsApplication.getUserIdManager(this));
        SearchManagerListener searchListener = new SearchManagerListener() {
            /**
             * Called when search results changed. Refreshes the content of the directory. It
             * doesn't refresh elements on the action bar. e.g. The current directory name displayed
             * on the action bar won't get updated.
             */
            @Override
            public void onSearchChanged(@Nullable String query) {
                if (mSearchManager.isSearching()) {
                    Metrics.logSearchMode(query != null, mSearchManager.hasCheckedChip());
                    if (mInjector.pickResult != null) {
                        mInjector.pickResult.increaseActionCount();
                    }
                }

                mInjector.actions.loadDocumentsForCurrentStack();

                expandAppBar();
                DirectoryFragment dir = getDirectoryFragment();
                if (dir != null) {
                    dir.scrollToTop();
                }
            }

            @Override
            public void onSearchFinished() {
                // Restores menu icons state
                invalidateOptionsMenu();
            }

            @Override
            public void onSearchViewChanged(boolean opened) {
                mNavigator.update();
                // We also need to update AppsRowManager because we may want to show/hide the
                // appsRow in cross-profile search according to the searching conditions.
                mAppsRowManager.updateView(BaseActivity.this);
            }

            @Override
            public void onSearchChipStateChanged(View v) {
                final Checkable chip = (Checkable) v;
                if (chip.isChecked()) {
                    final SearchChipData item = (SearchChipData) v.getTag();
                    Metrics.logUserAction(MetricConsts.USER_ACTION_SEARCH_CHIP);
                    Metrics.logSearchType(item.getChipType());
                }
                // We also need to update AppsRowManager because we may want to show/hide the
                // appsRow in cross-profile search according to the searching conditions.
                mAppsRowManager.updateView(BaseActivity.this);
            }

            @Override
            public void onSearchViewFocusChanged(boolean hasFocus) {
                final boolean isInitailSearch
                        = !TextUtils.isEmpty(mSearchManager.getCurrentSearch())
                        && TextUtils.isEmpty(mSearchManager.getSearchViewText());
                if (hasFocus) {
                    if (!isInitailSearch) {
                        SearchFragment.showFragment(getSupportFragmentManager(),
                                mSearchManager.getSearchViewText());
                    }
                } else {
                    SearchFragment.dismissFragment(getSupportFragmentManager());
                }
            }

            @Override
            public void onSearchViewClearClicked() {
                if (SearchFragment.get(getSupportFragmentManager()) == null) {
                    SearchFragment.showFragment(getSupportFragmentManager(),
                            mSearchManager.getSearchViewText());
                }
            }
        };

        // "Commands" are meta input for controlling system behavior.
        // We piggy back on search input as it is the only text input
        // area in the app. But the functionality is independent
        // of "regular" search query processing.
        final CommandInterceptor cmdInterceptor = new CommandInterceptor(mInjector.features);
        cmdInterceptor.add(new CommandInterceptor.DumpRootsCacheHandler(this));

        // A tiny decorator that adds support for enabling CommandInterceptor
        // based on query input. It's sorta like CommandInterceptor, but its metaaahhh.
        EventHandler<String> queryInterceptor =
                CommandInterceptor.createDebugModeFlipper(
                        mInjector.features,
                        mInjector.debugHelper::toggleDebugMode,
                        cmdInterceptor);

        ViewGroup chipGroup = findViewById(R.id.search_chip_group);
        mUserIdManager = DocumentsApplication.getUserIdManager(this);
        mSearchManager = new SearchViewManager(searchListener, queryInterceptor,
                chipGroup, savedInstanceState);
        // initialize the chip sets by accept mime types
        mSearchManager.initChipSets(mState.acceptMimes);
        // update the chip items by the mime types of the root
        mSearchManager.updateChips(getCurrentRoot().derivedMimeTypes);
        // parse the query content from intent when launch the
        // activity at the first time
        if (savedInstanceState == null) {
            mHasQueryContentFromIntent = mSearchManager.parseQueryContentFromIntent(getIntent(),
                    mState.action);
        }

        mNavigator.setSearchBarClickListener(v -> {
            mSearchManager.onSearchBarClicked();
            mNavigator.update();
        });

        mNavigator.setProfileTabsListener(userId -> {
            // There are several possible cases that may trigger this callback.
            // 1. A user click on tab layout.
            // 2. A user click on tab layout, when filter is checked. (searching = true)
            // 3. A user click on a open a dir of a different user in search (stack size > 1)
            // 4. After tab layout is initialized.

            if (!mState.stack.isInitialized()) {
                return;
            }

            // Reload the roots when the selected user is changed.
            // After reloading, we have visually same roots in the drawer. But they are
            // different by holding different userId. Next time when user select a root, it can
            // bring the user to correct root doc.
            final RootsFragment roots = RootsFragment.get(getSupportFragmentManager());
            if (roots != null) {
                roots.onSelectedUserChanged();
            }

            if (mState.stack.size() <= 1) {
                // We do not load cross-profile root if the stack contains two documents. The
                // stack may contain >1 docs when the user select a folder of the other user in
                // search. In that case, we don't want to reload the root. The whole stack
                // and the root will be updated in openFolderInSearchResult.

                // When a user filters files by search chips on the root doc, we will be in
                // searching mode and with stack size 1 (0 if rootDoc cannot be loaded).
                // The activity will clear search on root picked. If we don't clear the search,
                // user may see the search result screen show up briefly and then get cleared.
                mSearchManager.cancelSearch();
                mInjector.actions.loadCrossProfileRoot(getCurrentRoot(), userId);
            }
        });

        mSortController = SortController.create(this, mState.derivedMode, mState.sortModel);

        mPreferencesMonitor = new PreferencesMonitor(
                getApplicationContext().getPackageName(),
                PreferenceManager.getDefaultSharedPreferences(this),
                this::onPreferenceChanged);
        mPreferencesMonitor.start();

        // Base classes must update result in their onCreate.
        setResult(AppCompatActivity.RESULT_CANCELED);
    }

    public void onPreferenceChanged(String pref) {
        // For now, we only work with prefs that we backup. This
        // just limits the scope of what we expect to come flowing
        // through here until we know we want more and fancier options.
        assert (LocalPreferences.shouldBackup(pref));
    }

    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);

        mRootsMonitor = new RootsMonitor<>(
                this,
                mInjector.actions,
                mProviders,
                mDocs,
                mState,
                mSearchManager,
                mInjector.actionModeController::finishActionMode);
        mRootsMonitor.start();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        boolean showMenu = super.onCreateOptionsMenu(menu);

        getMenuInflater().inflate(R.menu.activity, menu);
        mNavigator.update();
        boolean fullBarSearch = getResources().getBoolean(R.bool.full_bar_search_view);
        boolean showSearchBar = getResources().getBoolean(R.bool.show_search_bar);
        mSearchManager.install(menu, fullBarSearch, showSearchBar);

        final ActionMenuView subMenuView = findViewById(R.id.sub_menu);
        // If size is 0, it means the menu has not inflated and it should only do once.
        if (subMenuView.getMenu().size() == 0) {
            subMenuView.setOnMenuItemClickListener(this::onOptionsItemSelected);
            getMenuInflater().inflate(R.menu.sub_menu, subMenuView.getMenu());
        }

        return showMenu;
    }

    @Override
    @CallSuper
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        mSearchManager.showMenu(mState.stack);
        final ActionMenuView subMenuView = findViewById(R.id.sub_menu);
        mInjector.menuManager.updateSubMenu(subMenuView.getMenu());
        return true;
    }

    @Override
    protected void onDestroy() {
        mRootsMonitor.stop();
        mPreferencesMonitor.stop();
        mSortController.destroy();
        super.onDestroy();
    }

    private State getState(@Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            State state = savedInstanceState.<State>getParcelable(Shared.EXTRA_STATE);
            if (DEBUG) {
                Log.d(mTag, "Recovered existing state object: " + state);
            }
            return state;
        }

        State state = new State();

        final Intent intent = getIntent();

        state.sortModel = SortModel.createModel();
        state.localOnly = intent.getBooleanExtra(Intent.EXTRA_LOCAL_ONLY, false);
        state.excludedAuthorities = getExcludedAuthorities();
        state.restrictScopeStorage = Shared.shouldRestrictStorageAccessFramework(this);
        state.showHiddenFiles = LocalPreferences.getShowHiddenFiles(
                getApplicationContext(),
                getApplicationContext()
                        .getResources()
                        .getBoolean(R.bool.show_hidden_files_by_default));

        includeState(state);

        if (DEBUG) {
            Log.d(mTag, "Created new state object: " + state);
        }

        return state;
    }

    private void setContainer() {
        View root = findViewById(R.id.coordinator_layout);
        root.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            root.setPadding(insets.getSystemWindowInsetLeft(),
                    insets.getSystemWindowInsetTop(), insets.getSystemWindowInsetRight(), 0);

            View saveContainer = findViewById(R.id.container_save);
            saveContainer.setPadding(0, 0, 0, insets.getSystemWindowInsetBottom());

            View rootsContainer = findViewById(R.id.container_roots);
            rootsContainer.setPadding(0, 0, 0, insets.getSystemWindowInsetBottom());

            return insets.consumeSystemWindowInsets();
        });

        getWindow().setNavigationBarDividerColor(Color.TRANSPARENT);
        if (Build.VERSION.SDK_INT >= 29) {
            getWindow().setNavigationBarColor(Color.TRANSPARENT);
            getWindow().setNavigationBarContrastEnforced(true);
        } else {
            getWindow().setNavigationBarColor(getColor(R.color.nav_bar_translucent));
        }
    }

    @Override
    public void setRootsDrawerOpen(boolean open) {
        mNavigator.revealRootsDrawer(open);
    }

    @Override
    public void setRootsDrawerLocked(boolean locked) {
        mDrawer.setLocked(locked);
        mNavigator.update();
    }

    @Override
    public void onRootPicked(RootInfo root) {
        // Clicking on the current root removes search
        mSearchManager.cancelSearch();

        // Skip refreshing if root nor directory didn't change
        if (root.equals(getCurrentRoot()) && mState.stack.size() <= 1) {
            return;
        }

        mInjector.actionModeController.finishActionMode();
        mSortController.onViewModeChanged(mState.derivedMode);

        // Set summary header's visibility. Only recents and downloads root may have summary in
        // their docs.
        mState.sortModel.setDimensionVisibility(
                SortModel.SORT_DIMENSION_ID_SUMMARY,
                root.isRecents() || root.isDownloads() ? View.VISIBLE : View.INVISIBLE);

        // Clear entire backstack and start in new root
        mState.stack.changeRoot(root);

        // Recents is always in memory, so we just load it directly.
        // Otherwise we delegate loading data from disk to a task
        // to ensure a responsive ui.
        if (mProviders.isRecentsRoot(root)) {
            refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);
        } else {
            mInjector.actions.getRootDocument(
                    root,
                    TimeoutTask.DEFAULT_TIMEOUT,
                    doc -> mInjector.actions.openRootDocument(doc));
        }

        expandAppBar();
        updateHeaderTitle();
    }

    protected ProfileTabsAddons getProfileTabsAddon() {
        return mNavigator.getProfileTabsAddons();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        switch (item.getItemId()) {
            case android.R.id.home:
                onBackPressed();
                return true;

            case R.id.option_menu_create_dir:
                getInjector().actions.showCreateDirectoryDialog();
                return true;

            case R.id.option_menu_search:
                // SearchViewManager listens for this directly.
                return false;

            case R.id.option_menu_select_all:
                getInjector().actions.selectAllFiles();
                return true;

            case R.id.option_menu_debug:
                getInjector().actions.showDebugMessage();
                return true;

            case R.id.option_menu_sort:
                getInjector().actions.showSortDialog();
                return true;

            case R.id.option_menu_launcher:
                getInjector().actions.switchLauncherIcon();
                return true;

            case R.id.option_menu_show_hidden_files:
                onClickedShowHiddenFiles();
                return true;

            case R.id.sub_menu_grid:
                setViewMode(State.MODE_GRID);
                return true;

            case R.id.sub_menu_list:
                setViewMode(State.MODE_LIST);
                return true;

            default:
                return super.onOptionsItemSelected(item);
        }
    }

    protected final @Nullable DirectoryFragment getDirectoryFragment() {
        return DirectoryFragment.get(getSupportFragmentManager());
    }

    /**
     * Returns true if a directory can be created in the current location.
     * @return
     */
    protected boolean canCreateDirectory() {
        final RootInfo root = getCurrentRoot();
        final DocumentInfo cwd = getCurrentDirectory();
        return cwd != null
                && cwd.isCreateSupported()
                && !mSearchManager.isSearching()
                && !root.isRecents();
    }

    /**
     * Returns true if a directory can be inspected.
     */
    protected boolean canInspectDirectory() {
        return false;
    }

    // TODO: make navigator listen to state
    @Override
    public final void updateNavigator() {
        mNavigator.update();
    }

    @Override
    public void restoreRootAndDirectory() {
        // We're trying to restore stuff in document stack from saved instance. If we didn't have a
        // chance to spawn a fragment before we need to do it now. However if we spawned a fragment
        // already, system will automatically restore the fragment for us so we don't need to do
        // that manually this time.
        if (DirectoryFragment.get(getSupportFragmentManager()) == null) {
            refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);
        }
    }

    /**
     * Refreshes the content of the director and the menu/action bar.
     * The current directory name and selection will get updated.
     * @param anim
     */
    @Override
    public final void refreshCurrentRootAndDirectory(int anim) {
        mSearchManager.cancelSearch();

        // only set the query content in the first launch
        if (mHasQueryContentFromIntent) {
            mHasQueryContentFromIntent = false;
            mSearchManager.setCurrentSearch(mSearchManager.getQueryContentFromIntent());
        }

        mState.derivedMode = LocalPreferences.getViewMode(this, mState.stack.getRoot(), MODE_GRID);

        mNavigator.update();

        refreshDirectory(anim);

        final RootsFragment roots = RootsFragment.get(getSupportFragmentManager());
        if (roots != null) {
            roots.onCurrentRootChanged();
        }

        String appName = getString(R.string.files_label);
        String currentTitle = getTitle() != null ? getTitle().toString() : "";
        if (currentTitle.equals(appName)) {
            // First launch, TalkBack announces app name.
            getWindow().getDecorView().announceForAccessibility(appName);
        }

        String newTitle = mState.stack.getTitle();
        if (newTitle != null) {
            // Causes talkback to announce the activity's new title
            setTitle(newTitle);
        }

        invalidateOptionsMenu();
        mSortController.onViewModeChanged(mState.derivedMode);
        mSearchManager.updateChips(getCurrentRoot().derivedMimeTypes);
        mAppsRowManager.updateView(this);
    }

    private final List<String> getExcludedAuthorities() {
        List<String> authorities = new ArrayList<>();
        if (getIntent().getBooleanExtra(DocumentsContract.EXTRA_EXCLUDE_SELF, false)) {
            // Exclude roots provided by the calling package.
            String packageName = Shared.getCallingPackageName(this);
            try {
                PackageInfo pkgInfo = getPackageManager().getPackageInfo(packageName,
                        PackageManager.GET_PROVIDERS);
                for (ProviderInfo provider: pkgInfo.providers) {
                    authorities.add(provider.authority);
                }
            } catch (PackageManager.NameNotFoundException e) {
                Log.e(mTag, "Calling package name does not resolve: " + packageName);
            }
        }
        return authorities;
    }

    public static BaseActivity get(Fragment fragment) {
        return (BaseActivity) fragment.getActivity();
    }

    public State getDisplayState() {
        return mState;
    }

    /**
     * Updates hidden files visibility based on user action.
     */
    private void onClickedShowHiddenFiles() {
        boolean showHiddenFiles = !mState.showHiddenFiles;
        Context context = getApplicationContext();

        Metrics.logUserAction(showHiddenFiles
                ? MetricConsts.USER_ACTION_SHOW_HIDDEN_FILES
                : MetricConsts.USER_ACTION_HIDE_HIDDEN_FILES);
        LocalPreferences.setShowHiddenFiles(context, showHiddenFiles);
        mState.showHiddenFiles = showHiddenFiles;

        // Calls this to trigger either MultiRootDocumentsLoader or DirectoryLoader reloading.
        mInjector.actions.loadDocumentsForCurrentStack();
    }

    /**
     * Set mode based on explicit user action.
     */
    void setViewMode(@ViewMode int mode) {
        if (mode == State.MODE_GRID) {
            Metrics.logUserAction(MetricConsts.USER_ACTION_GRID);
        } else if (mode == State.MODE_LIST) {
            Metrics.logUserAction(MetricConsts.USER_ACTION_LIST);
        }

        LocalPreferences.setViewMode(this, getCurrentRoot(), mode);
        mState.derivedMode = mode;

        final ActionMenuView subMenuView = findViewById(R.id.sub_menu);
        mInjector.menuManager.updateSubMenu(subMenuView.getMenu());

        DirectoryFragment dir = getDirectoryFragment();
        if (dir != null) {
            dir.onViewModeChanged();
        }

        mSortController.onViewModeChanged(mode);
    }

    /**
     * Reload documnets by current stack in certain situation.
     */
    public void reloadDocumentsIfNeeded() {
        if (isInRecents() || mSearchManager.isSearching()) {
            // Both using MultiRootDocumentsLoader which have not ContentObserver.
            mInjector.actions.loadDocumentsForCurrentStack();
        }
    }

    public void expandAppBar() {
        final AppBarLayout appBarLayout = findViewById(R.id.app_bar);
        if (appBarLayout != null) {
            appBarLayout.setExpanded(true);
        }
    }

    public void updateHeaderTitle() {
        if (!mState.stack.isInitialized()) {
            //stack has not initialized, the header will update after the stack finishes loading
            return;
        }

        final RootInfo root = mState.stack.getRoot();
        final String rootTitle = root.title;
        String result;

        switch (root.derivedType) {
            case RootInfo.TYPE_RECENTS:
                result = getHeaderRecentTitle();
                break;
            case RootInfo.TYPE_IMAGES:
            case RootInfo.TYPE_VIDEO:
            case RootInfo.TYPE_AUDIO:
                result = rootTitle;
                break;
            case RootInfo.TYPE_DOWNLOADS:
                result = getHeaderDownloadsTitle();
                break;
            case RootInfo.TYPE_LOCAL:
            case RootInfo.TYPE_MTP:
            case RootInfo.TYPE_SD:
            case RootInfo.TYPE_USB:
                result = getHeaderStorageTitle(rootTitle);
                break;
            default:
                final String summary = root.summary;
                result = getHeaderDefaultTitle(rootTitle, summary);
                break;
        }

        TextView headerTitle = findViewById(R.id.header_title);
        headerTitle.setText(result);
    }

    private String getHeaderRecentTitle() {
        // If stack size larger than 1, it means user global search than enter a folder, but search
        // is not expanded on that time.
        boolean isGlobalSearch = mSearchManager.isSearching() || mState.stack.size() > 1;
        if (mState.isPhotoPicking()) {
            final int resId = isGlobalSearch
                    ? R.string.root_info_header_image_global_search
                    : R.string.root_info_header_image_recent;
            return getString(resId);
        } else {
            final int resId = isGlobalSearch
                    ? R.string.root_info_header_global_search
                    : R.string.root_info_header_recent;
            return getString(resId);
        }
    }

    private String getHeaderDownloadsTitle() {
        return getString(mState.isPhotoPicking()
            ? R.string.root_info_header_image_downloads : R.string.root_info_header_downloads);
    }

    private String getHeaderStorageTitle(String rootTitle) {
        if (mState.stack.size() > 1) {
            final int resId = mState.isPhotoPicking()
                    ? R.string.root_info_header_image_folder : R.string.root_info_header_folder;
            return getString(resId, getCurrentTitle());
        } else {
            final int resId = mState.isPhotoPicking()
                    ? R.string.root_info_header_image_storage : R.string.root_info_header_storage;
            return getString(resId, rootTitle);
        }
    }

    private String getHeaderDefaultTitle(String rootTitle, String summary) {
        if (TextUtils.isEmpty(summary)) {
            final int resId = mState.isPhotoPicking()
                    ? R.string.root_info_header_image_app : R.string.root_info_header_app;
            return getString(resId, rootTitle);
        } else {
            final int resId = mState.isPhotoPicking()
                    ? R.string.root_info_header_image_app_with_summary
                    : R.string.root_info_header_app_with_summary;
            return getString(resId, rootTitle, summary);
        }
    }

    /**
     * Get title string equal to the string action bar displayed.
     * @return current directory title name
     */
    public String getCurrentTitle() {
        if (!mState.stack.isInitialized()) {
            return null;
        }

        if (mState.stack.size() > 1) {
            return getCurrentDirectory().displayName;
        } else {
            return getCurrentRoot().title;
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        super.onSaveInstanceState(state);
        state.putParcelable(Shared.EXTRA_STATE, mState);
        mSearchManager.onSaveInstanceState(state);
    }

    @Override
    public boolean isSearchExpanded() {
        return mSearchManager.isExpanded();
    }

    @Override
    public UserId getSelectedUser() {
        return mNavigator.getSelectedUser();
    }

    public RootInfo getCurrentRoot() {
        RootInfo root = mState.stack.getRoot();
        if (root != null) {
            return root;
        } else {
            return mProviders.getRecentsRoot(getSelectedUser());
        }
    }

    @Override
    public DocumentInfo getCurrentDirectory() {
        return mState.stack.peek();
    }

    @Override
    public boolean isInRecents() {
        return mState.stack.isRecents();
    }

    @VisibleForTesting
    public void addEventListener(EventListener listener) {
        mEventListeners.add(listener);
    }

    @VisibleForTesting
    public void removeEventListener(EventListener listener) {
        mEventListeners.remove(listener);
    }

    @VisibleForTesting
    public void notifyDirectoryLoaded(Uri uri) {
        for (EventListener listener : mEventListeners) {
            listener.onDirectoryLoaded(uri);
        }
    }

    @VisibleForTesting
    @Override
    public void notifyDirectoryNavigated(Uri uri) {
        for (EventListener listener : mEventListeners) {
            listener.onDirectoryNavigated(uri);
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            mInjector.debugHelper.debugCheck(event.getDownTime(), event.getKeyCode());
        }

        DocumentsApplication.getDragAndDropManager(this).onKeyEvent(event);

        return super.dispatchKeyEvent(event);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mInjector.actions.onActivityResult(requestCode, resultCode, data);
    }

    /**
     * Pops the top entry off the directory stack, and returns the user to the previous directory.
     * If the directory stack only contains one item, this method does nothing.
     *
     * @return Whether the stack was popped.
     */
    protected boolean popDir() {
        if (mState.stack.size() > 1) {
            final DirectoryFragment fragment = getDirectoryFragment();
            if (fragment != null) {
                fragment.stopScroll();
            }

            mState.stack.pop();
            refreshCurrentRootAndDirectory(AnimationView.ANIM_LEAVE);
            return true;
        }
        return false;
    }

    protected boolean focusSidebar() {
        RootsFragment rf = RootsFragment.get(getSupportFragmentManager());
        assert (rf != null);
        return rf.requestFocus();
    }

    /**
     * Closes the activity when it's idle.
     */
    private void addListenerForLaunchCompletion() {
        addEventListener(new EventListener() {
            @Override
            public void onDirectoryNavigated(Uri uri) {
            }

            @Override
            public void onDirectoryLoaded(Uri uri) {
                removeEventListener(this);
                getMainLooper().getQueue().addIdleHandler(new IdleHandler() {
                    @Override
                    public boolean queueIdle() {
                        // If startup benchmark is requested by a whitelisted testing package, then
                        // close the activity once idle, and notify the testing activity.
                        if (getIntent().getBooleanExtra(EXTRA_BENCHMARK, false) &&
                                BENCHMARK_TESTING_PACKAGE.equals(getCallingPackage())) {
                            setResult(RESULT_OK);
                            finish();
                        }

                        Metrics.logStartupMs((int) (new Date().getTime() - mStartTime));

                        // Remove the idle handler.
                        return false;
                    }
                });
            }
        });
    }

    @VisibleForTesting
    protected interface EventListener {
        /**
         * @param uri Uri navigated to. If recents, then null.
         */
        void onDirectoryNavigated(@Nullable Uri uri);

        /**
         * @param uri Uri of the loaded directory. If recents, then null.
         */
        void onDirectoryLoaded(@Nullable Uri uri);
    }
}
