package org.robolectric.shadows;

import static android.content.pm.PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
import static android.content.pm.PackageManager.GET_ACTIVITIES;
import static android.content.pm.PackageManager.GET_CONFIGURATIONS;
import static android.content.pm.PackageManager.GET_GIDS;
import static android.content.pm.PackageManager.GET_INSTRUMENTATION;
import static android.content.pm.PackageManager.GET_INTENT_FILTERS;
import static android.content.pm.PackageManager.GET_META_DATA;
import static android.content.pm.PackageManager.GET_PERMISSIONS;
import static android.content.pm.PackageManager.GET_PROVIDERS;
import static android.content.pm.PackageManager.GET_RECEIVERS;
import static android.content.pm.PackageManager.GET_RESOLVED_FILTER;
import static android.content.pm.PackageManager.GET_SERVICES;
import static android.content.pm.PackageManager.GET_SHARED_LIBRARY_FILES;
import static android.content.pm.PackageManager.GET_SIGNATURES;
import static android.content.pm.PackageManager.GET_URI_PERMISSION_PATTERNS;
import static android.content.pm.PackageManager.MATCH_DIRECT_BOOT_AWARE;
import static android.content.pm.PackageManager.MATCH_DIRECT_BOOT_UNAWARE;
import static android.content.pm.PackageManager.MATCH_DISABLED_COMPONENTS;
import static android.content.pm.PackageManager.MATCH_DISABLED_UNTIL_USED_COMPONENTS;
import static android.content.pm.PackageManager.MATCH_UNINSTALLED_PACKAGES;
import static android.content.pm.PackageManager.SIGNATURE_FIRST_NOT_SIGNED;
import static android.content.pm.PackageManager.SIGNATURE_MATCH;
import static android.content.pm.PackageManager.SIGNATURE_NEITHER_SIGNED;
import static android.content.pm.PackageManager.SIGNATURE_NO_MATCH;
import static android.content.pm.PackageManager.SIGNATURE_SECOND_NOT_SIGNED;
import static android.os.Build.VERSION_CODES.JELLY_BEAN_MR1;
import static android.os.Build.VERSION_CODES.KITKAT;
import static android.os.Build.VERSION_CODES.LOLLIPOP_MR1;
import static android.os.Build.VERSION_CODES.M;
import static android.os.Build.VERSION_CODES.N;
import static java.util.Arrays.asList;

import android.Manifest;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentFilter.AuthorityEntry;
import android.content.IntentSender;
import android.content.pm.ApplicationInfo;
import android.content.pm.ComponentInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.IPackageDataObserver;
import android.content.pm.IPackageDeleteObserver;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PackageParser;
import android.content.pm.PackageParser.Component;
import android.content.pm.PackageParser.Package;
import android.content.pm.PackageStats;
import android.content.pm.PackageUserState;
import android.content.pm.PermissionGroupInfo;
import android.content.pm.PermissionInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.PatternMatcher;
import android.os.PersistableBundle;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.ArraySet;
import android.util.Pair;
import com.google.common.base.Preconditions;
import com.google.common.collect.HashMultimap;
import com.google.common.collect.Multimap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.TempDirectory;

@Implements(PackageManager.class)
public class ShadowPackageManager {

  static Map<String, Boolean> permissionRationaleMap = new HashMap<>();
  static List<FeatureInfo> systemAvailableFeatures = new ArrayList<>();
  static final List<String> systemSharedLibraryNames = new ArrayList<>();
  static final Map<String, PackageInfo> packageInfos = new LinkedHashMap<>();
  static final Map<String, Package> packages = new LinkedHashMap<>();
  private static Map<String, PackageInfo> packageArchiveInfo = new HashMap<>();
  static final Map<String, PackageStats> packageStatsMap = new HashMap<>();
  static final Map<String, String> packageInstallerMap = new HashMap<>();
  static final Map<Integer, String[]> packagesForUid = new HashMap<>();
  static final Map<String, Integer> uidForPackage = new HashMap<>();
  static final Map<Integer, List<String>> packagesForUserId = new HashMap<>();
  static final Map<Integer, String> namesForUid = new HashMap<>();
  static final Map<Integer, Integer> verificationResults = new HashMap<>();
  static final Map<Integer, Long> verificationTimeoutExtension = new HashMap<>();
  static final Map<String, String> currentToCanonicalNames = new HashMap<>();
  static final Map<ComponentName, ComponentState> componentList = new LinkedHashMap<>();
  static final Map<ComponentName, Drawable> drawableList = new LinkedHashMap<>();
  static final Map<String, Drawable> applicationIcons = new HashMap<>();
  static final Map<String, Drawable> unbadgedApplicationIcons = new HashMap<>();
  static final Map<String, Boolean> systemFeatureList = new LinkedHashMap<>();
  static final Map<IntentFilterWrapper, ComponentName> preferredActivities = new LinkedHashMap<>();
  static final Map<Pair<String, Integer>, Drawable> drawables = new LinkedHashMap<>();
  static final Map<String, Integer> applicationEnabledSettingMap = new HashMap<>();
  static Map<String, PermissionInfo> extraPermissions = new HashMap<>();
  static Map<String, PermissionGroupInfo> extraPermissionGroups = new HashMap<>();
  public static Map<String, Resources> resources = new HashMap<>();
  private static final Map<Intent, List<ResolveInfo>> resolveInfoForIntent =
      new TreeMap<>(new IntentComparator());
  private static Set<String> deletedPackages = new HashSet<>();
  static Map<String, IPackageDeleteObserver> pendingDeleteCallbacks = new HashMap<>();
  static Set<String> hiddenPackages = new HashSet<>();
  static Multimap<Integer, String> sequenceNumberChangedPackagesMap = HashMultimap.create();
  static boolean canRequestPackageInstalls = false;

