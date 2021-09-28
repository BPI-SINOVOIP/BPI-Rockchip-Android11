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

package com.android.cts.usespermissiondiffcertapp;

import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_CLEAR_PRIMARY_CLIP;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_GRANT_URI;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_REVOKE_URI;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_SET_PRIMARY_CLIP;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_START_ACTIVITIES;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_START_ACTIVITY;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_START_SERVICE;
import static com.android.cts.permissiondeclareapp.UtilsProvider.ACTION_VERIFY_OUTGOING_PERSISTED;
import static com.android.cts.permissiondeclareapp.UtilsProvider.EXTRA_INTENT;
import static com.android.cts.permissiondeclareapp.UtilsProvider.EXTRA_MODE;
import static com.android.cts.permissiondeclareapp.UtilsProvider.EXTRA_PACKAGE_NAME;
import static com.android.cts.permissiondeclareapp.UtilsProvider.EXTRA_URI;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;

import com.android.cts.permissiondeclareapp.UtilsProvider;

import java.util.Objects;

public class Utils {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    private static void call(Intent intent) {
        final Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_INTENT, intent);
        getContext().getContentResolver().call(UtilsProvider.URI, "", "", extras);
    }

    static void grantClipUriPermissionViaActivity(ClipData clip, int mode) {
        grantClipUriPermission(clip, mode, ACTION_START_ACTIVITY);
    }

    static void grantClipUriPermissionViaActivities(ClipData clip, int mode) {
        grantClipUriPermission(clip, mode, ACTION_START_ACTIVITIES);
    }

    static void grantClipUriPermissionViaService(ClipData clip, int mode) {
        grantClipUriPermission(clip, mode, ACTION_START_SERVICE);
    }

    private static void grantClipUriPermission(ClipData clip, int mode, String action) {
        Intent grantIntent = new Intent();
        if (clip.getItemCount() == 1) {
            grantIntent.setData(clip.getItemAt(0).getUri());
        } else {
            grantIntent.setClipData(clip);
            // Make this Intent unique from the one that started it.
            for (int i=0; i<clip.getItemCount(); i++) {
                Uri uri = clip.getItemAt(i).getUri();
                if (uri != null) {
                    grantIntent.addCategory(uri.toString());
                }
            }
        }
        grantIntent.addFlags(mode);
        grantIntent.setClass(getContext(),
                Objects.equals(ACTION_START_SERVICE, action) ? ReceiveUriService.class
                        : ReceiveUriActivity.class);
        Intent intent = new Intent();
        intent.setAction(action);
        intent.putExtra(EXTRA_INTENT, grantIntent);
        call(intent);
    }

    static void grantClipUriPermissionViaContext(Uri uri, int mode) {
        Intent intent = new Intent();
        intent.setAction(ACTION_GRANT_URI);
        intent.putExtra(EXTRA_PACKAGE_NAME, getContext().getPackageName());
        intent.putExtra(EXTRA_URI, uri);
        intent.putExtra(EXTRA_MODE, mode);
        call(intent);
    }

    static void revokeClipUriPermissionViaContext(Uri uri, int mode) {
        Intent intent = new Intent();
        intent.setAction(ACTION_REVOKE_URI);
        intent.putExtra(EXTRA_URI, uri);
        intent.putExtra(EXTRA_MODE, mode);
        call(intent);
    }

    static void setPrimaryClip(ClipData clip) {
        Intent intent = new Intent();
        intent.setAction(ACTION_SET_PRIMARY_CLIP);
        intent.setClipData(clip);
        call(intent);
    }

    static void clearPrimaryClip() {
        Intent intent = new Intent();
        intent.setAction(ACTION_CLEAR_PRIMARY_CLIP);
        call(intent);
    }

    static void verifyOutgoingPersisted(Uri uri) {
        Intent intent = new Intent();
        intent.setAction(ACTION_VERIFY_OUTGOING_PERSISTED);
        intent.putExtra(EXTRA_URI, uri);
        call(intent);
    }
}
