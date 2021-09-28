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

package com.android.documentsui.files;

import static android.content.ContentResolver.wrap;

import static com.android.documentsui.base.SharedMinimal.DEBUG;

import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.FileUtils;
import android.provider.DocumentsContract;
import android.text.TextUtils;
import android.util.Log;
import android.view.DragEvent;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;
import androidx.recyclerview.selection.ItemDetailsLookup.ItemDetails;
import androidx.recyclerview.selection.MutableSelection;
import androidx.recyclerview.selection.Selection;

import com.android.documentsui.AbstractActionHandler;
import com.android.documentsui.ActionModeAddons;
import com.android.documentsui.ActivityConfig;
import com.android.documentsui.DocumentsAccess;
import com.android.documentsui.DocumentsApplication;
import com.android.documentsui.DragAndDropManager;
import com.android.documentsui.Injector;
import com.android.documentsui.MetricConsts;
import com.android.documentsui.Metrics;
import com.android.documentsui.R;
import com.android.documentsui.TimeoutTask;
import com.android.documentsui.base.DebugFlags;
import com.android.documentsui.base.DocumentFilters;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.DocumentStack;
import com.android.documentsui.base.Features;
import com.android.documentsui.base.Lookup;
import com.android.documentsui.base.MimeTypes;
import com.android.documentsui.base.Providers;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.Shared;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;
import com.android.documentsui.clipping.ClipStore;
import com.android.documentsui.clipping.DocumentClipper;
import com.android.documentsui.clipping.UrisSupplier;
import com.android.documentsui.dirlist.AnimationView;
import com.android.documentsui.inspector.InspectorActivity;
import com.android.documentsui.queries.SearchViewManager;
import com.android.documentsui.roots.ProvidersAccess;
import com.android.documentsui.services.FileOperation;
import com.android.documentsui.services.FileOperationService;
import com.android.documentsui.services.FileOperations;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

import javax.annotation.Nullable;

/**
 * Provides {@link FilesActivity} action specializations to fragments.
 * @param <T> activity which extends {@link FragmentActivity} and implements
 *              {@link AbstractActionHandler.CommonAddons}.
 */
