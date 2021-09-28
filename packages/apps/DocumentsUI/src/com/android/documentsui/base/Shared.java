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

import static com.android.documentsui.base.SharedMinimal.TAG;

import android.app.Activity;
import android.app.compat.CompatChanges;
import android.compat.annotation.ChangeId;
import android.compat.annotation.EnabledAfter;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Looper;
import android.os.Process;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.annotation.PluralsRes;
import androidx.appcompat.app.AlertDialog;

import com.android.documentsui.R;
import com.android.documentsui.ui.MessageBuilder;
import com.android.documentsui.util.VersionUtils;

import java.text.Collator;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

/** @hide */
public final class Shared {

    /** Intent action name to pick a copy destination. */
    public static final String ACTION_PICK_COPY_DESTINATION =
            "com.android.documentsui.PICK_COPY_DESTINATION";

    // These values track values declared in MediaDocumentsProvider.
    public static final String METADATA_KEY_AUDIO = "android.media.metadata.audio";
    public static final String METADATA_KEY_VIDEO = "android.media.metadata.video";
    public static final String METADATA_VIDEO_LATITUDE = "android.media.metadata.video:latitude";
    public static final String METADATA_VIDEO_LONGITUTE = "android.media.metadata.video:longitude";

    /**
     * Extra flag used to store the current stack so user opens in right spot.
     */
    public static final String EXTRA_STACK = "com.android.documentsui.STACK";

    /**
     * Extra flag used to store query of type String in the bundle.
     */
    public static final String EXTRA_QUERY = "query";

    /**
     * Extra flag used to store chip's title of type String array in the bundle.
     */
    public static final String EXTRA_QUERY_CHIPS = "query_chips";

    /**
     * Extra flag used to store state of type State in the bundle.
     */
    public static final String EXTRA_STATE = "state";

    /**
     * Extra flag used to store root of type RootInfo in the bundle.
     */
    public static final String EXTRA_ROOT = "root";

    /**
     * Extra flag used to store document of DocumentInfo type in the bundle.
     */
    public static final String EXTRA_DOC = "document";

    /**
     * Extra flag used to store DirectoryFragment's selection of Selection type in the bundle.
     */
    public static final String EXTRA_SELECTION = "selection";

    /**
     * Extra flag used to store DirectoryFragment's ignore state of boolean type in the bundle.
     */
    public static final String EXTRA_IGNORE_STATE = "ignoreState";

    /**
     * Extra flag used to store pick result state of PickResult type in the bundle.
     */
    public static final String EXTRA_PICK_RESULT = "pickResult";

    /**
     * Extra for an Intent for enabling performance benchmark. Used only by tests.
     */
    public static final String EXTRA_BENCHMARK = "com.android.documentsui.benchmark";

    /**
     * Extra flag used to signify to inspector that debug section can be shown.
     */
    public static final String EXTRA_SHOW_DEBUG = "com.android.documentsui.SHOW_DEBUG";

    /**
     * Maximum number of items in a Binder transaction packet.
     */
    public static final int MAX_DOCS_IN_INTENT = 500;

    /**
     * Animation duration of checkbox in directory list/grid in millis.
     */
    public static final int CHECK_ANIMATION_DURATION = 100;

    /**
     * Class name of launcher icon avtivity.
     */
    public static final String LAUNCHER_TARGET_CLASS = "com.android.documentsui.LauncherActivity";

    private static final Collator sCollator;

    /**
     * We support restrict Storage Access Framework from {@link android.os.Build.VERSION_CODES#R}.
     * App Compatibility flag that indicates whether the app should be restricted or not.
     * This flag is turned on by default for all apps targeting >
     * {@link android.os.Build.VERSION_CODES#Q}.
     */
    @ChangeId
    @EnabledAfter(targetSdkVersion = android.os.Build.VERSION_CODES.Q)
    private static final long RESTRICT_STORAGE_ACCESS_FRAMEWORK = 141600225L;

    static {
        sCollator = Collator.getInstance();
        sCollator.setStrength(Collator.SECONDARY);
    }