  /**
   * Settings for a particular package.
   *
   * <p>This class mirrors {@link com.android.server.pm.PackageSetting}, which is used by {@link
   * PackageManager}.
   */
  public static class PackageSetting {

    /** Whether the package is suspended in {@link PackageManager}. */
    private boolean suspended = false;

    /** The message to be displayed to the user when they try to launch the app. */
    private String dialogMessage = null;

    /** An optional {@link PersistableBundle} shared with the app. */
    private PersistableBundle suspendedAppExtras = null;

    /** An optional {@link PersistableBundle} shared with the launcher. */
    private PersistableBundle suspendedLauncherExtras = null;

    public PackageSetting() {}

    public PackageSetting(PackageSetting that) {
      this.suspended = that.suspended;
      this.dialogMessage = that.dialogMessage;
      this.suspendedAppExtras = deepCopyNullablePersistableBundle(that.suspendedAppExtras);
      this.suspendedLauncherExtras =
          deepCopyNullablePersistableBundle(that.suspendedLauncherExtras);
    }

    /**
     * Sets the suspension state of the package.
     *
     * <p>If {@code suspended} is false, {@code dialogMessage}, {@code appExtras}, and {@code
     * launcherExtras} will be ignored.
     */
    void setSuspended(
        boolean suspended,
        String dialogMessage,
        PersistableBundle appExtras,
        PersistableBundle launcherExtras) {
      this.suspended = suspended;
      this.dialogMessage = suspended ? dialogMessage : null;
      this.suspendedAppExtras = suspended ? deepCopyNullablePersistableBundle(appExtras) : null;
      this.suspendedLauncherExtras =
          suspended ? deepCopyNullablePersistableBundle(launcherExtras) : null;
    }

    public boolean isSuspended() {
      return suspended;
    }

    public String getDialogMessage() {
      return dialogMessage;
    }

    public PersistableBundle getSuspendedAppExtras() {
      return suspendedAppExtras;
    }

    public PersistableBundle getSuspendedLauncherExtras() {
      return suspendedLauncherExtras;
    }

    private static PersistableBundle deepCopyNullablePersistableBundle(PersistableBundle bundle) {
      return bundle == null ? null : bundle.deepCopy();
    }
  }

  static final Map<String, PackageSetting> packageSettings = new HashMap<>();

  // From com.android.server.pm.PackageManagerService.compareSignatures().
  static int compareSignature(Signature[] signatures1, Signature[] signatures2) {
    if (signatures1 == null) {
      return (signatures2 == null) ? SIGNATURE_NEITHER_SIGNED : SIGNATURE_FIRST_NOT_SIGNED;
    }
    if (signatures2 == null) {
      return SIGNATURE_SECOND_NOT_SIGNED;
    }
    if (signatures1.length != signatures2.length) {
      return SIGNATURE_NO_MATCH;
    }
    HashSet<Signature> signatures1set = new HashSet<>(asList(signatures1));
    HashSet<Signature> signatures2set = new HashSet<>(asList(signatures2));
    return signatures1set.equals(signatures2set) ? SIGNATURE_MATCH : SIGNATURE_NO_MATCH;
  }

  static String resolvePackageName(String packageName, ComponentName componentName) {
    String classString = componentName.getClassName();
    int index = classString.indexOf('.');
    if (index == -1) {
      classString = packageName + "." + classString;
    } else if (index == 0) {
      classString = packageName + classString;
    }
    return classString;
  }

  // TODO(christianw): reconcile with ParallelUniverse.setUpPackageStorage
  private static void setUpPackageStorage(ApplicationInfo applicationInfo) {
    TempDirectory tempDirectory = RuntimeEnvironment.getTempDirectory();

    if (applicationInfo.sourceDir == null) {
      applicationInfo.sourceDir =
          tempDirectory
              .createIfNotExists(applicationInfo.packageName + "-sourceDir")
              .toAbsolutePath()
              .toString();
    }

    if (applicationInfo.dataDir == null) {
      applicationInfo.dataDir =
          tempDirectory
              .createIfNotExists(applicationInfo.packageName + "-dataDir")
              .toAbsolutePath()
              .toString();
    }
    if (applicationInfo.publicSourceDir == null) {
      applicationInfo.publicSourceDir = applicationInfo.sourceDir;
    }
    if (RuntimeEnvironment.getApiLevel() >= N) {
      applicationInfo.credentialProtectedDataDir =
          tempDirectory.createIfNotExists("userDataDir").toAbsolutePath().toString();
      applicationInfo.deviceProtectedDataDir =
          tempDirectory.createIfNotExists("deviceDataDir").toAbsolutePath().toString();
    }
  }