public class ActionHandler<T extends FragmentActivity & AbstractActionHandler.CommonAddons>
        extends AbstractActionHandler<T> {

    private static final String TAG = "ManagerActionHandler";
    private static final int SHARE_FILES_COUNT_LIMIT = 100;

    private final ActionModeAddons mActionModeAddons;
    private final Features mFeatures;
    private final ActivityConfig mConfig;
    private final DocumentClipper mClipper;
    private final ClipStore mClipStore;
    private final DragAndDropManager mDragAndDropManager;

    ActionHandler(
            T activity,
            State state,
            ProvidersAccess providers,
            DocumentsAccess docs,
            SearchViewManager searchMgr,
            Lookup<String, Executor> executors,
            ActionModeAddons actionModeAddons,
            DocumentClipper clipper,
            ClipStore clipStore,
            DragAndDropManager dragAndDropManager,
            Injector injector) {

        super(activity, state, providers, docs, searchMgr, executors, injector);

        mActionModeAddons = actionModeAddons;
        mFeatures = injector.features;
        mConfig = injector.config;
        mClipper = clipper;
        mClipStore = clipStore;
        mDragAndDropManager = dragAndDropManager;
    }

    @Override
    public boolean dropOn(DragEvent event, RootInfo root) {
        if (!root.supportsCreate() || root.isLibrary()) {
            return false;
        }

        // DragEvent gets recycled, so it is possible that by the time the callback is called,
        // event.getLocalState() and event.getClipData() returns null. Thus, we want to save
        // references to ensure they are non null.
        final ClipData clipData = event.getClipData();
        final Object localState = event.getLocalState();

        return mDragAndDropManager.drop(
                clipData, localState, root, this, mDialogs::showFileOperationStatus);
    }

    @Override
    public void openSelectedInNewWindow() {
        Selection<String> selection = getStableSelection();
        if (selection.isEmpty()) {
            return;
        }

        assert(selection.size() == 1);
        DocumentInfo doc = mModel.getDocument(selection.iterator().next());
        assert(doc != null);
        openInNewWindow(new DocumentStack(mState.stack, doc));
    }

    @Override
    public void openSettings(RootInfo root) {
        Metrics.logUserAction(MetricConsts.USER_ACTION_SETTINGS);
        final Intent intent = new Intent(DocumentsContract.ACTION_DOCUMENT_ROOT_SETTINGS);
        intent.setDataAndType(root.getUri(), DocumentsContract.Root.MIME_TYPE_ITEM);
        root.userId.startActivityAsUser(mActivity, intent);
    }

    @Override
    public void pasteIntoFolder(RootInfo root) {
        this.getRootDocument(
                root,
                TimeoutTask.DEFAULT_TIMEOUT,
                (DocumentInfo doc) -> pasteIntoFolder(root, doc));
    }

    private void pasteIntoFolder(RootInfo root, @Nullable DocumentInfo doc) {
        DocumentStack stack = new DocumentStack(root, doc);
        mClipper.copyFromClipboard(doc, stack, mDialogs::showFileOperationStatus);
    }

    @Override
    public @Nullable DocumentInfo renameDocument(String name, DocumentInfo document) {
        ContentResolver resolver = document.userId.getContentResolver(mActivity);
        ContentProviderClient client = null;

        try {
            client = DocumentsApplication.acquireUnstableProviderOrThrow(
                    resolver, document.derivedUri.getAuthority());
            Uri newUri = DocumentsContract.renameDocument(
                    wrap(client), document.derivedUri, name);
            return DocumentInfo.fromUri(resolver, newUri, document.userId);
        } catch (Exception e) {
            Log.w(TAG, "Failed to rename file", e);
            return null;
        } finally {
            FileUtils.closeQuietly(client);
        }
    }

    @Override
    public void openRoot(RootInfo root) {
        Metrics.logRootVisited(MetricConsts.FILES_SCOPE, root);
        mActivity.onRootPicked(root);
    }

    @Override
    public boolean openItem(ItemDetails<String> details, @ViewType int type,
            @ViewType int fallback) {
        DocumentInfo doc = mModel.getDocument(details.getSelectionKey());
        if (doc == null) {
            Log.w(TAG, "Can't view item. No Document available for modeId: "
                    + details.getSelectionKey());
            return false;
        }
        mInjector.searchManager.recordHistory();

        return openDocument(doc, type, fallback);
    }

    // TODO: Make this private and make tests call openDocument(DocumentDetails, int, int) instead.
    @VisibleForTesting
    public boolean openDocument(DocumentInfo doc, @ViewType int type, @ViewType int fallback) {
        if (mConfig.isDocumentEnabled(doc.mimeType, doc.flags, mState)) {
            onDocumentOpened(doc, type, fallback, false);
            mSelectionMgr.clearSelection();
            return !doc.isContainer();
        }
        return false;
    }

    @Override
    public void springOpenDirectory(DocumentInfo doc) {
        assert(doc.isDirectory());
        mActionModeAddons.finishActionMode();
        openContainerDocument(doc);
    }

    private Selection<String> getSelectedOrFocused() {
        final MutableSelection<String> selection = this.getStableSelection();
        if (selection.isEmpty()) {
            String focusModelId = mFocusHandler.getFocusModelId();
            if (focusModelId != null) {
                selection.add(focusModelId);
            }
        }

        return selection;
    }

    @Override
    public void cutToClipboard() {
        Metrics.logUserAction(MetricConsts.USER_ACTION_CUT_CLIPBOARD);
        Selection<String> selection = getSelectedOrFocused();

        if (selection.isEmpty()) {
            return;
        }

        if (mModel.hasDocuments(selection, DocumentFilters.NOT_MOVABLE)) {
            mDialogs.showOperationUnsupported();
            return;
        }

        mSelectionMgr.clearSelection();

        mClipper.clipDocumentsForCut(mModel::getItemUri, selection, mState.stack.peek());

        mDialogs.showDocumentsClipped(selection.size());
    }

    @Override
    public void copyToClipboard() {
        Metrics.logUserAction(MetricConsts.USER_ACTION_COPY_CLIPBOARD);
        Selection<String> selection = getSelectedOrFocused();

        if (selection.isEmpty()) {
            return;
        }
        mSelectionMgr.clearSelection();

        mClipper.clipDocumentsForCopy(mModel::getItemUri, selection);

        mDialogs.showDocumentsClipped(selection.size());
    }

    @Override
    public void viewInOwner() {
        Metrics.logUserAction(MetricConsts.USER_ACTION_VIEW_IN_APPLICATION);
        Selection<String> selection = getSelectedOrFocused();

        if (selection.isEmpty() || selection.size() > 1) {
            return;
        }
        DocumentInfo doc = mModel.getDocument(selection.iterator().next());
        Intent intent = new Intent(DocumentsContract.ACTION_DOCUMENT_SETTINGS);
        intent.setPackage(mProviders.getPackageName(UserId.DEFAULT_USER, doc.authority));
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setData(doc.derivedUri);
        try {
            doc.userId.startActivityAsUser(mActivity, intent);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Failed to view settings in application for " + doc.derivedUri, e);
            mDialogs.showNoApplicationFound();
        }
    }

    @Override
    public void showDeleteDialog() {
        Selection selection = getSelectedOrFocused();
        if (selection.isEmpty()) {
            return;
        }

        DeleteDocumentFragment.show(mActivity.getSupportFragmentManager(),
                mModel.getDocuments(selection),
                mState.stack.peek());
    }


    @Override
    public void deleteSelectedDocuments(List<DocumentInfo> docs, DocumentInfo srcParent) {
        if (docs == null || docs.isEmpty()) {
            return;
        }

        mActionModeAddons.finishActionMode();

        List<Uri> uris = new ArrayList<>(docs.size());
        for (DocumentInfo doc : docs) {
            uris.add(doc.derivedUri);
        }

        UrisSupplier srcs;
        try {
            srcs = UrisSupplier.create(
                    uris,
                    mClipStore);
        } catch (Exception e) {
            Log.e(TAG, "Failed to delete a file because we were unable to get item URIs.", e);
            mDialogs.showFileOperationStatus(
                    FileOperations.Callback.STATUS_FAILED,
                    FileOperationService.OPERATION_DELETE,
                    uris.size());
            return;
        }

        FileOperation operation = new FileOperation.Builder()
                .withOpType(FileOperationService.OPERATION_DELETE)
                .withDestination(mState.stack)
                .withSrcs(srcs)
                .withSrcParent(srcParent == null ? null : srcParent.derivedUri)
                .build();

        FileOperations.start(mActivity, operation, mDialogs::showFileOperationStatus,
                FileOperations.createJobId());
    }

    @Override
    public void shareSelectedDocuments() {
        Metrics.logUserAction(MetricConsts.USER_ACTION_SHARE);

        Selection<String> selection = getStableSelection();
        if (selection.isEmpty()) {
            return;
        } else if (selection.size() > SHARE_FILES_COUNT_LIMIT) {
            mDialogs.showShareOverLimit(SHARE_FILES_COUNT_LIMIT);
            return;
        }

        // Model must be accessed in UI thread, since underlying cursor is not threadsafe.
        List<DocumentInfo> docs = mModel.loadDocuments(
                selection, DocumentFilters.sharable(mFeatures));

        Intent intent;

        if (docs.size() == 1) {
            intent = new Intent(Intent.ACTION_SEND);
            DocumentInfo doc = docs.get(0);
            intent.setType(doc.mimeType);
            intent.putExtra(Intent.EXTRA_STREAM, doc.getDocumentUri());

        } else if (docs.size() > 1) {
            intent = new Intent(Intent.ACTION_SEND_MULTIPLE);

            final ArrayList<String> mimeTypes = new ArrayList<>();
            final ArrayList<Uri> uris = new ArrayList<>();
            for (DocumentInfo doc : docs) {
                mimeTypes.add(doc.mimeType);
                uris.add(doc.getDocumentUri());
            }

            intent.setType(MimeTypes.findCommonMimeType(mimeTypes));
            intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);

        } else {
            // Everything filtered out, nothing to share.
            return;
        }

        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.addCategory(Intent.CATEGORY_DEFAULT);

        if (mFeatures.isVirtualFilesSharingEnabled()
                && mModel.hasDocuments(selection, DocumentFilters.VIRTUAL)) {
            intent.addCategory(Intent.CATEGORY_TYPED_OPENABLE);
        }

        Intent chooserIntent = Intent.createChooser(
                intent, mActivity.getResources().getText(R.string.share_via));

        mActivity.startActivity(chooserIntent);
    }

    @Override
    public void loadDocumentsForCurrentStack() {
        super.loadDocumentsForCurrentStack();
    }

    @Override
    public void initLocation(Intent intent) {
        assert(intent != null);

        // stack is initialized if it's restored from bundle, which means we're restoring a
        // previously stored state.
        if (mState.stack.isInitialized()) {
            if (DEBUG) {
                Log.d(TAG, "Stack already resolved for uri: " + intent.getData());
            }
            restoreRootAndDirectory();
            return;
        }

        if (launchToStackLocation(intent)) {
            if (DEBUG) {
                Log.d(TAG, "Launched to location from stack.");
            }
            return;
        }

        if (launchToRoot(intent)) {
            if (DEBUG) {
                Log.d(TAG, "Launched to root for browsing.");
            }
            return;
        }

        if (launchToDocument(intent)) {
            if (DEBUG) {
                Log.d(TAG, "Launched to a document.");
            }
            return;
        }

        if (launchToDownloads(intent)) {
            if (DEBUG) {
                Log.d(TAG, "Launched to a downloads.");
            }
            return;
        }

        if (DEBUG) {
            Log.d(TAG, "Launching directly into Home directory.");
        }
        launchToDefaultLocation();
    }

    @Override
    protected void launchToDefaultLocation() {
        loadHomeDir();
    }

    // If EXTRA_STACK is not null in intent, we'll skip other means of loading
    // or restoring the stack (like URI).
    //
    // When restoring from a stack, if a URI is present, it should only ever be:
    // -- a launch URI: Launch URIs support sensible activity management,
    //    but don't specify a real content target)
    // -- a fake Uri from notifications. These URIs have no authority (TODO: details).
    //
    // Any other URI is *sorta* unexpected...except when browsing an archive
    // in downloads.
    private boolean launchToStackLocation(Intent intent) {
        DocumentStack stack = intent.getParcelableExtra(Shared.EXTRA_STACK);
        if (stack == null || stack.getRoot() == null) {
            return false;
        }

        mState.stack.reset(stack);
        if (mState.stack.isEmpty()) {
            mActivity.onRootPicked(mState.stack.getRoot());
        } else {
            mActivity.refreshCurrentRootAndDirectory(AnimationView.ANIM_NONE);
        }

        return true;
    }

    private boolean launchToRoot(Intent intent) {
        String action = intent.getAction();
        if (Intent.ACTION_VIEW.equals(action)) {
            Uri uri = intent.getData();
            if (DocumentsContract.isRootUri(mActivity, uri)) {
                if (DEBUG) {
                    Log.d(TAG, "Launching with root URI.");
                }
                // If we've got a specific root to display, restore that root using a dedicated
                // authority. That way a misbehaving provider won't result in an ANR.
                loadRoot(uri, UserId.DEFAULT_USER);
                return true;
            } else if (DocumentsContract.isRootsUri(mActivity, uri)) {
                if (DEBUG) {
                    Log.d(TAG, "Launching first root with roots URI.");
                }
                // TODO: b/116760996 Let the user can disambiguate between roots if there are
                // multiple from DocumentsProvider instead of launching the first root in default
                loadFirstRoot(uri);
                return true;
            }
        }
        return false;
    }

    private boolean launchToDocument(Intent intent) {
        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            Uri uri = intent.getData();
            if (DocumentsContract.isDocumentUri(mActivity, uri)) {
                return launchToDocument(intent.getData());
            }
        }

        return false;
    }

    private boolean launchToDownloads(Intent intent) {
        if (DownloadManager.ACTION_VIEW_DOWNLOADS.equals(intent.getAction())) {
            Uri uri = DocumentsContract.buildRootUri(Providers.AUTHORITY_DOWNLOADS,
                    Providers.ROOT_ID_DOWNLOADS);
            loadRoot(uri, UserId.DEFAULT_USER);
            return true;
        }

        return false;
    }

    @Override
    public void showChooserForDoc(DocumentInfo doc) {
        assert(!doc.isDirectory());

        if (manageDocument(doc)) {
            Log.w(TAG, "Open with is not yet supported for managed doc.");
            return;
        }

        Intent intent = Intent.createChooser(buildViewIntent(doc), null);
        intent.putExtra(Intent.EXTRA_AUTO_LAUNCH_SINGLE_CHOICE, false);
        try {
            doc.userId.startActivityAsUser(mActivity, intent);
        } catch (ActivityNotFoundException e) {
            mDialogs.showNoApplicationFound();
        }
    }

    @Override
    public void showInspector(DocumentInfo doc) {
        Metrics.logUserAction(MetricConsts.USER_ACTION_INSPECTOR);
        Intent intent = InspectorActivity.createIntent(mActivity, doc.derivedUri, doc.userId);

        // permit the display of debug info about the file.
        intent.putExtra(
                Shared.EXTRA_SHOW_DEBUG,
                mFeatures.isDebugSupportEnabled() &&
                        (DEBUG || DebugFlags.getDocumentDetailsEnabled()));

        // The "root document" (top level folder in a root) don't usually have a
        // human friendly display name. That's because we've never shown the root
        // folder's name to anyone.
        // For that reason when the doc being inspected is the root folder,
        // we override the displayName of the doc w/ the Root's name instead.
        // The Root's name is shown to the user in the sidebar.
        if (doc.isDirectory() && mState.stack.size() == 1 && mState.stack.get(0).equals(doc)) {
            RootInfo root = mActivity.getCurrentRoot();
            // Recents root title isn't defined, but inspector is disabled for recents root folder.
            assert !TextUtils.isEmpty(root.title);
            intent.putExtra(Intent.EXTRA_TITLE, root.title);
        }
        mActivity.startActivity(intent);
    }
}
