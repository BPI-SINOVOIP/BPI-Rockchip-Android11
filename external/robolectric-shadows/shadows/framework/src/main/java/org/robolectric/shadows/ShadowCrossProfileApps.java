/*
 * Copyright (C) 2020 The Android Open Source Project
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

package org.robolectric.shadows;

import static android.content.pm.PackageManager.MATCH_DIRECT_BOOT_AWARE;
import static android.content.pm.PackageManager.MATCH_DIRECT_BOOT_UNAWARE;
import static android.os.Build.VERSION_CODES.P;
import static android.os.Build.VERSION_CODES.Q;
import static android.os.Build.VERSION_CODES.R;

import static com.google.common.base.Preconditions.checkNotNull;

import android.Manifest;
import android.Manifest.permission;
import android.annotation.NonNull;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.app.AppOpsManager;
import android.app.AppOpsManager.Mode;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.CrossProfileApps;
import android.content.pm.ICrossProfileApps;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Process;
import android.os.UserHandle;
import android.text.TextUtils;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.Iterables;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

import javax.annotation.Nullable;

/** Robolectric implementation of {@link CrossProfileApps}. */
@Implements(value = CrossProfileApps.class, minSdk = P)
public class ShadowCrossProfileApps {

  // BEGIN-INTERNAL
  private final static String INTERACT_ACROSS_PROFILES_APPOP = AppOpsManager.permissionToOp(
          Manifest.permission.INTERACT_ACROSS_PROFILES);
  private static final Set<String> configurableInteractAcrossProfilePackages = new HashSet<>();
  // END-INTERNAL

  private final Set<UserHandle> targetUserProfiles = new LinkedHashSet<>();
  private final List<StartedMainActivity> startedMainActivities = new ArrayList<>();
  private final List<StartedActivity> startedActivities =
          Collections.synchronizedList(new ArrayList<>());
  private final Map<String, Integer> packageNameAppOpModes = new HashMap<>();

  private Context context;
  private PackageManager packageManager;

  @Implementation
  protected void __constructor__(Context context, ICrossProfileApps service) {
    this.context = context;
    this.packageManager = context.getPackageManager();
  }

  /**
   * Returns a list of {@link UserHandle}s currently accessible. This list is populated from calls
   * to {@link #addTargetUserProfile(UserHandle)}.
   */
  @Implementation
  protected List<UserHandle> getTargetUserProfiles() {
    return ImmutableList.copyOf(targetUserProfiles);
  }

  /**
   * Returns a {@link Drawable} that can be shown for profile switching, which is guaranteed to
   * always be the same for a particular user and to be distinct between users.
   */
  @Implementation
  protected Drawable getProfileSwitchingIconDrawable(UserHandle userHandle) {
    verifyCanAccessUser(userHandle);
    return new ColorDrawable(userHandle.getIdentifier());
  }

  /**
   * Returns a {@link CharSequence} that can be shown as a label for profile switching, which is
   * guaranteed to always be the same for a particular user and to be distinct between users.
   */
  @Implementation
  protected CharSequence getProfileSwitchingLabel(UserHandle userHandle) {
    verifyCanAccessUser(userHandle);
    return "Switch to " + userHandle;
  }

  /**
   * Simulates starting the main activity specified in the specified profile, performing the same
   * security checks done by the real {@link CrossProfileApps}.
   *
   * <p>The most recent main activity started can be queried by {@link #peekNextStartedActivity()}
   * ()}.
   */
  @Implementation
  protected void startMainActivity(ComponentName componentName, UserHandle targetUser) {
    verifyCanAccessUser(targetUser);
    verifyActivityInManifest(componentName, /* requireMainActivity= */ true);
    startedMainActivities.add(new StartedMainActivity(componentName, targetUser));
    startedActivities.add(new StartedActivity(componentName, targetUser));
  }

  /**
   * Simulates starting the activity specified in the specified profile, performing the same
   * security checks done by the real {@link CrossProfileApps}.
   *
   * <p>The most recent main activity started can be queried by {@link #peekNextStartedActivity()}
   * ()}.
   */
  @Implementation(minSdk = Q)
  @SystemApi
  @RequiresPermission(permission.INTERACT_ACROSS_PROFILES)
  protected void startActivity(ComponentName componentName, UserHandle targetUser) {
    verifyCanAccessUser(targetUser);
    verifyActivityInManifest(componentName, /* requireMainActivity= */ false);
    verifyHasInteractAcrossProfilesPermission();
    startedActivities.add(new StartedActivity(componentName, targetUser));
  }

  /** Adds {@code userHandle} to the list of accessible handles. */
  public void addTargetUserProfile(UserHandle userHandle) {
    if (userHandle.equals(Process.myUserHandle())) {
      throw new IllegalArgumentException("Cannot target current user");
    }
    targetUserProfiles.add(userHandle);
  }