  /**
   * Sets extra resolve infos for an intent.
   *
   * <p>Those entries are added to whatever might be in the manifest already.
   *
   * <p>Note that all resolve infos will have {@link ResolveInfo#isDefault} field set to {@code
   * true} to allow their resolution for implicit intents. If this is not what you want, then you
   * still have the reference to those ResolveInfos, and you can set the field back to {@code
   * false}.
   */
  public void setResolveInfosForIntent(Intent intent, List<ResolveInfo> info) {
    resolveInfoForIntent.remove(intent);
    for (ResolveInfo resolveInfo : info) {
      addResolveInfoForIntent(intent, resolveInfo);
    }
  }

  /**
   * @deprecated please use {@link #setResolveInfosForIntent} or {@link
   *     #addResolveInfoForIntent(Intent, ResolveInfo)} instead.
   */
  @Deprecated
  public void addResolveInfoForIntent(Intent intent, List<ResolveInfo> info) {
    setResolveInfosForIntent(intent, info);
  }

  /**
   * Adds extra resolve info for an intent.
   *
   * <p>Note that this resolve info will have {@link ResolveInfo#isDefault} field set to {@code
   * true} to allow its resolution for implicit intents. If this is not what you want, then please
   * use {@link #addResolveInfoForIntentNoDefaults} instead.
   */
  public void addResolveInfoForIntent(Intent intent, ResolveInfo info) {
    info.isDefault = true;
    ComponentInfo[] componentInfos =
        new ComponentInfo[] {
          info.activityInfo,
          info.serviceInfo,
          Build.VERSION.SDK_INT >= KITKAT ? info.providerInfo : null
        };
    for (ComponentInfo component : componentInfos) {
      if (component != null && component.applicationInfo != null) {
        component.applicationInfo.flags |= ApplicationInfo.FLAG_INSTALLED;
      }
    }
    addResolveInfoForIntentNoDefaults(intent, info);
  }

  /**
   * Adds the {@code info} as {@link ResolveInfo} for the intent but without applying any default
   * values.
   *
   * <p>In particular it will not make the {@link ResolveInfo#isDefault} field {@code true}, that
   * means that this resolve info will not resolve for {@link Intent#resolveActivity} and {@link
   * Context#startActivity}.
   */
  public void addResolveInfoForIntentNoDefaults(Intent intent, ResolveInfo info) {
    Preconditions.checkNotNull(info);
    List<ResolveInfo> infoList = resolveInfoForIntent.get(intent);
    if (infoList == null) {
      infoList = new ArrayList<>();
      resolveInfoForIntent.put(intent, infoList);
    }
    infoList.add(info);
  }

  public void removeResolveInfosForIntent(Intent intent, String packageName) {
    List<ResolveInfo> infoList = resolveInfoForIntent.get(intent);
    if (infoList == null) {
      infoList = new ArrayList<>();
      resolveInfoForIntent.put(intent, infoList);
    }

    for (Iterator<ResolveInfo> iterator = infoList.iterator(); iterator.hasNext(); ) {
      ResolveInfo resolveInfo = iterator.next();
      if (getPackageName(resolveInfo).equals(packageName)) {
        iterator.remove();
      }
    }
  }

  private static String getPackageName(ResolveInfo resolveInfo) {
    if (resolveInfo.resolvePackageName != null) {
      return resolveInfo.resolvePackageName;
    } else if (resolveInfo.activityInfo != null) {
      return resolveInfo.activityInfo.packageName;
    } else if (resolveInfo.serviceInfo != null) {
      return resolveInfo.serviceInfo.packageName;
    } else if (resolveInfo.providerInfo != null) {
      return resolveInfo.providerInfo.packageName;
    }
    throw new IllegalStateException(
        "Could not find package name for ResolveInfo " + resolveInfo.toString());
  }

  public void addActivityIcon(ComponentName component, Drawable drawable) {
    drawableList.put(component, drawable);
  }

  public void addActivityIcon(Intent intent, Drawable drawable) {
    drawableList.put(intent.getComponent(), drawable);
  }

  public void setApplicationIcon(String packageName, Drawable drawable) {
    applicationIcons.put(packageName, drawable);
  }

  public void setUnbadgedApplicationIcon(String packageName, Drawable drawable) {
    unbadgedApplicationIcons.put(packageName, drawable);
  }

  /**
   * Return the flags set in call to {@link
   * android.app.ApplicationPackageManager#setComponentEnabledSetting(ComponentName, int, int)}.
   *
   * @param componentName The component name.
   * @return The flags.
   */
  public int getComponentEnabledSettingFlags(ComponentName componentName) {
    ComponentState state = componentList.get(componentName);
    return state != null ? state.flags : 0;
  }