    /**
     * @deprecated use {@link MessageBuilder#getQuantityString}
     */
    @Deprecated
    public static String getQuantityString(Context context, @PluralsRes int resourceId,
            int quantity) {
        return context.getResources().getQuantityString(resourceId, quantity, quantity);
    }

    /**
     * Whether the calling app should be restricted in Storage Access Framework or not.
     */
    public static boolean shouldRestrictStorageAccessFramework(Activity activity) {
        if (!VersionUtils.isAtLeastR()) {
            return false;
        }

        final String packageName = getCallingPackageName(activity);
        final boolean ret = CompatChanges.isChangeEnabled(RESTRICT_STORAGE_ACCESS_FRAMEWORK,
                packageName, Process.myUserHandle());

        Log.d(TAG,
                "shouldRestrictStorageAccessFramework = " + ret + ", packageName = " + packageName);

        return ret;
    }

    public static String formatTime(Context context, long when) {
        // TODO: DateUtils should make this easier
        ZoneId zoneId = ZoneId.systemDefault();
        LocalDateTime then = LocalDateTime.ofInstant(Instant.ofEpochMilli(when), zoneId);
        LocalDateTime now = LocalDateTime.ofInstant(Instant.now(), zoneId);

        int flags = DateUtils.FORMAT_NO_NOON | DateUtils.FORMAT_NO_MIDNIGHT
                | DateUtils.FORMAT_ABBREV_ALL;

        if (then.getYear() != now.getYear()) {
            flags |= DateUtils.FORMAT_SHOW_YEAR | DateUtils.FORMAT_SHOW_DATE;
        } else if (then.getDayOfYear() != now.getDayOfYear()) {
            flags |= DateUtils.FORMAT_SHOW_DATE;
        } else {
            flags |= DateUtils.FORMAT_SHOW_TIME;
        }

        return DateUtils.formatDateTime(context, when, flags);
    }

    /**
     * A convenient way to transform any list into a (parcelable) ArrayList.
     * Uses cast if possible, else creates a new list with entries from {@code list}.
     */
    public static <T> ArrayList<T> asArrayList(List<T> list) {
        return list instanceof ArrayList
            ? (ArrayList<T>) list
            : new ArrayList<>(list);
    }

    /**
     * Compare two strings against each other using system default collator in a
     * case-insensitive mode. Clusters strings prefixed with {@link DIR_PREFIX}
     * before other items.
     */
    public static int compareToIgnoreCaseNullable(String lhs, String rhs) {
        final boolean leftEmpty = TextUtils.isEmpty(lhs);
        final boolean rightEmpty = TextUtils.isEmpty(rhs);

        if (leftEmpty && rightEmpty) return 0;
        if (leftEmpty) return -1;
        if (rightEmpty) return 1;

        return sCollator.compare(lhs, rhs);
    }

