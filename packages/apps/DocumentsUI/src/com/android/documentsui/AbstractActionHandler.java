/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static com.android.documentsui.base.DocumentInfo.getCursorInt;
import static com.android.documentsui.base.DocumentInfo.getCursorString;
import static com.android.documentsui.base.SharedMinimal.DEBUG;

import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcelable;
import android.provider.DocumentsContract;
import android.util.Log;
import android.util.Pair;
import android.view.DragEvent;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;
import androidx.loader.app.LoaderManager.LoaderCallbacks;
import androidx.loader.content.Loader;
import androidx.recyclerview.selection.ItemDetailsLookup.ItemDetails;
import androidx.recyclerview.selection.MutableSelection;
import androidx.recyclerview.selection.SelectionTracker;

import com.android.documentsui.AbstractActionHandler.CommonAddons;
import com.android.documentsui.LoadDocStackTask.LoadDocStackCallback;
import com.android.documentsui.base.BooleanConsumer;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.DocumentStack;
import com.android.documentsui.base.Lookup;
import com.android.documentsui.base.MimeTypes;
import com.android.documentsui.base.Providers;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.Shared;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.dirlist.AnimationView;
import com.android.documentsui.dirlist.AnimationView.AnimationType;
import com.android.documentsui.dirlist.FocusHandler;
import com.android.documentsui.files.LauncherActivity;
import com.android.documentsui.files.QuickViewIntentBuilder;
import com.android.documentsui.queries.SearchViewManager;
import com.android.documentsui.roots.GetRootDocumentTask;
import com.android.documentsui.roots.LoadFirstRootTask;
import com.android.documentsui.roots.LoadRootTask;
import com.android.documentsui.roots.ProvidersAccess;
import com.android.documentsui.sidebar.EjectRootTask;
import com.android.documentsui.sorting.SortListFragment;
import com.android.documentsui.ui.DialogController;
import com.android.documentsui.ui.Snackbars;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

import javax.annotation.Nullable;

/**
 * Provides support for specializing the actions (openDocument etc.) to the host activity.
 */