  /**
   * Installs a package with the {@link PackageManager}.
   *
   * <p>In order to create PackageInfo objects in a valid state please use {@link
   * androidx.test.core.content.pm.PackageInfoBuilder}.
   *
   * <p>This method automatically simulates instalation of a package in the system, so it adds a
   * flag {@link ApplicationInfo#FLAG_INSTALLED} to the application info and makes sure it exits. It
   * will update applicationInfo in package components as well.
   *
   * <p>If you don't want the package to be installed, use {@link #addPackageNoDefaults} instead.
   */
  public void installPackage(PackageInfo packageInfo) {
    ApplicationInfo appInfo = packageInfo.applicationInfo;
    if (appInfo == null) {
      appInfo = new ApplicationInfo();
      appInfo.packageName = packageInfo.packageName;
      packageInfo.applicationInfo = appInfo;
    }
    appInfo.flags |= ApplicationInfo.FLAG_INSTALLED;
    ComponentInfo[][] componentInfoArrays =
        new ComponentInfo[][] {
          packageInfo.activities,
          packageInfo.services,
          packageInfo.providers,
          packageInfo.receivers,
        };
    for (ComponentInfo[] componentInfos : componentInfoArrays) {
      if (componentInfos == null) {
        continue;
      }
      for (ComponentInfo componentInfo : componentInfos) {
        if (componentInfo.applicationInfo == null) {
          componentInfo.applicationInfo = appInfo;
        }
        componentInfo.applicationInfo.flags |= ApplicationInfo.FLAG_INSTALLED;
      }
    }
    addPackageNoDefaults(packageInfo);
  }

  /**
   * Adds a package to the {@link PackageManager}, but doesn't set any default values on it.
   *
   * <p>Right now it will not set {@link ApplicationInfo#FLAG_INSTALLED} flag on its application, so
   * if not set explicitly, it will be treated as not installed.
   */
  public void addPackageNoDefaults(PackageInfo packageInfo) {
    PackageStats packageStats = new PackageStats(packageInfo.packageName);
    addPackage(packageInfo, packageStats);
  }

  /**
   * Installs a package with its stats with the {@link PackageManager}.
   *
   * <p>This method doesn't add any defaults to the {@code packageInfo} parameters. You should make
   * sure it is valid (see {@link #installPackage(PackageInfo)}).
   */
  public synchronized void addPackage(PackageInfo packageInfo, PackageStats packageStats) {
    Preconditions.checkArgument(packageInfo.packageName.equals(packageStats.packageName));

    packageInfos.put(packageInfo.packageName, packageInfo);
    packageStatsMap.put(packageInfo.packageName, packageStats);

    packageSettings.put(packageInfo.packageName, new PackageSetting());

    applicationEnabledSettingMap.put(
        packageInfo.packageName, PackageManager.COMPONENT_ENABLED_STATE_DEFAULT);
    if (packageInfo.applicationInfo != null) {
      namesForUid.put(packageInfo.applicationInfo.uid, packageInfo.packageName);
    }
  }

  /** @deprecated Use {@link #installPackage(PackageInfo)} instead. */
  @Deprecated
  public void addPackage(String packageName) {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = packageName;

    ApplicationInfo applicationInfo = new ApplicationInfo();

    applicationInfo.packageName = packageName;
    // TODO: setUpPackageStorage should be in installPackage but we need to fix all tests first
    setUpPackageStorage(applicationInfo);
    packageInfo.applicationInfo = applicationInfo;
    installPackage(packageInfo);
  }

  /** This method is getting renamed to {link {@link #installPackage}. */
  @Deprecated
  public void addPackage(PackageInfo packageInfo) {
    installPackage(packageInfo);
  }

  /**
   * Testing API allowing to retrieve internal package representation.
   *
   * <p>This will allow to modify the package in a way visible to Robolectric, as this is
   * Robolectric's internal full package representation.
   *
   * <p>Note that maybe a better way is to just modify the test manifest to make those modifications
   * in a standard way.
   *
   * <p>Retrieving package info using {@link PackageManager#getPackageInfo} / {@link
   * PackageManager#getApplicationInfo} will return defensive copies that will be stripped out of
   * information according to provided flags. Don't use it to modify Robolectric state.
   */
  public PackageInfo getInternalMutablePackageInfo(String packageName) {
    return packageInfos.get(packageName);
  }

  /** @deprecated Use {@link #getInternalMutablePackageInfo} instead. It has better name. */
  @Deprecated
  public PackageInfo getPackageInfoForTesting(String packageName) {
    return getInternalMutablePackageInfo(packageName);
  }

  public void addPermissionInfo(PermissionInfo permissionInfo) {
    extraPermissions.put(permissionInfo.name, permissionInfo);
  }

  /**
   * Adds {@code packageName} to the list of changed packages for the particular {@code
   * sequenceNumber}.
   *
   * @param sequenceNumber has to be >= 0
   * @param packageName name of the package that was changed
   */
  public void addChangedPackage(int sequenceNumber, String packageName) {
    if (sequenceNumber < 0) {
      return;
    }
    sequenceNumberChangedPackagesMap.put(sequenceNumber, packageName);
  }

  /**
   * Allows overriding or adding permission-group elements. These would be otherwise specified by
   * either (the
   * system)[https://developer.android.com/guide/topics/permissions/requesting.html#perm-groups] or
   * by (the app
   * itself)[https://developer.android.com/guide/topics/manifest/permission-group-element.html], as
   * part of its manifest
   *
   * <p>{@link android.content.pm.PackageParser.PermissionGroup}s added through this method have
   * precedence over those specified with the same name by one of the aforementioned methods.
   *
   * @see PackageManager#getAllPermissionGroups(int)
   * @see PackageManager#getPermissionGroupInfo(String, int)
   */
  public void addPermissionGroupInfo(PermissionGroupInfo permissionGroupInfo) {
    extraPermissionGroups.put(permissionGroupInfo.name, permissionGroupInfo);
  }

