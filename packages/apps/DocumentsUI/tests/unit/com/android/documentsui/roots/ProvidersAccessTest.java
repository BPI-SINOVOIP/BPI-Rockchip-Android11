/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.documentsui.roots;

import static com.google.common.collect.Lists.newArrayList;
import static com.google.common.truth.Truth.assertThat;

import android.provider.DocumentsContract;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.annotation.Nullable;

import com.android.documentsui.base.Providers;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;

import com.google.common.collect.Lists;
import com.google.common.truth.Correspondence;

import java.util.List;
import java.util.Objects;

@SmallTest
public class ProvidersAccessTest extends AndroidTestCase {

    private static final UserId OTHER_USER = UserId.of(UserId.DEFAULT_USER.getIdentifier() + 1);
    private static final Correspondence<RootInfo, RootInfo> USER_ID_MIME_TYPES_CORRESPONDENCE =
            new Correspondence<RootInfo, RootInfo>() {
                @Override
                public boolean compare(@Nullable RootInfo actual, @Nullable RootInfo expected) {
                    return actual != null && expected != null
                            && Objects.equals(actual.userId, expected.userId)
                            && Objects.equals(actual.derivedMimeTypes, expected.derivedMimeTypes);
                }

                @Override
                public String toString() {
                    return "has same userId and mimeTypes as in";
                }
            };

    private static RootInfo mNull = buildForMimeTypes((String[]) null);
    private static RootInfo mEmpty = buildForMimeTypes();
    private static RootInfo mWild = buildForMimeTypes("*/*");
    private static RootInfo mImages = buildForMimeTypes("image/*");
    private static RootInfo mAudio = buildForMimeTypes(
            "audio/*", "application/ogg", "application/x-flac");
    private static RootInfo mDocs = buildForMimeTypes(
            "application/msword", "application/vnd.ms-excel");
    private static RootInfo mMalformed1 = buildForMimeTypes("meow");
    private static RootInfo mMalformed2 = buildForMimeTypes("*/meow");
    private static RootInfo mImagesOtherUser = buildForMimeTypes(OTHER_USER, "image/*");

    private List<RootInfo> mRoots;

    private State mState;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mRoots = Lists.newArrayList(
                mNull, mWild, mEmpty, mImages, mAudio, mDocs, mMalformed1, mMalformed2,
                mImagesOtherUser);

        mState = new State();
        mState.action = State.ACTION_OPEN;
        mState.localOnly = false;
    }

    public void testMatchingRoots_Everything() throws Exception {
        mState.acceptMimes = new String[]{"*/*"};
        assertContainsExactly(
                newArrayList(mNull, mWild, mImages, mAudio, mDocs, mMalformed1, mMalformed2),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_PngOrWild() throws Exception {
        mState.acceptMimes = new String[] { "image/png", "*/*" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mImages, mAudio, mDocs, mMalformed1, mMalformed2),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_AudioWild() throws Exception {
        mState.acceptMimes = new String[] { "audio/*" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mAudio),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_AudioWildOrImageWild() throws Exception {
        mState.acceptMimes = new String[] { "audio/*", "image/*" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mAudio, mImages),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_AudioSpecific() throws Exception {
        mState.acceptMimes = new String[] { "audio/mpeg" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mAudio),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_Document() throws Exception {
        mState.acceptMimes = new String[] { "application/msword" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mDocs),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_Application() throws Exception {
        mState.acceptMimes = new String[] { "application/*" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mAudio, mDocs),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_FlacOrPng() throws Exception {
        mState.acceptMimes = new String[] { "application/x-flac", "image/png" };
        assertContainsExactly(
                newArrayList(mNull, mWild, mAudio, mImages),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testMatchingRoots_FlacOrPng_crossProfile() throws Exception {
        mState.supportsCrossProfile = true;
        mState.acceptMimes = new String[]{"application/x-flac", "image/png"};
        assertContainsExactly(
                newArrayList(mNull, mWild, mAudio, mImages, mImagesOtherUser),
                ProvidersAccess.getMatchingRoots(mRoots, mState));
    }

    public void testDefaultRoot() {
        mState.acceptMimes = new String[] { "*/*" };
        assertNull(ProvidersAccess.getDefaultRoot(mRoots, mState));

        RootInfo downloads = buildForMimeTypes("*/*");
        downloads.authority = Providers.AUTHORITY_DOWNLOADS;
        mRoots.add(downloads);

        assertEquals(downloads, ProvidersAccess.getDefaultRoot(mRoots, mState));
    }

    public void testDefaultRoot_openDocumentTree() {
        RootInfo storage = buildForMimeTypes("*/*");
        storage.authority = Providers.AUTHORITY_STORAGE;
        storage.flags = DocumentsContract.Root.FLAG_SUPPORTS_IS_CHILD;
        mRoots.add(storage);

        mState.action = State.ACTION_OPEN_TREE;
        mState.acceptMimes = new String[] { "*/*" };
        assertEquals(storage, ProvidersAccess.getDefaultRoot(mRoots, mState));
    }

    public void testExcludedAuthorities() throws Exception {
        final List<RootInfo> roots = newArrayList();

        // Set up some roots
        for (int i = 0; i < 5; ++i) {
            RootInfo root = new RootInfo();
            root.userId = UserId.DEFAULT_USER;
            root.authority = "authority" + i;
            roots.add(root);
        }
        // Make some allowed authorities
        List<RootInfo> allowedRoots = newArrayList(
            roots.get(0), roots.get(2), roots.get(4));
        // Set up the excluded authority list
        for (RootInfo root: roots) {
            if (!allowedRoots.contains(root)) {
                mState.excludedAuthorities.add(root.authority);
            }
        }
        mState.acceptMimes = new String[] { "*/*" };

        assertContainsExactly(
            allowedRoots,
            ProvidersAccess.getMatchingRoots(roots, mState));
    }

    private static void assertContainsExactly(List<RootInfo> expected, List<RootInfo> actual) {
        assertThat(actual)
                .comparingElementsUsing(USER_ID_MIME_TYPES_CORRESPONDENCE)
                .containsExactlyElementsIn(expected);
    }

    private static RootInfo buildForMimeTypes(String... mimeTypes) {
        return buildForMimeTypes(UserId.DEFAULT_USER, mimeTypes);
    }

    private static RootInfo buildForMimeTypes(UserId userId, String... mimeTypes) {
        final RootInfo root = new RootInfo();
        root.userId = userId;
        root.derivedMimeTypes = mimeTypes;
        return root;
    }
}