public abstract class AbstractActionHandler<T extends FragmentActivity & CommonAddons>
        implements ActionHandler {

    @VisibleForTesting
    public static final int CODE_AUTHENTICATION = 43;

    @VisibleForTesting
    static final int LOADER_ID = 42;

    private static final String TAG = "AbstractActionHandler";
    private static final int REFRESH_SPINNER_TIMEOUT = 500;

    protected final T mActivity;
    protected final State mState;
    protected final ProvidersAccess mProviders;
    protected final DocumentsAccess mDocs;
    protected final FocusHandler mFocusHandler;
    protected final SelectionTracker<String> mSelectionMgr;
    protected final SearchViewManager mSearchMgr;
    protected final Lookup<String, Executor> mExecutors;
    protected final DialogController mDialogs;
    protected final Model mModel;
    protected final Injector<?> mInjector;

    private final LoaderBindings mBindings;

    private Runnable mDisplayStateChangedListener;

    private ContentLock mContentLock;

    @Override
    public void registerDisplayStateChangedListener(Runnable l) {
        mDisplayStateChangedListener = l;
    }
    @Override
    public void unregisterDisplayStateChangedListener(Runnable l) {
        if (mDisplayStateChangedListener == l) {
            mDisplayStateChangedListener = null;
        }
    }

    public AbstractActionHandler(
            T activity,
            State state,
            ProvidersAccess providers,
            DocumentsAccess docs,
            SearchViewManager searchMgr,
            Lookup<String, Executor> executors,
            Injector<?> injector) {

        assert(activity != null);
        assert(state != null);
        assert(providers != null);
        assert(searchMgr != null);
        assert(docs != null);
        assert(injector != null);

        mActivity = activity;
        mState = state;
        mProviders = providers;
        mDocs = docs;
        mFocusHandler = injector.focusManager;
        mSelectionMgr = injector.selectionMgr;
        mSearchMgr = searchMgr;
        mExecutors = executors;
        mDialogs = injector.dialogs;
        mModel = injector.getModel();
        mInjector = injector;

        mBindings = new LoaderBindings();
    }

    @Override
    public void ejectRoot(RootInfo root, BooleanConsumer listener) {
        new EjectRootTask(
                mActivity.getContentResolver(),
                root.authority,
                root.rootId,
                listener).executeOnExecutor(ProviderExecutor.forAuthority(root.authority));
    }

    @Override
    public void startAuthentication(PendingIntent intent) {
        try {
            mActivity.startIntentSenderForResult(intent.getIntentSender(), CODE_AUTHENTICATION,
                    null, 0, 0, 0);
        } catch (IntentSender.SendIntentException cancelled) {
            Log.d(TAG, "Authentication Pending Intent either canceled or ignored.");
        }
    }

    @Override
    public void requestQuietModeDisabled(RootInfo info, UserId userId) {
        new RequestQuietModeDisabledTask(mActivity, userId).execute();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case CODE_AUTHENTICATION:
                onAuthenticationResult(resultCode);
                break;
        }
    }

    private void onAuthenticationResult(int resultCode) {
        if (resultCode == FragmentActivity.RESULT_OK) {
            Log.v(TAG, "Authentication was successful. Refreshing directory now.");
            mActivity.refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);
        }
    }

    @Override
    public void getRootDocument(RootInfo root, int timeout, Consumer<DocumentInfo> callback) {
        GetRootDocumentTask task = new GetRootDocumentTask(
                root,
                mActivity,
                timeout,
                mDocs,
                callback);

        task.executeOnExecutor(mExecutors.lookup(root.authority));
    }

    @Override
    public void refreshDocument(DocumentInfo doc, BooleanConsumer callback) {
        RefreshTask task = new RefreshTask(
                mInjector.features,
                mState,
                doc,
                REFRESH_SPINNER_TIMEOUT,
                mActivity.getApplicationContext(),
                mActivity::isDestroyed,
                callback);
        task.executeOnExecutor(mExecutors.lookup(doc == null ? null : doc.authority));
    }

    @Override
    public void openSelectedInNewWindow() {
        throw new UnsupportedOperationException("Can't open in new window.");
    }

    @Override
    public void openInNewWindow(DocumentStack path) {
        Metrics.logUserAction(MetricConsts.USER_ACTION_NEW_WINDOW);

        Intent intent = LauncherActivity.createLaunchIntent(mActivity);
        intent.putExtra(Shared.EXTRA_STACK, (Parcelable) path);

        // Multi-window necessitates we pick how we are launched.
        // By default we'd be launched in-place above the existing app.
        // By setting launch-to-side ActivityManager will open us to side.
        if (mActivity.isInMultiWindowMode()) {
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }

        mActivity.startActivity(intent);
    }

    @Override
    public boolean openItem(ItemDetails<String> doc, @ViewType int type, @ViewType int fallback) {
        throw new UnsupportedOperationException("Can't open document.");
    }

    @Override
    public void showInspector(DocumentInfo doc) {
        throw new UnsupportedOperationException("Can't open properties.");
    }

    @Override
    public void springOpenDirectory(DocumentInfo doc) {
        throw new UnsupportedOperationException("Can't spring open directories.");
    }

    @Override
    public void openSettings(RootInfo root) {
        throw new UnsupportedOperationException("Can't open settings.");
    }

    @Override
    public void openRoot(ResolveInfo app, UserId userId) {
        throw new UnsupportedOperationException("Can't open an app.");
    }

    @Override
    public void showAppDetails(ResolveInfo info, UserId userId) {
        throw new UnsupportedOperationException("Can't show app details.");
    }

    @Override
    public boolean dropOn(DragEvent event, RootInfo root) {
        throw new UnsupportedOperationException("Can't open an app.");
    }

    @Override
    public void pasteIntoFolder(RootInfo root) {
        throw new UnsupportedOperationException("Can't paste into folder.");
    }

    @Override
    public void viewInOwner() {
        throw new UnsupportedOperationException("Can't view in application.");
    }

    @Override
    public void selectAllFiles() {
        Metrics.logUserAction(MetricConsts.USER_ACTION_SELECT_ALL);
        Model model = mInjector.getModel();

        // Exclude disabled files
        List<String> enabled = new ArrayList<>();
        for (String id : model.getModelIds()) {
            Cursor cursor = model.getItem(id);
            if (cursor == null) {
                Log.w(TAG, "Skipping selection. Can't obtain cursor for modeId: " + id);
                continue;
            }
            String docMimeType = getCursorString(
                    cursor, DocumentsContract.Document.COLUMN_MIME_TYPE);
            int docFlags = getCursorInt(cursor, DocumentsContract.Document.COLUMN_FLAGS);
            if (mInjector.config.isDocumentEnabled(docMimeType, docFlags, mState)) {
                enabled.add(id);
            }
        }

        // Only select things currently visible in the adapter.
        boolean changed = mSelectionMgr.setItemsSelected(enabled, true);
        if (changed) {
            mDisplayStateChangedListener.run();
        }
    }

    @Override
    public void deselectAllFiles() {
        mSelectionMgr.clearSelection();
    }

    @Override
    public void showCreateDirectoryDialog() {
        Metrics.logUserAction(MetricConsts.USER_ACTION_CREATE_DIR);

        CreateDirectoryFragment.show(mActivity.getSupportFragmentManager());
    }

    @Override
    public void showSortDialog() {
        SortListFragment.show(mActivity.getSupportFragmentManager(), mState.sortModel);
    }

    @Override
    @Nullable
    public DocumentInfo renameDocument(String name, DocumentInfo document) {
        throw new UnsupportedOperationException("Can't rename documents.");
    }

    @Override
    public void showChooserForDoc(DocumentInfo doc) {
        throw new UnsupportedOperationException("Show chooser for doc not supported!");
    }

    @Override
    public void openRootDocument(@Nullable DocumentInfo rootDoc) {
        if (rootDoc == null) {
            // There are 2 cases where rootDoc is null -- 1) loading recents; 2) failed to load root
            // document. Either case we should call refreshCurrentRootAndDirectory() to let
            // DirectoryFragment update UI.
            mActivity.refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);
        } else {
            openContainerDocument(rootDoc);
        }
    }

    @Override
    public void openContainerDocument(DocumentInfo doc) {
        assert(doc.isContainer());

        if (mSearchMgr.isSearching()) {
            loadDocument(
                    doc.derivedUri,
                    doc.userId,
                    (@Nullable DocumentStack stack) -> openFolderInSearchResult(stack, doc));
        } else {
            openChildContainer(doc);
        }
    }

    // TODO: Make this private and make tests call interface method instead.
    /**
     * Behavior when a document is opened.
     */
    @VisibleForTesting
    public void onDocumentOpened(DocumentInfo doc, @ViewType int type, @ViewType int fallback,
            boolean fromPicker) {
        // In picker mode, don't access archive container to avoid pick file in archive files.
        if (doc.isContainer() && !fromPicker) {
            openContainerDocument(doc);
            return;
        }

        if (manageDocument(doc)) {
            return;
        }

        // For APKs, even if the type is preview, we send an ACTION_VIEW intent to allow
        // PackageManager to install it.  This allows users to install APKs from any root.
        // The Downloads special case is handled above in #manageDocument.
        if (MimeTypes.isApkType(doc.mimeType)) {
            viewDocument(doc);
            return;
        }

        switch (type) {
            case VIEW_TYPE_REGULAR:
                if (viewDocument(doc)) {
                    return;
                }
                break;

            case VIEW_TYPE_PREVIEW:
                if (previewDocument(doc, fromPicker)) {
                    return;
                }
                break;

            default:
                throw new IllegalArgumentException("Illegal view type.");
        }

        switch (fallback) {
            case VIEW_TYPE_REGULAR:
                if (viewDocument(doc)) {
                    return;
                }
                break;

            case VIEW_TYPE_PREVIEW:
                if (previewDocument(doc, fromPicker)) {
                    return;
                }
                break;

            case VIEW_TYPE_NONE:
                break;

            default:
                throw new IllegalArgumentException("Illegal fallback view type.");
        }

        // Failed to view including fallback, and it's in an archive.
        if (type != VIEW_TYPE_NONE && fallback != VIEW_TYPE_NONE && doc.isInArchive()) {
            mDialogs.showViewInArchivesUnsupported();
        }
    }

    private boolean viewDocument(DocumentInfo doc) {
        if (doc.isPartial()) {
            Log.w(TAG, "Can't view partial file.");
            return false;
        }

        if (doc.isInArchive()) {
            Log.w(TAG, "Can't view files in archives.");
            return false;
        }

        if (doc.isDirectory()) {
            Log.w(TAG, "Can't view directories.");
            return true;
        }

        Intent intent = buildViewIntent(doc);
        if (DEBUG && intent.getClipData() != null) {
            Log.d(TAG, "Starting intent w/ clip data: " + intent.getClipData());
        }

        try {
            doc.userId.startActivityAsUser(mActivity, intent);
            return true;
        } catch (ActivityNotFoundException e) {
            mDialogs.showNoApplicationFound();
        }
        return false;
    }

    private boolean previewDocument(DocumentInfo doc, boolean fromPicker) {
        if (doc.isPartial()) {
            Log.w(TAG, "Can't view partial file.");
            return false;
        }

        Intent intent = new QuickViewIntentBuilder(
                mActivity,
                mActivity.getResources(),
                doc,
                mModel,
                fromPicker).build();

        if (intent != null) {
            // TODO: un-work around issue b/24963914. Should be fixed soon.
            try {
                doc.userId.startActivityAsUser(mActivity, intent);
                return true;
            } catch (SecurityException e) {
                // Carry on to regular view mode.
                Log.e(TAG, "Caught security error: " + e.getLocalizedMessage());
            }
        }

        return false;
    }


    protected boolean manageDocument(DocumentInfo doc) {
        if (isManagedDownload(doc)) {
            // First try managing the document; we expect manager to filter
            // based on authority, so we don't grant.
            Intent manage = new Intent(DocumentsContract.ACTION_MANAGE_DOCUMENT);
            manage.setData(doc.getDocumentUri());
            try {
                doc.userId.startActivityAsUser(mActivity, manage);
                return true;
            } catch (ActivityNotFoundException ex) {
                // Fall back to regular handling.
            }
        }

        return false;
    }

    private boolean isManagedDownload(DocumentInfo doc) {
        // Anything on downloads goes through the back through downloads manager
        // (that's the MANAGE_DOCUMENT bit).
        // This is done for two reasons:
        // 1) The file in question might be a failed/queued or otherwise have some
        //    specialized download handling.
        // 2) For APKs, the download manager will add on some important security stuff
        //    like origin URL.
        // 3) For partial files, the download manager will offer to restart/retry downloads.

        // All other files not on downloads, event APKs, would get no benefit from this
        // treatment, thusly the "isDownloads" check.

        // Launch MANAGE_DOCUMENTS only for the root level files, so it's not called for
        // files in archives or in child folders. Also, if the activity is already browsing
        // a ZIP from downloads, then skip MANAGE_DOCUMENTS.
        if (Intent.ACTION_VIEW.equals(mActivity.getIntent().getAction())
                && mState.stack.size() > 1) {
            // viewing the contents of an archive.
            return false;
        }

        // management is only supported in Downloads root or downloaded files show in Recent root.
        if (Providers.AUTHORITY_DOWNLOADS.equals(doc.authority)) {
            // only on APKs or partial files.
            return MimeTypes.isApkType(doc.mimeType) || doc.isPartial();
        }

        return false;
    }

    protected Intent buildViewIntent(DocumentInfo doc) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(doc.getDocumentUri(), doc.mimeType);

        // Downloads has traditionally added the WRITE permission
        // in the TrampolineActivity. Since this behavior is long
        // established, we set the same permission for non-managed files
        // This ensures consistent behavior between the Downloads root
        // and other roots.
        int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_SINGLE_TOP;
        if (doc.isWriteSupported()) {
            flags |= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        }
        intent.setFlags(flags);

        return intent;
    }

    @Override
    public boolean previewItem(ItemDetails<String> doc) {
        throw new UnsupportedOperationException("Can't handle preview.");
    }

    private void openFolderInSearchResult(@Nullable DocumentStack stack, DocumentInfo doc) {
        if (stack == null) {
            mState.stack.popToRootDocument();

            // Update navigator to give horizontal breadcrumb a chance to update documents. It
            // doesn't update its content if the size of document stack doesn't change.
            // TODO: update breadcrumb to take range update.
            mActivity.updateNavigator();

            mState.stack.push(doc);
        } else {
            if (!Objects.equals(mState.stack.getRoot(), stack.getRoot())) {
                // It is now possible when opening cross-profile folder.
                Log.w(TAG, "Provider returns " + stack.getRoot() + " rather than expected "
                        + mState.stack.getRoot());
            }

            final DocumentInfo top = stack.peek();
            if (top.isArchive()) {
                // Swap the zip file in original provider and the one provided by ArchiveProvider.
                stack.pop();
                stack.push(mDocs.getArchiveDocument(top.derivedUri, top.userId));
            }

            mState.stack.reset();
            // Update navigator to give horizontal breadcrumb a chance to update documents. It
            // doesn't update its content if the size of document stack doesn't change.
            // TODO: update breadcrumb to take range update.
            mActivity.updateNavigator();

            mState.stack.reset(stack);
        }

        // Show an opening animation only if pressing "back" would get us back to the
        // previous directory. Especially after opening a root document, pressing
        // back, wouldn't go to the previous root, but close the activity.
        final int anim = (mState.stack.hasLocationChanged() && mState.stack.size() > 1)
                ? AnimationView.ANIM_ENTER : AnimationView.ANIM_NONE;
        mActivity.refreshCurrentRootAndDirectory(anim);
    }

    private void openChildContainer(DocumentInfo doc) {
        DocumentInfo currentDoc = null;

        if (doc.isDirectory()) {
            // Regular directory.
            currentDoc = doc;
        } else if (doc.isArchive()) {
            // Archive.
            currentDoc = mDocs.getArchiveDocument(doc.derivedUri, doc.userId);
        }

        assert(currentDoc != null);
        if (currentDoc.equals(mState.stack.peek())) {
            Log.w(TAG, "This DocumentInfo is already in current DocumentsStack");
            return;
        }

        mActivity.notifyDirectoryNavigated(currentDoc.derivedUri);

        mState.stack.push(currentDoc);
        // Show an opening animation only if pressing "back" would get us back to the
        // previous directory. Especially after opening a root document, pressing
        // back, wouldn't go to the previous root, but close the activity.
        final int anim = (mState.stack.hasLocationChanged() && mState.stack.size() > 1)
                ? AnimationView.ANIM_ENTER : AnimationView.ANIM_NONE;
        mActivity.refreshCurrentRootAndDirectory(anim);
    }

    @Override
    public void setDebugMode(boolean enabled) {
        if (!mInjector.features.isDebugSupportEnabled()) {
            return;
        }

        mState.debugMode = enabled;
        mInjector.features.forceFeature(R.bool.feature_command_interceptor, enabled);
        mInjector.features.forceFeature(R.bool.feature_inspector, enabled);
        mActivity.invalidateOptionsMenu();

        if (enabled) {
            showDebugMessage();
        } else {
            mActivity.getWindow().setStatusBarColor(
                    mActivity.getResources().getColor(R.color.app_background_color));
        }
    }

    @Override
    public void showDebugMessage() {
        assert (mInjector.features.isDebugSupportEnabled());

        int[] colors = mInjector.debugHelper.getNextColors();
        Pair<String, Integer> messagePair = mInjector.debugHelper.getNextMessage();

        Snackbars.showCustomTextWithImage(mActivity, messagePair.first, messagePair.second);

        mActivity.getWindow().setStatusBarColor(colors[1]);
    }

    @Override
    public void switchLauncherIcon() {
        PackageManager pm = mActivity.getPackageManager();
        if (pm != null) {
            final boolean enalbled = Shared.isLauncherEnabled(mActivity);
            ComponentName component = new ComponentName(
                    mActivity.getPackageName(), Shared.LAUNCHER_TARGET_CLASS);
            pm.setComponentEnabledSetting(component, enalbled
                    ? PackageManager.COMPONENT_ENABLED_STATE_DISABLED
                    : PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                    PackageManager.DONT_KILL_APP);
        }
    }

    @Override
    public void cutToClipboard() {
        throw new UnsupportedOperationException("Cut not supported!");
    }

    @Override
    public void copyToClipboard() {
        throw new UnsupportedOperationException("Copy not supported!");
    }

    @Override
    public void showDeleteDialog() {
        throw new UnsupportedOperationException("Delete not supported!");
    }

    @Override
    public void deleteSelectedDocuments(List<DocumentInfo> docs, DocumentInfo srcParent) {
        throw new UnsupportedOperationException("Delete not supported!");
    }

    @Override
    public void shareSelectedDocuments() {
        throw new UnsupportedOperationException("Share not supported!");
    }

    protected final void loadDocument(Uri uri, UserId userId, LoadDocStackCallback callback) {
        new LoadDocStackTask(
                mActivity,
                mProviders,
                mDocs,
                userId,
                callback
                ).executeOnExecutor(mExecutors.lookup(uri.getAuthority()), uri);
    }

    @Override
    public final void loadRoot(Uri uri, UserId userId) {
        new LoadRootTask<>(mActivity, mProviders, uri, userId, this::onRootLoaded)
                .executeOnExecutor(mExecutors.lookup(uri.getAuthority()));
    }

    @Override
    public final void loadCrossProfileRoot(RootInfo info, UserId selectedUser) {
        if (info.isRecents()) {
            openRoot(mProviders.getRecentsRoot(selectedUser));
            return;
        }
        new LoadRootTask<>(mActivity, mProviders, info.getUri(), selectedUser,
                new LoadCrossProfileRootCallback(info, selectedUser))
                .executeOnExecutor(mExecutors.lookup(info.getUri().getAuthority()));
    }

    private class LoadCrossProfileRootCallback implements LoadRootTask.LoadRootCallback {
        private final RootInfo mOriginalRoot;
        private final UserId mSelectedUserId;

        LoadCrossProfileRootCallback(RootInfo rootInfo, UserId selectedUser) {
            mOriginalRoot = rootInfo;
            mSelectedUserId = selectedUser;
        }

        @Override
        public void onRootLoaded(@Nullable RootInfo root) {
            if (root == null) {
                // There is no such root in the other profile. Maybe the provider is missing on
                // the other profile. Create a dummy root and open it to show error message.
                root = RootInfo.copyRootInfo(mOriginalRoot);
                root.userId = mSelectedUserId;
            }
            openRoot(root);
        }
    }

    @Override
    public final void loadFirstRoot(Uri uri) {
        new LoadFirstRootTask<>(mActivity, mProviders, uri, this::onRootLoaded)
                .executeOnExecutor(mExecutors.lookup(uri.getAuthority()));
    }

    @Override
    public void loadDocumentsForCurrentStack() {
        // mState.stack may be empty when we cannot load the root document.
        // However, we still want to restart loader because we may need to perform search in a
        // cross-profile scenario.
        // For RecentsLoader and GlobalSearchLoader, they do not require rootDoc so it is no-op.
        // For DirectoryLoader, the loader needs to handle the case when stack.peek() returns null.

        mActivity.getSupportLoaderManager().restartLoader(LOADER_ID, null, mBindings);
    }

    protected final boolean launchToDocument(Uri uri) {
        // We don't support launching to a document in an archive.
        if (!Providers.isArchiveUri(uri)) {
            loadDocument(uri, UserId.DEFAULT_USER, this::onStackLoaded);
            return true;
        }

        return false;
    }

    private void onStackLoaded(@Nullable DocumentStack stack) {
        if (stack != null) {
            if (!stack.peek().isDirectory()) {
                // Requested document is not a directory. Pop it so that we can launch into its
                // parent.
                stack.pop();
            }
            mState.stack.reset(stack);
            mActivity.refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);

            Metrics.logLaunchAtLocation(mState, stack.getRoot().getUri());
        } else {
            Log.w(TAG, "Failed to launch into the given uri. Launch to default location.");
            launchToDefaultLocation();

            Metrics.logLaunchAtLocation(mState, null);
        }
    }

    private void onRootLoaded(@Nullable RootInfo root) {
        boolean invalidRootForAction =
                (root != null
                && !root.supportsChildren()
                && mState.action == State.ACTION_OPEN_TREE);

        if (invalidRootForAction) {
            loadDeviceRoot();
        } else if (root != null) {
            mActivity.onRootPicked(root);
        } else {
            launchToDefaultLocation();
        }
    }

    protected abstract void launchToDefaultLocation();

    protected void restoreRootAndDirectory() {
        if (!mState.stack.getRoot().isRecents() && mState.stack.isEmpty()) {
            mActivity.onRootPicked(mState.stack.getRoot());
        } else {
            mActivity.restoreRootAndDirectory();
        }
    }

    protected final void loadDeviceRoot() {
        loadRoot(DocumentsContract.buildRootUri(Providers.AUTHORITY_STORAGE,
                Providers.ROOT_ID_DEVICE), UserId.DEFAULT_USER);
    }

    protected final void loadHomeDir() {
        loadRoot(Shared.getDefaultRootUri(mActivity), UserId.DEFAULT_USER);
    }

    protected final void loadRecent() {
        mState.stack.changeRoot(mProviders.getRecentsRoot(UserId.DEFAULT_USER));
        mActivity.refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);
    }

    protected MutableSelection<String> getStableSelection() {
        MutableSelection<String> selection = new MutableSelection<>();
        mSelectionMgr.copySelection(selection);
        return selection;
    }

    @Override
    public ActionHandler reset(ContentLock reloadLock) {
        mContentLock = reloadLock;
        mActivity.getLoaderManager().destroyLoader(LOADER_ID);
        return this;
    }

    private final class LoaderBindings implements LoaderCallbacks<DirectoryResult> {

        @Override
        public Loader<DirectoryResult> onCreateLoader(int id, Bundle args) {
            Context context = mActivity;

            if (mState.stack.isRecents()) {
                final LockingContentObserver observer = new LockingContentObserver(
                        mContentLock, AbstractActionHandler.this::loadDocumentsForCurrentStack);
                MultiRootDocumentsLoader loader;

                if (mSearchMgr.isSearching()) {
                    if (DEBUG) {
                        Log.d(TAG, "Creating new GlobalSearchLoader.");
                    }
                    loader = new GlobalSearchLoader(
                            context,
                            mProviders,
                            mState,
                            mExecutors,
                            mInjector.fileTypeLookup,
                            mSearchMgr.buildQueryArgs(),
                            mState.stack.getRoot().userId);
                } else {
                    if (DEBUG) {
                        Log.d(TAG, "Creating new loader recents.");
                    }
                    loader = new RecentsLoader(
                            context,
                            mProviders,
                            mState,
                            mExecutors,
                            mInjector.fileTypeLookup,
                            mState.stack.getRoot().userId);
                }
                loader.setObserver(observer);
                return loader;
            } else {
                // There maybe no root docInfo
                DocumentInfo rootDoc = mState.stack.peek();

                String authority = rootDoc == null
                        ? mState.stack.getRoot().authority
                        : rootDoc.authority;
                String documentId = rootDoc == null
                        ? mState.stack.getRoot().documentId
                        : rootDoc.documentId;

                Uri contentsUri = mSearchMgr.isSearching()
                        ? DocumentsContract.buildSearchDocumentsUri(
                        mState.stack.getRoot().authority,
                        mState.stack.getRoot().rootId,
                        mSearchMgr.getCurrentSearch())
                        : DocumentsContract.buildChildDocumentsUri(
                                authority,
                                documentId);

                final Bundle queryArgs = mSearchMgr.isSearching()
                        ? mSearchMgr.buildQueryArgs()
                        : null;

                if (mInjector.config.managedModeEnabled(mState.stack)) {
                    contentsUri = DocumentsContract.setManageMode(contentsUri);
                }

                if (DEBUG) {
                    Log.d(TAG,
                        "Creating new directory loader for: "
                            + DocumentInfo.debugString(mState.stack.peek()));
                }

                return new DirectoryLoader(
                        mInjector.features,
                        context,
                        mState,
                        contentsUri,
                        mInjector.fileTypeLookup,
                        mContentLock,
                        queryArgs);
            }
        }

        @Override
        public void onLoadFinished(Loader<DirectoryResult> loader, DirectoryResult result) {
            if (DEBUG) {
                Log.d(TAG, "Loader has finished for: "
                    + DocumentInfo.debugString(mState.stack.peek()));
            }
            assert(result != null);

            mInjector.getModel().update(result);
        }

        @Override
        public void onLoaderReset(Loader<DirectoryResult> loader) {}
    }
    /**
     * A class primarily for the support of isolating our tests
     * from our concrete activity implementations.
     */
    public interface CommonAddons {
        void restoreRootAndDirectory();
        void refreshCurrentRootAndDirectory(@AnimationType int anim);
        void onRootPicked(RootInfo root);
        // TODO: Move this to PickAddons as multi-document picking is exclusive to that activity.
        void onDocumentsPicked(List<DocumentInfo> docs);
        void onDocumentPicked(DocumentInfo doc);
        RootInfo getCurrentRoot();
        DocumentInfo getCurrentDirectory();
        UserId getSelectedUser();
        /**
         * Check whether current directory is root of recent.
         */
        boolean isInRecents();
        void setRootsDrawerOpen(boolean open);

        /**
         * Set the locked status of the DrawerController.
         */
        void setRootsDrawerLocked(boolean locked);

        // TODO: Let navigator listens to State
        void updateNavigator();

        @VisibleForTesting
        void notifyDirectoryNavigated(Uri docUri);
    }
}