    private static boolean isSystemApp(ApplicationInfo ai) {
        return (ai.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }

    private static boolean isUpdatedSystemApp(ApplicationInfo ai) {
        return (ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0;
    }

    /**
     * Returns the calling package, possibly overridden by EXTRA_PACKAGE_NAME.
     * @param activity
     * @return
     */
    public static String getCallingPackageName(Activity activity) {
        String callingPackage = activity.getCallingPackage();
        // System apps can set the calling package name using an extra.
        try {
            ApplicationInfo info =
                    activity.getPackageManager().getApplicationInfo(callingPackage, 0);
            if (isSystemApp(info) || isUpdatedSystemApp(info)) {
                final String extra = activity.getIntent().getStringExtra(
                        Intent.EXTRA_PACKAGE_NAME);
                if (extra != null && !TextUtils.isEmpty(extra)) {
                    callingPackage = extra;
                }
            }
        } catch (NameNotFoundException e) {
            // Couldn't lookup calling package info. This isn't really
            // gonna happen, given that we're getting the name of the
            // calling package from trusty old Activity.getCallingPackage.
            // For that reason, we ignore this exception.
        }
        return callingPackage;
    }

    /**
     * Returns the calling app name.
     * @param activity
     * @return the calling app name or general anonymous name if not found
     */
    @NonNull
    public static String getCallingAppName(Activity activity) {
        final String anonymous = activity.getString(R.string.anonymous_application);
        final String packageName = getCallingPackageName(activity);
        if (TextUtils.isEmpty(packageName)) {
            return anonymous;
        }

        final PackageManager pm = activity.getPackageManager();
        ApplicationInfo ai;
        try {
            ai = pm.getApplicationInfo(packageName, 0);
        } catch (final PackageManager.NameNotFoundException e) {
            return anonymous;
        }

        CharSequence result = pm.getApplicationLabel(ai);
        return TextUtils.isEmpty(result) ? anonymous : result.toString();
    }

    /**
     * Returns the default directory to be presented after starting the activity.
     * Method can be overridden if the change of the behavior of the the child activity is needed.
     */
    public static Uri getDefaultRootUri(Activity activity) {
        Uri defaultUri = Uri.parse(activity.getResources().getString(R.string.default_root_uri));

        if (!DocumentsContract.isRootUri(activity, defaultUri)) {
            Log.e(TAG, "Default Root URI is not a valid root URI, falling back to Downloads.");
            defaultUri = DocumentsContract.buildRootUri(Providers.AUTHORITY_DOWNLOADS,
                    Providers.ROOT_ID_DOWNLOADS);
        }

        return defaultUri;
    }

    public static boolean isHardwareKeyboardAvailable(Context context) {
        return context.getResources().getConfiguration().keyboard != Configuration.KEYBOARD_NOKEYS;
    }

    public static void ensureKeyboardPresent(Context context, AlertDialog dialog) {
        if (!isHardwareKeyboardAvailable(context)) {
            dialog.getWindow().setSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
        }
    }

    /**
     * Check config whether DocumentsUI is launcher enabled or not.
     * @return true if launcher icon is shown.
     */
    public static boolean isLauncherEnabled(Context context) {
        PackageManager pm = context.getPackageManager();
        if (pm != null) {
            final ComponentName component = new ComponentName(
                    context.getPackageName(), LAUNCHER_TARGET_CLASS);
            final int value = pm.getComponentEnabledSetting(component);
            return value == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
        }

        return false;
    }

    public static String getDeviceName(ContentResolver resolver) {
        // We match the value supplied by ExternalStorageProvider for
        // the internal storage root.
        return Settings.Global.getString(resolver, Settings.Global.DEVICE_NAME);
    }

    public static void checkMainLoop() {
        if (Looper.getMainLooper() != Looper.myLooper()) {
            Log.e(TAG, "Calling from non-UI thread!");
        }
    }

    /**
     * This method exists solely to smooth over the fact that two different types of
     * views cannot be bound to the same id in different layouts. "What's this crazy-pants
     * stuff?", you say? Here's an example:
     *
     * The main DocumentsUI view (aka "Files app") when running on a phone has a drop-down
     * "breadcrumb" (file path representation) in both landscape and portrait orientation.
     * Larger format devices, like a tablet, use a horizontal "Dir1 > Dir2 > Dir3" format
     * breadcrumb in landscape layouts, but the regular drop-down breadcrumb in portrait
     * mode.
     *
     * Our initial inclination was to give each of those views the same ID (as they both
     * implement the same "Breadcrumb" interface). But at runtime, when rotating a device
     * from one orientation to the other, deeeeeeep within the UI toolkit a exception
     * would happen, because one view instance (drop-down) was being inflated in place of
     * another (horizontal). I'm writing this code comment significantly after the face,
     * so I don't recall all of the details, but it had to do with View type-checking the
     * Parcelable state in onRestore, or something like that. Either way, this isn't
     * allowed (my patch to fix this was rejected).
     *
     * To work around this we have this cute little method that accepts multiple
     * resource IDs, and along w/ type inference finds our view, no matter which
     * id it is wearing, and returns it.
     */
    @SuppressWarnings("TypeParameterUnusedInFormals")
    public static @Nullable <T> T findView(Activity activity, int... resources) {
        for (int id : resources) {
            @SuppressWarnings("unchecked")
            View view = activity.findViewById(id);
            if (view != null) {
                return (T) view;
            }
        }
        return null;
    }

    private Shared() {
        throw new UnsupportedOperationException("provides static fields only");
    }
}