  public void removePackage(String packageName) {
    packages.remove(packageName);
    packageInfos.remove(packageName);

    packageSettings.remove(packageName);
  }

  public void setSystemFeature(String name, boolean supported) {
    systemFeatureList.put(name, supported);
  }

  public void addDrawableResolution(String packageName, int resourceId, Drawable drawable) {
    drawables.put(new Pair(packageName, resourceId), drawable);
  }

  public void setNameForUid(int uid, String name) {
    namesForUid.put(uid, name);
  }

  public void setPackagesForCallingUid(String... packagesForCallingUid) {
    packagesForUid.put(Binder.getCallingUid(), packagesForCallingUid);
    for (String packageName : packagesForCallingUid) {
      uidForPackage.put(packageName, Binder.getCallingUid());
    }
  }

  public void setPackagesForUid(int uid, String... packagesForCallingUid) {
    packagesForUid.put(uid, packagesForCallingUid);
    for (String packageName : packagesForCallingUid) {
      uidForPackage.put(packageName, uid);
    }
  }

  @Implementation
  @Nullable
  protected String[] getPackagesForUid(int uid) {
    return packagesForUid.get(uid);
  }

  public void setInstalledPackagesForUserId(int userId, List<String> packages) {
    packagesForUserId.put(userId, packages);
    for (String packageName : packages) {
      addPackage(packageName);
    }
  }

  public void setPackageArchiveInfo(String archiveFilePath, PackageInfo packageInfo) {
    packageArchiveInfo.put(archiveFilePath, packageInfo);
  }

  public int getVerificationResult(int id) {
    Integer result = verificationResults.get(id);
    if (result == null) {
      // 0 isn't a "valid" result, so we can check for the case when verification isn't
      // called, if needed
      return 0;
    }
    return result;
  }

  public long getVerificationExtendedTimeout(int id) {
    Long result = verificationTimeoutExtension.get(id);
    if (result == null) {
      return 0;
    }
    return result;
  }

  public void setShouldShowRequestPermissionRationale(String permission, boolean show) {
    permissionRationaleMap.put(permission, show);
  }

  public void addSystemAvailableFeature(FeatureInfo featureInfo) {
    systemAvailableFeatures.add(featureInfo);
  }

  public void clearSystemAvailableFeatures() {
    systemAvailableFeatures.clear();
  }

  /** Adds a value to be returned by {@link PackageManager#getSystemSharedLibraryNames()}. */
  public void addSystemSharedLibraryName(String name) {
    systemSharedLibraryNames.add(name);
  }

  /** Clears the values returned by {@link PackageManager#getSystemSharedLibraryNames()}. */
  public void clearSystemSharedLibraryNames() {
    systemSharedLibraryNames.clear();
  }

  public void addCurrentToCannonicalName(String currentName, String canonicalName) {
    currentToCanonicalNames.put(currentName, canonicalName);
  }

  /**
   * Sets if the {@link PackageManager} is allowed to request package installs through package
   * installer.
   */
  public void setCanRequestPackageInstalls(boolean canRequestPackageInstalls) {
    ShadowPackageManager.canRequestPackageInstalls = canRequestPackageInstalls;
  }

  @Implementation(minSdk = N)
  protected List<ResolveInfo> queryBroadcastReceiversAsUser(
      Intent intent, int flags, UserHandle userHandle) {
    return null;
  }

  @Implementation(minSdk = JELLY_BEAN_MR1)
  protected List<ResolveInfo> queryBroadcastReceivers(
      Intent intent, int flags, @UserIdInt int userId) {
    return null;
  }

  @Implementation
  protected PackageInfo getPackageArchiveInfo(String archiveFilePath, int flags) {
    List<PackageInfo> result = new ArrayList<>();
    for (PackageInfo packageInfo : packageInfos.values()) {
      if (applicationEnabledSettingMap.get(packageInfo.packageName)
              != COMPONENT_ENABLED_STATE_DISABLED
          || (flags & MATCH_UNINSTALLED_PACKAGES) == MATCH_UNINSTALLED_PACKAGES) {
        result.add(packageInfo);
      }
    }

    List<PackageInfo> packages = result;
    for (PackageInfo aPackage : packages) {
      ApplicationInfo appInfo = aPackage.applicationInfo;
      if (appInfo != null && archiveFilePath.equals(appInfo.sourceDir)) {
        return aPackage;
      }
    }
    return null;
  }

  @Implementation
  protected void freeStorageAndNotify(long freeStorageSize, IPackageDataObserver observer) {}

  @Implementation
  protected void freeStorage(long freeStorageSize, IntentSender pi) {}

