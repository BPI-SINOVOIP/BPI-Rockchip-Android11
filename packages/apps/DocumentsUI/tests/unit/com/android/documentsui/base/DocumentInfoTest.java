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

package com.android.documentsui.base;

import static androidx.core.util.Preconditions.checkArgument;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.rule.provider.ProviderTestRule;

import com.android.documentsui.InspectorProvider;
import com.android.documentsui.testing.TestProvidersAccess;
import com.android.documentsui.util.VersionUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import java.util.HashSet;
import java.util.Set;

@SmallTest
public class DocumentInfoTest extends AndroidTestCase {

    private static final DocumentInfo TEST_DOC
            = createDocInfo("authority.a", "doc.1", "text/plain");
    private static final String FOLDER_NAME = "Top";
    private static final String FILE_NAME = InspectorProvider.OPEN_IN_PROVIDER_TEST;

    private Context mContext;
    private ContentResolver mResolver;

    @Rule
    private ProviderTestRule mProviderTestRule = new ProviderTestRule.Builder(
            InspectorProvider.class, InspectorProvider.AUTHORITY).build();

    @Before
    public void setUp() throws Exception {
        super.setUp();

        mContext = prepareContentResolverSource();
        mResolver = mContext.getContentResolver();
    }

    private static DocumentInfo createDocInfo(String authority, String docId, String mimeType) {
        DocumentInfo doc = new DocumentInfo();
        doc.userId = UserId.DEFAULT_USER;
        doc.authority = authority;
        doc.documentId = docId;
        doc.mimeType = mimeType;
        doc.deriveFields();
        return doc;
    }

    protected Context prepareContentResolverSource() {
        ContentResolver contentResolver = mProviderTestRule.getResolver();
        Context context = mock(Context.class);
        // inject ContentResolver
        when(context.getContentResolver()).thenReturn(contentResolver);
        return context;
    }

    @Test
    public void testEquals() throws Exception {
        assertEquals(TEST_DOC, TEST_DOC);
        assertEquals(TEST_DOC, createDocInfo("authority.a", "doc.1", "text/plain"));
    }

    @Test
    public void testEquals_HandlesNulls() throws Exception {
        assertFalse(TEST_DOC.equals(null));
    }

    @Test
    public void testEquals_HandlesNullFields() throws Exception {
        assertFalse(TEST_DOC.equals(new DocumentInfo()));
        assertFalse(new DocumentInfo().equals(TEST_DOC));
    }

    @Test
    public void testNotEquals_differentUser() throws Exception {
        DocumentInfo documentInfo1 = createDocInfo("authority.a", "doc.1", "text/plain");
        DocumentInfo documentInfo2 = createDocInfo("authority.a", "doc.1", "text/plain");
        documentInfo1.userId = UserId.of(documentInfo2.userId.getIdentifier() + 1);
        assertFalse(documentInfo1.equals(documentInfo2));
    }

    @Test
    public void testNotEquals_differentAuthority() throws Exception {
        assertFalse(TEST_DOC.equals(createDocInfo("authority.b", "doc.1", "text/plain")));
    }

    @Test
    public void testNotEquals_differentDocId() throws Exception {
        assertFalse(TEST_DOC.equals(createDocInfo("authority.a", "doc.2", "text/plain")));
    }

    @Test
    public void testNotEquals_differentMimetype() throws Exception {
        assertFalse(TEST_DOC.equals(createDocInfo("authority.a", "doc.1", "image/png")));
    }

    @Test
    public void testFolderMimeTypeFromUri() throws Exception {
        final Uri validUri = DocumentsContract.buildDocumentUri(
                InspectorProvider.AUTHORITY, FOLDER_NAME);

        final Set<String> mimeTypes = new HashSet<>();
        DocumentInfo.addMimeTypes(mResolver, validUri, mimeTypes);

        assertThat(mimeTypes.size()).isEqualTo(1);

        assertThat(mimeTypes.contains(DocumentsContract.Document.MIME_TYPE_DIR)).isTrue();
    }

    @Test
    public void testFileMimeTypeFromUri() throws Exception {
        final Uri validUri = DocumentsContract.buildDocumentUri(
                InspectorProvider.AUTHORITY, FILE_NAME);

        final Set<String> mimeTypes = new HashSet<>();
        DocumentInfo.addMimeTypes(mResolver, validUri, mimeTypes);

        assertThat(mimeTypes.size()).isEqualTo(1);

        assertThat(mimeTypes.contains("text/plain")).isTrue();
    }

    @Test
    public void testGetTreeDocumentUri_currentUser() {
        checkArgument(UserId.CURRENT_USER.equals(TEST_DOC.userId));

        assertThat(TEST_DOC.getTreeDocumentUri())
                .isEqualTo(DocumentsContract.buildTreeDocumentUri(TEST_DOC.authority,
                        TEST_DOC.documentId));
    }

    @Test
    public void testGetTreeDocumentUri_otherUser_shouldHaveDifferentUri() {
        if (VersionUtils.isAtLeastR()) {
            final DocumentInfo doc = createDocInfo("authority.a", "doc.1", "text/plain");
            final DocumentInfo otherUserDoc = createDocInfo("authority.a", "doc.1", "text/plain");
            otherUserDoc.userId = TestProvidersAccess.OtherUser.USER_ID;

            // Make sure they do not return the same tree uri
            assertThat(otherUserDoc.getTreeDocumentUri()).isNotEqualTo(doc.getTreeDocumentUri());
        }
    }

    @Test
    public void testGetTreeDocumentUri_otherUser_sameHostAndPath() {
        if (VersionUtils.isAtLeastR()) {
            final DocumentInfo doc = createDocInfo("authority.a", "doc.1", "text/plain");
            final DocumentInfo otherUserDoc = createDocInfo("authority.a", "doc.1", "text/plain");
            otherUserDoc.userId = TestProvidersAccess.OtherUser.USER_ID;

            // They should have same host(authority without user info) and path
            assertThat(otherUserDoc.getTreeDocumentUri().getHost())
                    .isEqualTo(doc.getTreeDocumentUri().getHost());
            assertThat(otherUserDoc.getTreeDocumentUri().getPath())
                    .isEqualTo(doc.getTreeDocumentUri().getPath());
        }
    }

    @Test
    public void testGetTreeDocumentUri_otherUser_userInfo() {
        if (VersionUtils.isAtLeastR()) {
            final DocumentInfo doc = createDocInfo("authority.a", "doc.1", "text/plain");
            final DocumentInfo otherUserDoc = createDocInfo("authority.a", "doc.1", "text/plain");
            otherUserDoc.userId = TestProvidersAccess.OtherUser.USER_ID;

            // Different user info between doc and otherUserDoc
            assertThat(otherUserDoc.getTreeDocumentUri().getUserInfo())
                    .isNotEqualTo(doc.getTreeDocumentUri().getUserInfo());

            // Same user info within otherUserDoc
            assertThat(otherUserDoc.getTreeDocumentUri().getUserInfo())
                    .isEqualTo(otherUserDoc.getDocumentUri().getUserInfo());
        }
    }
}
