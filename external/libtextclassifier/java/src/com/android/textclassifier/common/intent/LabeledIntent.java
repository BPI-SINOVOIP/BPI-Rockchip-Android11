/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.textclassifier.common.intent;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;
import androidx.annotation.DrawableRes;
import androidx.core.app.RemoteActionCompat;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.IconCompat;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.base.Preconditions;
import javax.annotation.Nullable;

/** Helper class to store the information from which RemoteActions are built. */
public final class LabeledIntent {
  private static final String TAG = "LabeledIntent";
  public static final int DEFAULT_REQUEST_CODE = 0;
  private static final TitleChooser DEFAULT_TITLE_CHOOSER =
      (labeledIntent, resolveInfo) -> {
        if (!TextUtils.isEmpty(labeledIntent.titleWithEntity)) {
          return labeledIntent.titleWithEntity;
        }
        return labeledIntent.titleWithoutEntity;
      };

  @Nullable public final String titleWithoutEntity;
  @Nullable public final String titleWithEntity;
  public final String description;
  @Nullable public final String descriptionWithAppName;
  // Do not update this intent.
  public final Intent intent;
  public final int requestCode;

  /**
   * Initializes a LabeledIntent.
   *
   * <p>NOTE: {@code requestCode} is required to not be {@link #DEFAULT_REQUEST_CODE} if
   * distinguishing info (e.g. the classified text) is represented in intent extras only. In such
   * circumstances, the request code should represent the distinguishing info (e.g. by generating a
   * hashcode) so that the generated PendingIntent is (somewhat) unique. To be correct, the
   * PendingIntent should be definitely unique but we try a best effort approach that avoids
   * spamming the system with PendingIntents.
   */
  // TODO: Fix the issue mentioned above so the behaviour is correct.
  public LabeledIntent(
      @Nullable String titleWithoutEntity,
      @Nullable String titleWithEntity,
      String description,
      @Nullable String descriptionWithAppName,
      Intent intent,
      int requestCode) {
    if (TextUtils.isEmpty(titleWithEntity) && TextUtils.isEmpty(titleWithoutEntity)) {
      throw new IllegalArgumentException(
          "titleWithEntity and titleWithoutEntity should not be both null");
    }
    this.titleWithoutEntity = titleWithoutEntity;
    this.titleWithEntity = titleWithEntity;
    this.description = Preconditions.checkNotNull(description);
    this.descriptionWithAppName = descriptionWithAppName;
    this.intent = Preconditions.checkNotNull(intent);
    this.requestCode = requestCode;
  }

  /**
   * Return the resolved result.
   *
   * @param context the context to resolve the result's intent and action
   * @param titleChooser for choosing an action title
   */
  @Nullable
  public Result resolve(Context context, @Nullable TitleChooser titleChooser) {
    final PackageManager pm = context.getPackageManager();
    final ResolveInfo resolveInfo = pm.resolveActivity(intent, 0);

    if (resolveInfo == null || resolveInfo.activityInfo == null) {
      TcLog.w(TAG, "resolveInfo or activityInfo is null");
      return null;
    }
    if (!hasPermission(context, resolveInfo.activityInfo)) {
      TcLog.d(TAG, "No permission to access: " + resolveInfo.activityInfo);
      return null;
    }

    final String packageName = resolveInfo.activityInfo.packageName;
    final String className = resolveInfo.activityInfo.name;
    if (packageName == null || className == null) {
      TcLog.w(TAG, "packageName or className is null");
      return null;
    }
    Intent resolvedIntent = new Intent(intent);
    boolean shouldShowIcon = false;
    IconCompat icon = null;
    if (!"android".equals(packageName)) {
      // We only set the component name when the package name is not resolved to "android"
      // to workaround a bug that explicit intent with component name == ResolverActivity
      // can't be launched on keyguard.
      resolvedIntent.setComponent(new ComponentName(packageName, className));
      if (resolveInfo.activityInfo.getIconResource() != 0) {
        icon =
            createIconFromPackage(context, packageName, resolveInfo.activityInfo.getIconResource());
        shouldShowIcon = true;
      }
    }
    if (icon == null) {
      // RemoteAction requires that there be an icon.
      icon = IconCompat.createWithResource(context, android.R.drawable.ic_menu_more);
    }
    final PendingIntent pendingIntent = createPendingIntent(context, resolvedIntent, requestCode);
    titleChooser = titleChooser == null ? DEFAULT_TITLE_CHOOSER : titleChooser;
    CharSequence title = titleChooser.chooseTitle(this, resolveInfo);
    if (TextUtils.isEmpty(title)) {
      TcLog.w(TAG, "Custom titleChooser return null, fallback to the default titleChooser");
      title = DEFAULT_TITLE_CHOOSER.chooseTitle(this, resolveInfo);
    }
    final RemoteActionCompat action =
        new RemoteActionCompat(icon, title, resolveDescription(resolveInfo, pm), pendingIntent);
    action.setShouldShowIcon(shouldShowIcon);
    return new Result(resolvedIntent, action);
  }