  /** Removes {@code userHandle} from the list of accessible handles, if present. */
  public void removeTargetUserProfile(UserHandle userHandle) {
    if (userHandle.equals(Process.myUserHandle())) {
      throw new IllegalArgumentException("Cannot target current user");
    }
    targetUserProfiles.remove(userHandle);
  }

  /** Clears the list of accessible handles. */
  public void clearTargetUserProfiles() {
    targetUserProfiles.clear();
  }

  /**
   * Returns the most recent {@link ComponentName}, {@link UserHandle} pair started by {@link
   * CrossProfileApps#startMainActivity(ComponentName, UserHandle)}, wrapped in {@link
   * StartedMainActivity}.
   *
   * @deprecated Use {@link #peekNextStartedActivity()} instead.
   */
  @Nullable
  @Deprecated
  public StartedMainActivity peekNextStartedMainActivity() {
    if (startedMainActivities.isEmpty()) {
      return null;
    } else {
      return Iterables.getLast(startedMainActivities);
    }
  }

  /**
   * Returns the most recent {@link ComponentName}, {@link UserHandle} pair started by {@link
   * CrossProfileApps#startMainActivity(ComponentName, UserHandle)} or {@link
   * CrossProfileApps#startActivity(ComponentName, UserHandle)}, wrapped in {@link StartedActivity}.
   */
  @Nullable
  public StartedActivity peekNextStartedActivity() {
    if (startedActivities.isEmpty()) {
      return null;
    } else {
      return Iterables.getLast(startedActivities);
    }
  }

  /**
   * Consumes the most recent {@link ComponentName}, {@link UserHandle} pair started by {@link
   * CrossProfileApps#startMainActivity(ComponentName, UserHandle)} or {@link
   * CrossProfileApps#startActivity(ComponentName, UserHandle)}, and returns it wrapped in {@link
   * StartedActivity}.
   */
  @Nullable
  public StartedActivity getNextStartedActivity() {
    if (startedActivities.isEmpty()) {
      return null;
    } else {
      return startedActivities.remove(startedActivities.size() - 1);
    }
  }

  /**
   * Clears all records of {@link StartedActivity}s from calls to {@link
   * CrossProfileApps#startActivity(ComponentName, UserHandle)} or {@link
   * CrossProfileApps#startMainActivity(ComponentName, UserHandle)}.
   */
  public void clearNextStartedActivities() {
    startedActivities.clear();
  }

  private void verifyCanAccessUser(UserHandle userHandle) {
    if (!targetUserProfiles.contains(userHandle)) {
      throw new SecurityException(
              "Not allowed to access "
                      + userHandle
                      + " (did you forget to call addTargetUserProfile?)");
    }
  }

  private void verifyHasInteractAcrossProfilesPermission() {
    if (context.checkSelfPermission(permission.INTERACT_ACROSS_PROFILES)
            != PackageManager.PERMISSION_GRANTED) {
      throw new SecurityException(
              "Attempt to launch activity without required "
                      + permission.INTERACT_ACROSS_PROFILES
                      + " permission");
    }
  }

  /**
   * Ensures that {@code component} is present in the manifest as an exported and enabled activity.
   * This check and the error thrown are the same as the check done by the real {@link
   * CrossProfileApps}.
   *
   * <p>If {@code requireMainActivity} is true, then this also asserts that the activity is a
   * launcher activity.
   */
  private void verifyActivityInManifest(ComponentName component, boolean requireMainActivity) {
    Intent launchIntent = new Intent();
    if (requireMainActivity) {
      launchIntent
              .setAction(Intent.ACTION_MAIN)
              .addCategory(Intent.CATEGORY_LAUNCHER)
              .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
              .setPackage(component.getPackageName());
    } else {
      launchIntent.setComponent(component);
    }

    boolean existsMatchingActivity =
            Iterables.any(
                    packageManager.queryIntentActivities(
                            launchIntent, MATCH_DIRECT_BOOT_AWARE | MATCH_DIRECT_BOOT_UNAWARE),
                    resolveInfo -> {
                      ActivityInfo activityInfo = resolveInfo.activityInfo;
                      return TextUtils.equals(activityInfo.packageName, component.getPackageName())
                              && TextUtils.equals(activityInfo.name, component.getClassName())
                              && activityInfo.exported;
                    });
    if (!existsMatchingActivity) {
      throw new SecurityException(
              "Attempt to launch activity without "
                      + " category Intent.CATEGORY_LAUNCHER or activity is not exported"
                      + component);
    }
  }

  // BEGIN-INTERNAL
  @Implementation(minSdk = R)
  @RequiresPermission(
          allOf={android.Manifest.permission.MANAGE_APP_OPS_MODES,
                  android.Manifest.permission.INTERACT_ACROSS_USERS})
  protected void setInteractAcrossProfilesAppOp(String packageName, @Mode int newMode) {
    packageNameAppOpModes.put(packageName, newMode);
  }

