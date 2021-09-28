/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.provider.cts;

import static android.provider.DocumentsContract.buildChildDocumentsUri;
import static android.provider.DocumentsContract.buildDocumentUri;
import static android.provider.DocumentsContract.buildDocumentUriUsingTree;
import static android.provider.DocumentsContract.buildRecentDocumentsUri;
import static android.provider.DocumentsContract.buildRootUri;
import static android.provider.DocumentsContract.buildRootsUri;
import static android.provider.DocumentsContract.buildSearchDocumentsUri;
import static android.provider.DocumentsContract.buildTreeDocumentUri;
import static android.provider.DocumentsContract.copyDocument;
import static android.provider.DocumentsContract.createDocument;
import static android.provider.DocumentsContract.createWebLinkIntent;
import static android.provider.DocumentsContract.deleteDocument;
import static android.provider.DocumentsContract.ejectRoot;
import static android.provider.DocumentsContract.findDocumentPath;
import static android.provider.DocumentsContract.getDocumentMetadata;
import static android.provider.DocumentsContract.isChildDocument;
import static android.provider.DocumentsContract.moveDocument;
import static android.provider.DocumentsContract.removeDocument;
import static android.provider.DocumentsContract.renameDocument;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.CALLS_REAL_METHODS;
import static org.mockito.Mockito.RETURNS_DEFAULTS;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;