  private String resolveDescription(ResolveInfo resolveInfo, PackageManager packageManager) {
    if (!TextUtils.isEmpty(descriptionWithAppName)) {
      // Example string format of descriptionWithAppName: "Use %1$s to open map".
      String applicationName = getApplicationName(resolveInfo, packageManager);
      if (!TextUtils.isEmpty(applicationName)) {
        return String.format(descriptionWithAppName, applicationName);
      }
    }
    return description;
  }

  @Nullable
  private static IconCompat createIconFromPackage(
      Context context, String packageName, @DrawableRes int iconRes) {
    try {
      Context packageContext = context.createPackageContext(packageName, 0);
      return IconCompat.createWithResource(packageContext, iconRes);
    } catch (PackageManager.NameNotFoundException e) {
      TcLog.e(TAG, "createIconFromPackage: failed to create package context", e);
    }
    return null;
  }

  private static PendingIntent createPendingIntent(
      final Context context, final Intent intent, int requestCode) {
    return PendingIntent.getActivity(
        context, requestCode, intent, PendingIntent.FLAG_UPDATE_CURRENT);
  }

  @Nullable
  private static String getApplicationName(ResolveInfo resolveInfo, PackageManager packageManager) {
    if (resolveInfo.activityInfo == null) {
      return null;
    }
    if ("android".equals(resolveInfo.activityInfo.packageName)) {
      return null;
    }
    if (resolveInfo.activityInfo.applicationInfo == null) {
      return null;
    }
    return packageManager.getApplicationLabel(resolveInfo.activityInfo.applicationInfo).toString();
  }

  private static boolean hasPermission(Context context, ActivityInfo info) {
    if (!info.exported) {
      return false;
    }
    if (info.permission == null) {
      return true;
    }
    return ContextCompat.checkSelfPermission(context, info.permission)
        == PackageManager.PERMISSION_GRANTED;
  }

  /** Data class that holds the result. */
  public static final class Result {
    public final Intent resolvedIntent;
    public final RemoteActionCompat remoteAction;

    public Result(Intent resolvedIntent, RemoteActionCompat remoteAction) {
      this.resolvedIntent = Preconditions.checkNotNull(resolvedIntent);
      this.remoteAction = Preconditions.checkNotNull(remoteAction);
    }
  }

  /**
   * An object to choose a title from resolved info. If {@code null} is returned, {@link
   * #titleWithEntity} will be used if it exists, {@link #titleWithoutEntity} otherwise.
   */
  public interface TitleChooser {
    /**
     * Picks a title from a {@link LabeledIntent} by looking into resolved info. {@code resolveInfo}
     * is guaranteed to have a non-null {@code activityInfo}.
     */
    @Nullable
    CharSequence chooseTitle(LabeledIntent labeledIntent, ResolveInfo resolveInfo);
  }
}