  /**
   * Runs the callbacks pending from calls to {@link PackageManager#deletePackage(String,
   * IPackageDeleteObserver, int)}
   */
  public void doPendingUninstallCallbacks() {
    boolean hasDeletePackagesPermission = false;
    String[] requestedPermissions =
        packageInfos.get(RuntimeEnvironment.application.getPackageName()).requestedPermissions;
    if (requestedPermissions != null) {
      for (String permission : requestedPermissions) {
        if (Manifest.permission.DELETE_PACKAGES.equals(permission)) {
          hasDeletePackagesPermission = true;
          break;
        }
      }
    }

    for (String packageName : pendingDeleteCallbacks.keySet()) {
      int resultCode = PackageManager.DELETE_FAILED_INTERNAL_ERROR;

      PackageInfo removed = packageInfos.get(packageName);
      if (hasDeletePackagesPermission && removed != null) {
        packageInfos.remove(packageName);

        packageSettings.remove(packageName);

        deletedPackages.add(packageName);
        resultCode = PackageManager.DELETE_SUCCEEDED;
      }

      try {
        pendingDeleteCallbacks.get(packageName).packageDeleted(packageName, resultCode);
      } catch (RemoteException e) {
        throw new RuntimeException(e);
      }
    }
    pendingDeleteCallbacks.clear();
  }

  /**
   * Returns package names successfully deleted with {@link PackageManager#deletePackage(String,
   * IPackageDeleteObserver, int)} Note that like real {@link PackageManager} the calling context
   * must have {@link android.Manifest.permission#DELETE_PACKAGES} permission set.
   */
  public Set<String> getDeletedPackages() {
    return deletedPackages;
  }

  protected List<ResolveInfo> queryOverriddenIntents(Intent intent, int flags) {
    List<ResolveInfo> overrides = resolveInfoForIntent.get(intent);
    if (overrides == null) {
      return Collections.emptyList();
    }
    List<ResolveInfo> result = new ArrayList<>(overrides.size());
    for (ResolveInfo resolveInfo : overrides) {
      result.add(ShadowResolveInfo.newResolveInfo(resolveInfo));
    }
    return result;
  }

  /**
   * Internal use only.
   *
   * @param appPackage
   */
  public void addPackageInternal(Package appPackage) {
    int flags =
        GET_ACTIVITIES
            | GET_RECEIVERS
            | GET_SERVICES
            | GET_PROVIDERS
            | GET_INSTRUMENTATION
            | GET_INTENT_FILTERS
            | GET_SIGNATURES
            | GET_RESOLVED_FILTER
            | GET_META_DATA
            | GET_GIDS
            | MATCH_DISABLED_COMPONENTS
            | GET_SHARED_LIBRARY_FILES
            | GET_URI_PERMISSION_PATTERNS
            | GET_PERMISSIONS
            | MATCH_UNINSTALLED_PACKAGES
            | GET_CONFIGURATIONS
            | MATCH_DISABLED_UNTIL_USED_COMPONENTS
            | MATCH_DIRECT_BOOT_UNAWARE
            | MATCH_DIRECT_BOOT_AWARE;

    packages.put(appPackage.packageName, appPackage);
    PackageInfo packageInfo;
    if (RuntimeEnvironment.getApiLevel() >= M) {
      packageInfo =
          PackageParser.generatePackageInfo(
              appPackage,
              new int[] {0},
              flags,
              0,
              0,
              new HashSet<String>(),
              new PackageUserState());
    } else if (RuntimeEnvironment.getApiLevel() >= LOLLIPOP_MR1) {
      packageInfo =
          ReflectionHelpers.callStaticMethod(
              PackageParser.class,
              "generatePackageInfo",
              ReflectionHelpers.ClassParameter.from(Package.class, appPackage),
              ReflectionHelpers.ClassParameter.from(int[].class, new int[] {0}),
              ReflectionHelpers.ClassParameter.from(int.class, flags),
              ReflectionHelpers.ClassParameter.from(long.class, 0L),
              ReflectionHelpers.ClassParameter.from(long.class, 0L),
              ReflectionHelpers.ClassParameter.from(ArraySet.class, new ArraySet<>()),
              ReflectionHelpers.ClassParameter.from(
                  PackageUserState.class, new PackageUserState()));
    } else if (RuntimeEnvironment.getApiLevel() >= JELLY_BEAN_MR1) {
      packageInfo =
          ReflectionHelpers.callStaticMethod(
              PackageParser.class,
              "generatePackageInfo",
              ReflectionHelpers.ClassParameter.from(Package.class, appPackage),
              ReflectionHelpers.ClassParameter.from(int[].class, new int[] {0}),
              ReflectionHelpers.ClassParameter.from(int.class, flags),
              ReflectionHelpers.ClassParameter.from(long.class, 0L),
              ReflectionHelpers.ClassParameter.from(long.class, 0L),
              ReflectionHelpers.ClassParameter.from(HashSet.class, new HashSet<>()),
              ReflectionHelpers.ClassParameter.from(
                  PackageUserState.class, new PackageUserState()));
    } else {
      packageInfo =
          ReflectionHelpers.callStaticMethod(
              PackageParser.class,
              "generatePackageInfo",
              ReflectionHelpers.ClassParameter.from(Package.class, appPackage),
              ReflectionHelpers.ClassParameter.from(int[].class, new int[] {0}),
              ReflectionHelpers.ClassParameter.from(int.class, flags),
              ReflectionHelpers.ClassParameter.from(long.class, 0L),
              ReflectionHelpers.ClassParameter.from(long.class, 0L),
              ReflectionHelpers.ClassParameter.from(HashSet.class, new HashSet<>()));
    }

    packageInfo.applicationInfo.uid = Process.myUid();
    packageInfo.applicationInfo.dataDir =
        RuntimeEnvironment.getTempDirectory()
            .createIfNotExists(packageInfo.packageName + "-dataDir")
            .toString();
    installPackage(packageInfo);
  }

