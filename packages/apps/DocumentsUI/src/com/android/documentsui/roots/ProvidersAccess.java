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

package com.android.documentsui.roots;

import static com.android.documentsui.base.SharedMinimal.DEBUG;
import static com.android.documentsui.base.SharedMinimal.VERBOSE;

import android.util.Log;

import androidx.annotation.Nullable;

import com.android.documentsui.base.MimeTypes;
import com.android.documentsui.base.RootInfo;
import com.android.documentsui.base.State;
import com.android.documentsui.base.UserId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Provides testable access to key {@link ProvidersCache} methods.
 */
public interface ProvidersAccess {

    String BROADCAST_ACTION = "com.android.documentsui.action.ROOT_CHANGED";

    /**
     * Return the requested {@link RootInfo}, but only loading the roots for the
     * requested user and authority. This is useful when we want to load fast without
     * waiting for all the other roots to come back.
     */
    RootInfo getRootOneshot(UserId userId, String authority, String rootId);

    Collection<RootInfo> getMatchingRootsBlocking(State state);

    Collection<RootInfo> getRootsBlocking();

    RootInfo getDefaultRootBlocking(State state);

    RootInfo getRecentsRoot(UserId userId);

    String getApplicationName(UserId userId, String authority);

    String getPackageName(UserId userId, String authority);

    /**
     * Returns a list of roots for the specified user and authority. If not found, then
     * an empty list is returned.
     */
    Collection<RootInfo> getRootsForAuthorityBlocking(UserId userId, String authority);

    public static List<RootInfo> getMatchingRoots(Collection<RootInfo> roots, State state) {

        final String tag = "ProvidersAccess";

        final List<RootInfo> matching = new ArrayList<>();
        for (RootInfo root : roots) {

            if (VERBOSE) Log.v(tag, "Evaluationg root: " + root);

            if (state.action == State.ACTION_CREATE && !root.supportsCreate()) {
                if (VERBOSE) Log.v(tag, "Excluding read-only root because: ACTION_CREATE.");
                continue;
            }

            if (state.action == State.ACTION_PICK_COPY_DESTINATION
                    && !root.supportsCreate()) {
                if (VERBOSE) Log.v(
                        tag, "Excluding read-only root because: ACTION_PICK_COPY_DESTINATION.");
                continue;
            }

            if (state.action == State.ACTION_OPEN_TREE && !root.supportsChildren()) {
                if (VERBOSE) Log.v(
                        tag, "Excluding root !supportsChildren because: ACTION_OPEN_TREE.");
                continue;
            }

            if (state.action == State.ACTION_OPEN_TREE && root.isRecents()) {
                if (VERBOSE) Log.v(
                        tag, "Excluding recent root because: ACTION_OPEN_TREE.");
                continue;
            }

            if (state.localOnly && !root.isLocalOnly()) {
                if (VERBOSE) Log.v(tag, "Excluding root because: unwanted non-local device.");
                continue;
            }

            if (state.action == State.ACTION_OPEN && root.isEmpty()) {
                if (VERBOSE) Log.v(tag, "Excluding empty root because: ACTION_OPEN.");
                continue;
            }

            if (state.action == State.ACTION_GET_CONTENT && root.isEmpty()) {
                if (VERBOSE) Log.v(tag, "Excluding empty root because: ACTION_GET_CONTENT.");
                continue;
            }

            if (!UserId.CURRENT_USER.equals(root.userId) && !state.supportsCrossProfile()) {
                if (VERBOSE) {
                    Log.v(tag, "Excluding root because: action does not support cross profile.");
                }
                continue;
            }

            final boolean overlap =
                    MimeTypes.mimeMatches(root.derivedMimeTypes, state.acceptMimes) ||
                    MimeTypes.mimeMatches(state.acceptMimes, root.derivedMimeTypes);
            if (!overlap) {
                if (VERBOSE) Log.v(
                        tag, "Excluding root because: unsupported content types > "
                        + Arrays.toString(state.acceptMimes));
                continue;
            }

            if (state.excludedAuthorities.contains(root.authority)) {
                if (VERBOSE) Log.v(tag, "Excluding root because: owned by calling package.");
                continue;
            }

            matching.add(root);
        }

        if (DEBUG) {
            Log.d(tag, "Matched roots: " + matching);
        }
        return matching;
    }

    /**
     * Returns the root should default show on current state.
     */
    static @Nullable RootInfo getDefaultRoot(Collection<RootInfo> roots, State state) {
        for (RootInfo root : ProvidersAccess.getMatchingRoots(roots, state)) {
            if (root.isExternalStorage() && state.action == State.ACTION_OPEN_TREE) {
                return root;
            }
            if (root.isDownloads()) {
                return root;
            }
        }
        return null;
    }

}