  /**
   * Returns the app-op mode associated with the given package name. If not set, returns {@code
   * null}.
   */
  @Nullable
  public @Mode Integer getInteractAcrossProfilesAppOp(String packageName) {
    return packageNameAppOpModes.get(packageName);
  }

  public void addCrossProfilePackage(String packageName){
    configurableInteractAcrossProfilePackages.add(packageName);
  }

  @Implementation(minSdk = R)
  protected void resetInteractAcrossProfilesAppOps(
          @NonNull Collection<String> previousCrossProfilePackages,
          @NonNull Set<String> newCrossProfilePackages) {

    final List<String> unsetCrossProfilePackages =
            previousCrossProfilePackages.stream()
                    .filter(packageName -> !newCrossProfilePackages.contains(packageName))
                    .collect(Collectors.toList());

    for (String packageName : unsetCrossProfilePackages) {
      if (!canConfigureInteractAcrossProfiles(packageName)) {
        setInteractAcrossProfilesAppOp(packageName,
                AppOpsManager.opToDefaultMode(INTERACT_ACROSS_PROFILES_APPOP));
      }
    }
  }

  // BEGIN-INTERNAL
  @Implementation(minSdk = R)
  protected void clearInteractAcrossProfilesAppOps() {
    findAllPackageNames().forEach(
        packageName -> setInteractAcrossProfilesAppOp(
            packageName, AppOpsManager.opToDefaultMode(INTERACT_ACROSS_PROFILES_APPOP)));
  }

  private List<String> findAllPackageNames() {
    return context.getPackageManager()
        .getInstalledApplications(/* flags= */ 0)
        .stream()
        .map(applicationInfo -> applicationInfo.packageName)
        .collect(Collectors.toList());
  }
  // END-INTERNAL

  @Implementation
  protected boolean canConfigureInteractAcrossProfiles(@NonNull String packageName) {
    return configurableInteractAcrossProfilePackages.contains(packageName);
  }

  @Implementation
  protected boolean canUserAttemptToConfigureInteractAcrossProfiles(@NonNull String packageName) {
    PackageInfo packageInfo;
    try {
      packageInfo = packageManager.getPackageInfo(packageName, /* flags= */ 0);
    } catch (PackageManager.NameNotFoundException e) {
      return false;
    }
    if (packageInfo == null || packageInfo.requestedPermissions == null) {
      return false;
    }
    return Arrays.asList(packageInfo.requestedPermissions).contains(
            Manifest.permission.INTERACT_ACROSS_PROFILES);
  }

  @Resetter
  public static void reset() {
    configurableInteractAcrossProfilePackages.clear();
  }
  // END-INTERNAL

  /**
   * Container object to hold parameters passed to {@link #startMainActivity(ComponentName,
   * UserHandle)}.
   *
   * @deprecated Use {@link #peekNextStartedActivity()} and {@link StartedActivity} instead.
   */
  @Deprecated
  public static class StartedMainActivity {

    private final ComponentName componentName;
    private final UserHandle userHandle;

    public StartedMainActivity(ComponentName componentName, UserHandle userHandle) {
      this.componentName = checkNotNull(componentName);
      this.userHandle = checkNotNull(userHandle);
    }

    public ComponentName getComponentName() {
      return componentName;
    }

    public UserHandle getUserHandle() {
      return userHandle;
    }

    @Override
    public boolean equals(Object o) {
      if (this == o) {
        return true;
      }
      if (o == null || getClass() != o.getClass()) {
        return false;
      }
      StartedMainActivity that = (StartedMainActivity) o;
      return Objects.equals(componentName, that.componentName)
              && Objects.equals(userHandle, that.userHandle);
    }

    @Override
    public int hashCode() {
      return Objects.hash(componentName, userHandle);
    }
  }

  /**
   * Container object to hold parameters passed to {@link #startMainActivity(ComponentName,
   * UserHandle)} or {@link #startActivity(ComponentName, UserHandle)}.
   */
  public static final class StartedActivity {

    private final ComponentName componentName;
    private final UserHandle userHandle;

    public StartedActivity(ComponentName componentName, UserHandle userHandle) {
      this.componentName = checkNotNull(componentName);
      this.userHandle = checkNotNull(userHandle);
    }

    public ComponentName getComponentName() {
      return componentName;
    }

    public UserHandle getUserHandle() {
      return userHandle;
    }

    @Override
    public boolean equals(Object o) {
      if (this == o) {
        return true;
      }
      if (o == null || getClass() != o.getClass()) {
        return false;
      }
      StartedActivity that = (StartedActivity) o;
      return Objects.equals(componentName, that.componentName)
              && Objects.equals(userHandle, that.userHandle);
    }

    @Override
    public int hashCode() {
      return Objects.hash(componentName, userHandle);
    }
  }
}