  public static class IntentComparator implements Comparator<Intent> {

    @Override
    public int compare(Intent i1, Intent i2) {
      if (i1 == null && i2 == null) return 0;
      if (i1 == null && i2 != null) return -1;
      if (i1 != null && i2 == null) return 1;
      if (i1.equals(i2)) return 0;
      String action1 = i1.getAction();
      String action2 = i2.getAction();
      if (action1 == null && action2 != null) return -1;
      if (action1 != null && action2 == null) return 1;
      if (action1 != null && action2 != null) {
        if (!action1.equals(action2)) {
          return action1.compareTo(action2);
        }
      }
      Uri data1 = i1.getData();
      Uri data2 = i2.getData();
      if (data1 == null && data2 != null) return -1;
      if (data1 != null && data2 == null) return 1;
      if (data1 != null && data2 != null) {
        if (!data1.equals(data2)) {
          return data1.compareTo(data2);
        }
      }
      ComponentName component1 = i1.getComponent();
      ComponentName component2 = i2.getComponent();
      if (component1 == null && component2 != null) return -1;
      if (component1 != null && component2 == null) return 1;
      if (component1 != null && component2 != null) {
        if (!component1.equals(component2)) {
          return component1.compareTo(component2);
        }
      }
      String package1 = i1.getPackage();
      String package2 = i2.getPackage();
      if (package1 == null && package2 != null) return -1;
      if (package1 != null && package2 == null) return 1;
      if (package1 != null && package2 != null) {
        if (!package1.equals(package2)) {
          return package1.compareTo(package2);
        }
      }
      Set<String> categories1 = i1.getCategories();
      Set<String> categories2 = i2.getCategories();
      if (categories1 == null) return categories2 == null ? 0 : -1;
      if (categories2 == null) return 1;
      if (categories1.size() > categories2.size()) return 1;
      if (categories1.size() < categories2.size()) return -1;
      String[] array1 = categories1.toArray(new String[0]);
      String[] array2 = categories2.toArray(new String[0]);
      Arrays.sort(array1);
      Arrays.sort(array2);
      for (int i = 0; i < array1.length; ++i) {
        int val = array1[i].compareTo(array2[i]);
        if (val != 0) return val;
      }
      return 0;
    }
  }

  /**
   * This class wraps {@link IntentFilter} so it has reasonable {@link #equals} and {@link
   * #hashCode} methods.
   */
  protected static class IntentFilterWrapper {
    final IntentFilter filter;
    private final HashSet<String> actions = new HashSet<>();
    private HashSet<String> categories = new HashSet<>();
    private HashSet<String> dataSchemes = new HashSet<>();
    private HashSet<String> dataSchemeSpecificParts = new HashSet<>();
    private HashSet<String> dataAuthorities = new HashSet<>();
    private HashSet<String> dataPaths = new HashSet<>();
    private HashSet<String> dataTypes = new HashSet<>();

    public IntentFilterWrapper(IntentFilter filter) {
      this.filter = filter;
      if (filter == null) {
        return;
      }
      for (int i = 0; i < filter.countActions(); i++) {
        actions.add(filter.getAction(i));
      }
      for (int i = 0; i < filter.countCategories(); i++) {
        categories.add(filter.getCategory(i));
      }
      for (int i = 0; i < filter.countDataAuthorities(); i++) {
        AuthorityEntry dataAuthority = filter.getDataAuthority(i);
        dataAuthorities.add(dataAuthority.getHost() + ":" + dataAuthority.getPort());
      }
      for (int i = 0; i < filter.countDataPaths(); i++) {
        PatternMatcher dataPath = filter.getDataPath(i);
        dataPaths.add(dataPath.toString());
      }
      for (int i = 0; i < filter.countDataSchemes(); i++) {
        dataSchemes.add(filter.getDataScheme(i));
      }
      if (VERSION.SDK_INT >= KITKAT) {
        for (int i = 0; i < filter.countDataSchemeSpecificParts(); i++) {
          dataSchemeSpecificParts.add(filter.getDataSchemeSpecificPart(i).toString());
        }
      }
      for (int i = 0; i < filter.countDataTypes(); i++) {
        dataTypes.add(filter.getDataType(i));
      }
    }

    @Override
    public boolean equals(Object o) {
      if (this == o) {
        return true;
      }
      if (!(o instanceof IntentFilterWrapper)) {
        return false;
      }
      IntentFilterWrapper that = (IntentFilterWrapper) o;
      if (filter == null && that.filter == null) {
        return true;
      }
      if (filter == null || that.filter == null) {
        return false;
      }
      return filter.getPriority() == that.filter.getPriority()
          && Objects.equals(actions, that.actions)
          && Objects.equals(categories, that.categories)
          && Objects.equals(dataSchemes, that.dataSchemes)
          && Objects.equals(dataSchemeSpecificParts, that.dataSchemeSpecificParts)
          && Objects.equals(dataAuthorities, that.dataAuthorities)
          && Objects.equals(dataPaths, that.dataPaths)
          && Objects.equals(dataTypes, that.dataTypes);
    }

