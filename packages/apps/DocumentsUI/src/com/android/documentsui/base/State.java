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

package com.android.documentsui.base;

import android.content.Intent;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.SparseArray;

import androidx.annotation.IntDef;

import com.android.documentsui.services.FileOperationService;
import com.android.documentsui.services.FileOperationService.OpType;
import com.android.documentsui.sorting.SortModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

public class State implements android.os.Parcelable {

    private static final String TAG = "State";

    @IntDef(flag = true, value = {
            ACTION_BROWSE,
            ACTION_PICK_COPY_DESTINATION,
            ACTION_OPEN,
            ACTION_CREATE,
            ACTION_GET_CONTENT,
            ACTION_OPEN_TREE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionType {}
    // File manager and related private picking activity.
    public static final int ACTION_BROWSE = 1;
    public static final int ACTION_PICK_COPY_DESTINATION = 2;
    // All public picking activities
    public static final int ACTION_OPEN = 3;
    public static final int ACTION_CREATE = 4;
    public static final int ACTION_GET_CONTENT = 5;
    public static final int ACTION_OPEN_TREE = 6;

    @IntDef(flag = true, value = {
            MODE_UNKNOWN,
            MODE_LIST,
            MODE_GRID
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewMode {}
    public static final int MODE_UNKNOWN = 0;
    public static final int MODE_LIST = 1;
    public static final int MODE_GRID = 2;

    public @ActionType int action;
    public String[] acceptMimes;

    /** Derived from local preferences */
    public @ViewMode int derivedMode = MODE_GRID;

    public boolean debugMode = false;

    /** Current sort state */
    public SortModel sortModel;

    public boolean allowMultiple;
    public boolean localOnly;

    public boolean openableOnly;
    public boolean restrictScopeStorage;
    public boolean showHiddenFiles;

    /**
     * Represents whether the state supports cross-profile file picking.
     */
    public boolean supportsCrossProfile = false;

    /**
     * Represents whether the intent is a cross-profile intent
     */
    public boolean canShareAcrossProfile = false;

    /**
     * Returns true if we are allowed to interact with the user.
     */
    public boolean canInteractWith(UserId userId) {
        return canShareAcrossProfile || UserId.CURRENT_USER.equals(userId);
    }

    /**
     * This is basically a sub-type for the copy operation. It can be either COPY,
     * COMPRESS, EXTRACT or MOVE.
     * The only legal values, if set, are: OPERATION_COPY, OPERATION_COMPRESS,
     * OPERATION_EXTRACT and OPERATION_MOVE. Other pick
     * operations don't use this. In those cases OPERATION_UNKNOWN is also legal.
     */
    public @OpType int copyOperationSubType = FileOperationService.OPERATION_UNKNOWN;

    /** Current user navigation stack; empty implies recents. */
    public final DocumentStack stack = new DocumentStack();

    /** Instance configs for every shown directory */
    public HashMap<String, SparseArray<Parcelable>> dirConfigs = new HashMap<>();

    /** Name of the package that started DocsUI */
    public List<String> excludedAuthorities = new ArrayList<>();

    public void initAcceptMimes(Intent intent, String defaultAcceptMimeType) {
        if (intent.hasExtra(Intent.EXTRA_MIME_TYPES)) {
            acceptMimes = intent.getStringArrayExtra(Intent.EXTRA_MIME_TYPES);
        } else {
            acceptMimes = new String[] { defaultAcceptMimeType };
        }
    }

    /**
     * Check current action should have preview function or not.
     * @return True, if the action should have preview.
     */
    public boolean shouldShowPreview() {
        return action == ACTION_GET_CONTENT
                || action == ACTION_OPEN_TREE
                || action == ACTION_OPEN;
    }

    /**
     * Check the action is file picking and acceptMimes are all images type or not.
     * @return True, if acceptMimes are all image type and action is file picking.
     */
    public boolean isPhotoPicking() {
        if (action != ACTION_GET_CONTENT && action != ACTION_OPEN || acceptMimes == null) {
            return false;
        }

        for (String mimeType : acceptMimes) {
            if (!MimeTypes.mimeMatches(MimeTypes.IMAGE_MIME, mimeType)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Returns true if DocsUI supports cross-profile for this {@link State}.
     */
    public boolean supportsCrossProfile() {
        return supportsCrossProfile;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(action);
        out.writeStringArray(acceptMimes);
        out.writeInt(allowMultiple ? 1 : 0);
        out.writeInt(localOnly ? 1 : 0);
        DurableUtils.writeToParcel(out, stack);
        out.writeMap(dirConfigs);
        out.writeList(excludedAuthorities);
        out.writeInt(openableOnly ? 1 : 0);
        out.writeInt(restrictScopeStorage ? 1 : 0);
        out.writeParcelable(sortModel, 0);
    }

    @Override
    public String toString() {
        return "State{"
                + "action=" + action
                + ", acceptMimes=" + Arrays.toString(acceptMimes)
                + ", allowMultiple=" + allowMultiple
                + ", localOnly=" + localOnly
                + ", stack=" + stack
                + ", dirConfigs=" + dirConfigs
                + ", excludedAuthorities=" + excludedAuthorities
                + ", openableOnly=" + openableOnly
                + ", restrictScopeStorage=" + restrictScopeStorage
                + ", sortModel=" + sortModel
                + "}";
    }

    public static final ClassLoaderCreator<State> CREATOR = new ClassLoaderCreator<State>() {
        @Override
        public State createFromParcel(Parcel in) {
            return createFromParcel(in, null);
        }

        @Override
        public State createFromParcel(Parcel in, ClassLoader loader) {
            final State state = new State();
            state.action = in.readInt();
            state.acceptMimes = in.createStringArray();
            state.allowMultiple = in.readInt() != 0;
            state.localOnly = in.readInt() != 0;
            DurableUtils.readFromParcel(in, state.stack);
            in.readMap(state.dirConfigs, loader);
            in.readList(state.excludedAuthorities, loader);
            state.openableOnly = in.readInt() != 0;
            state.restrictScopeStorage = in.readInt() != 0;
            state.sortModel = in.readParcelable(loader);
            return state;
        }

        @Override
        public State[] newArray(int size) {
            return new State[size];
        }
    };
}