import android.app.PendingIntent;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.UserHandle;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Path;
import android.provider.DocumentsProvider;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class DocumentsContractTest {
    private static final String AUTHORITY = "com.example";

    private static final String DOC_RED = "red";
    private static final String DOC_GREEN = "green";
    private static final String DOC_BLUE = "blue";
    private static final String DOC_RESULT = "result";

    private static final Uri URI_RED = buildDocumentUri(AUTHORITY, DOC_RED);
    private static final Uri URI_GREEN = buildDocumentUri(AUTHORITY, DOC_GREEN);
    private static final Uri URI_BLUE = buildDocumentUri(AUTHORITY, DOC_BLUE);
    private static final Uri URI_RESULT = buildDocumentUri(AUTHORITY, DOC_RESULT);

    private static final String MIME_TYPE = "application/octet-stream";
    private static final String DISPLAY_NAME = "My Test";
    private static final UserHandle TEST_USER_HANDLE = UserHandle.of(57);

    private Context mContext;
    private DocumentsProvider mProvider;
    private ContentResolver mResolver;

    @Before
    public void setUp() {
        mContext = mock(Context.class, RETURNS_DEFAULTS);
        mProvider = mock(DocumentsProvider.class, CALLS_REAL_METHODS);

        final ProviderInfo pi = new ProviderInfo();
        pi.authority = AUTHORITY;
        pi.exported = true;
        pi.grantUriPermissions = true;
        pi.readPermission = android.Manifest.permission.MANAGE_DOCUMENTS;
        pi.writePermission = android.Manifest.permission.MANAGE_DOCUMENTS;
        mProvider.attachInfo(mContext, pi);

        mResolver = ContentResolver.wrap(mProvider);
    }

    @Test
    public void testRootUri() {
        final String auth = "com.example";
        final String rootId = "rootId";
        final PackageManager pm = mock(PackageManager.class);
        final ProviderInfo providerInfo = new ProviderInfo();
        final ResolveInfo resolveInfo = new ResolveInfo();
        final List<ResolveInfo> infoList = new ArrayList<>();

        providerInfo.authority = auth;
        resolveInfo.providerInfo = providerInfo;
        infoList.add(resolveInfo);

        doReturn(pm).when(mContext).getPackageManager();
        doReturn(infoList).when(pm).queryIntentContentProviders(any(Intent.class), anyInt());

        final Uri uri = DocumentsContract.buildRootUri(auth, rootId);

        assertEquals(auth, uri.getAuthority());
        assertEquals(rootId, DocumentsContract.getRootId(uri));
        assertTrue(DocumentsContract.isRootUri(mContext, uri));
    }

    @Test
    public void testRootUri_returnFalse() {
        final String auth = "com.example";
        final String rootId = "rootId";
        final PackageManager pm = mock(PackageManager.class);
        final List<ResolveInfo> infoList = new ArrayList<>();

        doReturn(pm).when(mContext).getPackageManager();
        doReturn(infoList).when(pm).queryIntentContentProviders(any(Intent.class), anyInt());

        final Uri uri = DocumentsContract.buildRootUri(auth, rootId);

        assertEquals(auth, uri.getAuthority());
        assertEquals(rootId, DocumentsContract.getRootId(uri));
        assertFalse(DocumentsContract.isRootUri(mContext, uri));
    }

    @Test
    public void testRootsUri() {
        final String auth = "com.example";
        final PackageManager pm = mock(PackageManager.class);
        final ProviderInfo providerInfo = new ProviderInfo();
        final ResolveInfo resolveInfo = new ResolveInfo();
        final List<ResolveInfo> infoList = new ArrayList<>();

        providerInfo.authority = auth;
        resolveInfo.providerInfo = providerInfo;
        infoList.add(resolveInfo);

        doReturn(pm).when(mContext).getPackageManager();
        doReturn(infoList).when(pm).queryIntentContentProviders(any(Intent.class), anyInt());

        final Uri uri = DocumentsContract.buildRootsUri(auth);
        assertEquals(auth, uri.getAuthority());
        assertTrue(DocumentsContract.isRootsUri(mContext, uri));
    }

    @Test
    public void testRootsUri_returnsFalse() {
        final String auth = "com.example";
        final PackageManager pm = mock(PackageManager.class);
        final List<ResolveInfo> infoList = new ArrayList<>();

        doReturn(pm).when(mContext).getPackageManager();
        doReturn(infoList).when(pm).queryIntentContentProviders(any(Intent.class), anyInt());

        final Uri uri = DocumentsContract.buildRootsUri(auth);
        assertEquals(auth, uri.getAuthority());
        assertFalse(DocumentsContract.isRootsUri(mContext, uri));
    }

    @Test
    public void testDocumentUri() {
        final String auth = "com.example";
        final String docId = "doc:12";

        final Uri uri = DocumentsContract.buildDocumentUri(auth, docId);
        assertEquals(auth, uri.getAuthority());
        assertEquals(docId, DocumentsContract.getDocumentId(uri));
        assertFalse(DocumentsContract.isTreeUri(uri));
    }

    @Test
    public void testBuildDocumentUriAsUser_setsAuthority() {
        final String auth = "com.example";
        final String docId = "doc:12";

        final Uri uri = DocumentsContract.buildDocumentUriAsUser(auth, docId, TEST_USER_HANDLE);

        assertTrue(uri.getAuthority().contains(auth));
    }

    @Test
    public void testBuildDocumentUriAsUser_setsDocumentId() {
        final String auth = "com.example";
        final String docId = "doc:12";

        final Uri uri = DocumentsContract.buildDocumentUriAsUser(auth, docId, TEST_USER_HANDLE);

        assertEquals(docId, DocumentsContract.getDocumentId(uri));
    }

    @Test
    public void testBuildDocumentUriAsUser_setsUserInfo() {
        final String auth = "com.example";
        final String docId = "doc:12";

        final Uri uri = DocumentsContract.buildDocumentUriAsUser(auth, docId, TEST_USER_HANDLE);

        assertEquals(TEST_USER_HANDLE, ContentProvider.getUserHandleFromUri(uri));
    }

    @Test
    public void testTreeDocumentUri() {
        final String auth = "com.example";
        final String treeId = "doc:12";
        final String leafId = "doc:24";

        final Uri treeUri = DocumentsContract.buildTreeDocumentUri(auth, treeId);
        assertEquals(auth, treeUri.getAuthority());
        assertEquals(treeId, DocumentsContract.getTreeDocumentId(treeUri));
        assertTrue(DocumentsContract.isTreeUri(treeUri));

        final Uri leafUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, leafId);
        assertEquals(auth, leafUri.getAuthority());
        assertEquals(treeId, DocumentsContract.getTreeDocumentId(leafUri));
        assertEquals(leafId, DocumentsContract.getDocumentId(leafUri));
        assertTrue(DocumentsContract.isTreeUri(leafUri));
    }

    @Test
    public void testManageMode() {
        assertFalse(DocumentsContract.isManageMode(URI_RED));
        assertTrue(DocumentsContract.isManageMode(DocumentsContract.setManageMode(URI_RED)));
    }

    @Test
    public void testCreateDocument() throws Exception {
        doReturn(DOC_RESULT).when(mProvider).createDocument(DOC_RED, MIME_TYPE, DISPLAY_NAME);
        assertEquals(URI_RESULT, createDocument(mResolver, URI_RED, MIME_TYPE, DISPLAY_NAME));
    }

    @Test
    public void testRenameDocument() throws Exception {
        doReturn(DOC_RESULT).when(mProvider).renameDocument(DOC_RED, DISPLAY_NAME);
        assertEquals(URI_RESULT, renameDocument(mResolver, URI_RED, DISPLAY_NAME));
    }

    @Test
    public void testDeleteDocument() throws Exception {
        doNothing().when(mProvider).deleteDocument(DOC_RED);
        assertEquals(true, deleteDocument(mResolver, URI_RED));
    }

    @Test
    public void testDeleteDocument_Failure() throws Exception {
        doThrow(new RuntimeException()).when(mProvider).deleteDocument(DOC_RED);
        try {
            deleteDocument(mResolver, URI_RED);
            fail();
        } catch (RuntimeException expected) {
        }
    }

    @Test
    public void testCopyDocument() throws Exception {
        doReturn(DOC_RESULT).when(mProvider).copyDocument(DOC_RED, DOC_GREEN);
        assertEquals(URI_RESULT, copyDocument(mResolver, URI_RED, URI_GREEN));
    }

    @Test
    public void testMoveDocument() throws Exception {
        doReturn(DOC_RESULT).when(mProvider).moveDocument(DOC_RED, DOC_GREEN, DOC_BLUE);
        assertEquals(URI_RESULT, moveDocument(mResolver, URI_RED, URI_GREEN, URI_BLUE));
    }

    @Test
    public void testIsChildDocument() throws Exception {
        doReturn(true).when(mProvider).isChildDocument(DOC_RED, DOC_GREEN);
        assertEquals(true, isChildDocument(mResolver, URI_RED, URI_GREEN));
    }

    @Test
    public void testIsChildDocument_Failure() throws Exception {
        doReturn(false).when(mProvider).isChildDocument(DOC_RED, DOC_GREEN);
        assertEquals(false, isChildDocument(mResolver, URI_RED, URI_GREEN));
    }

    @Test
    public void testRemoveDocument() throws Exception {
        doNothing().when(mProvider).removeDocument(DOC_RED, DOC_GREEN);
        assertEquals(true, removeDocument(mResolver, URI_RED, URI_GREEN));
    }

    @Test
    public void testRemoveDocument_Failure() throws Exception {
        doThrow(new RuntimeException()).when(mProvider).removeDocument(DOC_RED, DOC_GREEN);
        try {
            removeDocument(mResolver, URI_RED, URI_GREEN);
            fail();
        } catch (RuntimeException expected) {
        }
    }

    @Test
    public void testEjectRoot() throws Exception {
        doNothing().when(mProvider).ejectRoot("r00t");
        ejectRoot(mResolver, buildRootUri(AUTHORITY, "r00t"));
    }

    @Test
    public void testEjectRoot_Failure() throws Exception {
        doThrow(new RuntimeException()).when(mProvider).ejectRoot("r00t");
        ejectRoot(mResolver, buildRootUri(AUTHORITY, "r00t"));
    }

    @Test
    public void testFindDocumentPath() throws Exception {
        final Path res = new Path(null, Arrays.asList("red", "blue"));

        doReturn(true).when(mProvider).isChildDocument(DOC_RED, DOC_BLUE);
        doReturn(res).when(mProvider).findDocumentPath(DOC_RED, DOC_BLUE);
        assertEquals(res, findDocumentPath(mResolver,
                buildDocumentUriUsingTree(buildTreeDocumentUri(AUTHORITY, DOC_RED), DOC_BLUE)));
    }

    @Test
    public void testCreateWebLinkIntent() throws Exception {
        final IntentSender res = PendingIntent
                .getActivity(InstrumentationRegistry.getTargetContext(), 0,
                        new Intent(Intent.ACTION_VIEW), 0)
                .getIntentSender();

        doReturn(res).when(mProvider).createWebLinkIntent(DOC_RED, Bundle.EMPTY);
        assertEquals(res, createWebLinkIntent(mResolver, URI_RED, Bundle.EMPTY));
    }

    @Test
    public void testGetDocumentMetadata() throws Exception {
        doReturn(Bundle.EMPTY).when(mProvider).getDocumentMetadata(DOC_RED);
        assertEquals(Bundle.EMPTY, getDocumentMetadata(mResolver, URI_RED));
    }

    @Test
    public void testGetDocumentStreamTypes() throws Exception {
        final String[] res = new String[] { MIME_TYPE };

        doReturn(res).when(mProvider).getDocumentStreamTypes(DOC_RED, MIME_TYPE);
        assertArrayEquals(res, mResolver.getStreamTypes(URI_RED, MIME_TYPE));
    }

    @Test
    public void testGetDocumentType() throws Exception {
        doReturn(MIME_TYPE).when(mProvider).getDocumentType(DOC_RED);
        assertEquals(MIME_TYPE, mResolver.getType(URI_RED));
    }

    @Test
    public void testOpenDocument() throws Exception {
        final ParcelFileDescriptor res = ParcelFileDescriptor.open(new File("/dev/null"),
                ParcelFileDescriptor.MODE_READ_ONLY);

        doReturn(res).when(mProvider).openDocument(DOC_RED, "r", null);
        assertEquals(res, mResolver.openFile(URI_RED, "r", null));
    }

    @Test
    public void testOpenDocumentThumbnail() throws Exception {
        final AssetFileDescriptor res = new AssetFileDescriptor(
                ParcelFileDescriptor.open(new File("/dev/null"),
                        ParcelFileDescriptor.MODE_READ_ONLY),
                0, AssetFileDescriptor.UNKNOWN_LENGTH);

        final Point size = new Point(32, 32);
        final Bundle opts = new Bundle();
        opts.putParcelable(ContentResolver.EXTRA_SIZE, size);

        doReturn(res).when(mProvider).openDocumentThumbnail(DOC_RED, size, null);
        assertEquals(res, mResolver.openTypedAssetFile(URI_RED, MIME_TYPE, opts, null));
    }

    @Test
    public void testOpenTypedDocument() throws Exception {
        final AssetFileDescriptor res = new AssetFileDescriptor(
                ParcelFileDescriptor.open(new File("/dev/null"),
                        ParcelFileDescriptor.MODE_READ_ONLY),
                0, AssetFileDescriptor.UNKNOWN_LENGTH);

        doReturn("image/png").when(mProvider).getDocumentType(DOC_RED);
        doReturn(res).when(mProvider).openTypedDocument(DOC_RED, "audio/*", null, null);
        assertEquals(res, mResolver.openTypedAssetFile(URI_RED, "audio/*", null, null));
    }

    @Test
    public void testQueryDocument() throws Exception {
        final Cursor res = new MatrixCursor(new String[0]);

        doReturn(res).when(mProvider).queryDocument(DOC_RED, null);
        assertEquals(res, mResolver.query(URI_RED, null, null, null));
    }

    @Test
    public void testQueryRoots() throws Exception {
        final Cursor res = new MatrixCursor(new String[0]);

        doReturn(res).when(mProvider).queryRoots(null);
        assertEquals(res, mResolver.query(buildRootsUri(AUTHORITY), null, null, null));
    }

    @Test
    public void testQueryChildDocuments() throws Exception {
        final Cursor res = new MatrixCursor(new String[0]);

        doReturn(res).when(mProvider).queryChildDocuments(DOC_RED, null, Bundle.EMPTY);
        assertEquals(res, mResolver.query(buildChildDocumentsUri(AUTHORITY, DOC_RED), null,
                Bundle.EMPTY, null));
    }

    @Test
    public void testQueryRecentDocuments() throws Exception {
        final Cursor res = new MatrixCursor(new String[0]);

        doReturn(res).when(mProvider).queryRecentDocuments(DOC_RED, null, Bundle.EMPTY, null);
        assertEquals(res, mResolver.query(buildRecentDocumentsUri(AUTHORITY, DOC_RED), null,
                Bundle.EMPTY, null));
    }

    @Test
    public void testQuerySearchDocuments() throws Exception {
        final Cursor res = new MatrixCursor(new String[0]);

        doReturn(res).when(mProvider).querySearchDocuments(DOC_RED, null, Bundle.EMPTY);
        assertEquals(res, mResolver.query(buildSearchDocumentsUri(AUTHORITY, DOC_RED, "moo"), null,
                Bundle.EMPTY, null));
    }

    @Test
    public void testGetDocumentThumbnail() throws Exception {
        // create file and image
        final String testImagePath =
                InstrumentationRegistry.getTargetContext().getExternalCacheDir().getPath()
                        + "/testimage.jpg";
        final int imageSize = 128;
        final int thumbnailSize = 32;
        File file = new File(testImagePath);
        try (FileOutputStream out = new FileOutputStream(file)) {
            writeImage(imageSize, imageSize, Color.RED, out);
        }

        final AssetFileDescriptor res = new AssetFileDescriptor(
                ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY),
                0, AssetFileDescriptor.UNKNOWN_LENGTH);
        final Point size = new Point(thumbnailSize, thumbnailSize);
        final Bundle opts = new Bundle();
        opts.putParcelable(ContentResolver.EXTRA_SIZE, size);

        doReturn(res).when(mProvider).openDocumentThumbnail(DOC_RED, size, null);
        Bitmap bitmap = DocumentsContract.getDocumentThumbnail(mResolver, URI_RED, size, null);

        // A provider may return a thumbnail of a different size, but never more than double the
        // requested size.
        assertFalse(bitmap.getWidth() > thumbnailSize * 2);
        assertFalse(bitmap.getHeight() > thumbnailSize * 2);
        assertColorMostlyEquals(Color.RED,
                bitmap.getPixel(bitmap.getWidth() / 2, bitmap.getHeight() / 2));

        // clean up
        file.delete();
        bitmap.recycle();
    }

    private static void writeImage(int width, int height, int color, OutputStream out) {
        final Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(color);
        bitmap.compress(Bitmap.CompressFormat.PNG, 90, out);
    }

    /**
     * Since thumbnails might be bounced through a compression pass, we're okay
     * if they're mostly equal.
     */
    private static void assertColorMostlyEquals(int expected, int actual) {
        assertEquals(Integer.toHexString(expected & 0xF0F0F0F0),
                Integer.toHexString(actual & 0xF0F0F0F0));
    }
}