    @Override
    public int hashCode() {
      return Objects.hash(
          filter == null ? null : filter.getPriority(),
          actions,
          categories,
          dataSchemes,
          dataSchemeSpecificParts,
          dataAuthorities,
          dataPaths,
          dataTypes);
    }

    public IntentFilter getFilter() {
      return filter;
    }
  }

  /** Compares {@link ResolveInfo}, where better is bigger. */
  static class ResolveInfoComparator implements Comparator<ResolveInfo> {

    private final HashSet<ComponentName> preferredComponents;

    public ResolveInfoComparator(HashSet<ComponentName> preferredComponents) {
      this.preferredComponents = preferredComponents;
    }

    @Override
    public int compare(ResolveInfo o1, ResolveInfo o2) {
      if (o1 == null && o2 == null) {
        return 0;
      }
      if (o1 == null) {
        return -1;
      }
      if (o2 == null) {
        return 1;
      }
      boolean o1isPreferred = isPreferred(o1);
      boolean o2isPreferred = isPreferred(o2);
      if (o1isPreferred != o2isPreferred) {
        return Boolean.compare(o1isPreferred, o2isPreferred);
      }
      if (o1.preferredOrder != o2.preferredOrder) {
        return Integer.compare(o1.preferredOrder, o2.preferredOrder);
      }
      if (o1.priority != o2.priority) {
        return Integer.compare(o1.priority, o2.priority);
      }
      return 0;
    }

    private boolean isPreferred(ResolveInfo resolveInfo) {
      return resolveInfo.activityInfo != null
          && resolveInfo.activityInfo.packageName != null
          && resolveInfo.activityInfo.name != null
          && preferredComponents.contains(
              new ComponentName(
                  resolveInfo.activityInfo.packageName, resolveInfo.activityInfo.name));
    }
  }

  protected static class ComponentState {
    public int newState;
    public int flags;

    public ComponentState(int newState, int flags) {
      this.newState = newState;
      this.flags = flags;
    }
  }

  /**
   * Get list of intent filters defined for given activity.
   *
   * @param componentName Name of the activity whose intent filters are to be retrieved
   * @return the activity's intent filters
   */
  public List<IntentFilter> getIntentFiltersForActivity(ComponentName componentName)
      throws NameNotFoundException {
    return getIntentFiltersForComponent(getAppPackage(componentName).activities, componentName);
  }

  /**
   * Get list of intent filters defined for given service.
   *
   * @param componentName Name of the service whose intent filters are to be retrieved
   * @return the service's intent filters
   */
  public List<IntentFilter> getIntentFiltersForService(ComponentName componentName)
      throws NameNotFoundException {
    return getIntentFiltersForComponent(getAppPackage(componentName).services, componentName);
  }

  /**
   * Get list of intent filters defined for given receiver.
   *
   * @param componentName Name of the receiver whose intent filters are to be retrieved
   * @return the receiver's intent filters
   */
  public List<IntentFilter> getIntentFiltersForReceiver(ComponentName componentName)
      throws NameNotFoundException {
    return getIntentFiltersForComponent(getAppPackage(componentName).receivers, componentName);
  }

  private static List<IntentFilter> getIntentFiltersForComponent(
      List<? extends Component> components, ComponentName componentName)
      throws NameNotFoundException {
    for (Component component : components) {
      if (component.getComponentName().equals(componentName)) {
        return component.intents;
      }
    }
    throw new NameNotFoundException("unknown component " + componentName);
  }

  private static Package getAppPackage(ComponentName componentName) throws NameNotFoundException {
    Package appPackage = packages.get(componentName.getPackageName());
    if (appPackage == null) {
      throw new NameNotFoundException("unknown package " + componentName.getPackageName());
    }
    return appPackage;
  }

  /**
   * Returns the current {@link PackageSetting} of {@code packageName}.
   *
   * <p>If {@code packageName} is not present in this {@link ShadowPackageManager}, this method will
   * return null.
   */
  public PackageSetting getPackageSetting(String packageName) {
    PackageSetting setting = packageSettings.get(packageName);
    return setting == null ? null : new PackageSetting(setting);
  }

  @Resetter
  public static void reset() {
    permissionRationaleMap.clear();
    systemAvailableFeatures.clear();
    systemSharedLibraryNames.clear();
    packageInfos.clear();
    packages.clear();
    packageArchiveInfo.clear();
    packageStatsMap.clear();
    packageInstallerMap.clear();
    packagesForUid.clear();
    uidForPackage.clear();
    namesForUid.clear();
    verificationResults.clear();
    verificationTimeoutExtension.clear();
    currentToCanonicalNames.clear();
    componentList.clear();
    drawableList.clear();
    applicationIcons.clear();
    unbadgedApplicationIcons.clear();
    systemFeatureList.clear();
    preferredActivities.clear();
    drawables.clear();
    applicationEnabledSettingMap.clear();
    extraPermissions.clear();
    extraPermissionGroups.clear();
    resources.clear();
    resolveInfoForIntent.clear();
    deletedPackages.clear();
    pendingDeleteCallbacks.clear();
    hiddenPackages.clear();
    sequenceNumberChangedPackagesMap.clear();
    packagesForUserId.clear();

    packageSettings.clear();
  }
}
