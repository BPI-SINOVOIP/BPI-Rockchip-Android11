package org.robolectric.shadows;

import static android.content.pm.ApplicationInfo.FLAG_ALLOW_BACKUP;
import static android.content.pm.ApplicationInfo.FLAG_ALLOW_CLEAR_USER_DATA;
import static android.content.pm.ApplicationInfo.FLAG_ALLOW_TASK_REPARENTING;
import static android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE;
import static android.content.pm.ApplicationInfo.FLAG_HAS_CODE;
import static android.content.pm.ApplicationInfo.FLAG_RESIZEABLE_FOR_SCREENS;
import static android.content.pm.ApplicationInfo.FLAG_SUPPORTS_LARGE_SCREENS;
import static android.content.pm.ApplicationInfo.FLAG_SUPPORTS_NORMAL_SCREENS;
import static android.content.pm.ApplicationInfo.FLAG_SUPPORTS_SCREEN_DENSITIES;
import static android.content.pm.ApplicationInfo.FLAG_SUPPORTS_SMALL_SCREENS;
import static android.content.pm.ApplicationInfo.FLAG_VM_SAFE_MODE;
import static android.content.pm.PackageInfo.REQUESTED_PERMISSION_GRANTED;
import static android.content.pm.PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
import static android.content.pm.PackageManager.MATCH_DISABLED_COMPONENTS;
import static android.content.pm.PackageManager.MATCH_UNINSTALLED_PACKAGES;
import static android.content.pm.PackageManager.PERMISSION_DENIED;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.content.pm.PackageManager.SIGNATURE_FIRST_NOT_SIGNED;
import static android.content.pm.PackageManager.SIGNATURE_MATCH;
import static android.content.pm.PackageManager.SIGNATURE_NEITHER_SIGNED;
import static android.content.pm.PackageManager.SIGNATURE_NO_MATCH;
import static android.content.pm.PackageManager.SIGNATURE_SECOND_NOT_SIGNED;
import static android.content.pm.PackageManager.SIGNATURE_UNKNOWN_PACKAGE;
import static android.content.pm.PackageManager.VERIFICATION_ALLOW;
import static android.os.Build.VERSION_CODES.JELLY_BEAN_MR1;
import static android.os.Build.VERSION_CODES.LOLLIPOP;
import static android.os.Build.VERSION_CODES.LOLLIPOP_MR1;
import static android.os.Build.VERSION_CODES.M;
import static android.os.Build.VERSION_CODES.N;
import static android.os.Build.VERSION_CODES.N_MR1;
import static android.os.Build.VERSION_CODES.O;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.robolectric.Robolectric.setupActivity;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.Manifest.permission_group;
import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.ChangedPackages;
import android.content.pm.FeatureInfo;
import android.content.pm.IPackageDeleteObserver;
import android.content.pm.IPackageStatsObserver;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PackageParser.Package;
import android.content.pm.PackageParser.PermissionGroup;
import android.content.pm.PackageStats;
import android.content.pm.PathPermission;
import android.content.pm.PermissionGroupInfo;
import android.content.pm.PermissionInfo;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.content.res.XmlResourceParser;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.os.Process;
import android.provider.DocumentsContract;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.common.base.Function;
import com.google.common.collect.Iterables;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import javax.annotation.Nullable;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.R;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager.PackageSetting;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

@RunWith(AndroidJUnit4.class)
public class ShadowPackageManagerTest {

  private static final String TEST_PACKAGE_NAME = "com.some.other.package";
  private static final String TEST_PACKAGE_LABEL = "My Little App";
  private static final String TEST_APP_PATH = "/values/app/application.apk";
  private static final String TEST_PACKAGE2_NAME = "com.a.second.package";
  private static final String TEST_PACKAGE2_LABEL = "A Second App";
  private static final String TEST_APP2_PATH = "/values/app/application2.apk";
  private static final Object USER_ID = 1;
  protected ShadowPackageManager shadowPackageManager;
  @Rule
  public TemporaryFolder temporaryFolder = new TemporaryFolder();
  private PackageManager packageManager;

  private final ArgumentCaptor<PackageStats> packageStatsCaptor = ArgumentCaptor.forClass(PackageStats.class);

  @Before
  public void setUp() {
    packageManager =
        ApplicationProvider.getApplicationContext().getPackageManager();
    shadowPackageManager = shadowOf(packageManager);
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void packageInstallerCreateSession() throws Exception {
    PackageInstaller packageInstaller =
        ApplicationProvider.getApplicationContext()
            .getPackageManager()
            .getPackageInstaller();
    int sessionId = packageInstaller.createSession(createSessionParams("packageName"));

    PackageInstaller.SessionInfo sessionInfo = packageInstaller.getSessionInfo(sessionId);
    assertThat(sessionInfo.isActive()).isTrue();

    assertThat(sessionInfo.appPackageName).isEqualTo("packageName");

    packageInstaller.abandonSession(sessionId);

    assertThat(packageInstaller.getSessionInfo(sessionId)).isNull();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void packageInstallerOpenSession() throws Exception {
    PackageInstaller packageInstaller =
        ApplicationProvider.getApplicationContext()
            .getPackageManager()
            .getPackageInstaller();
    int sessionId = packageInstaller.createSession(createSessionParams("packageName"));

    PackageInstaller.Session session = packageInstaller.openSession(sessionId);

    assertThat(session).isNotNull();
  }

  private static PackageInstaller.SessionParams createSessionParams(String appPackageName) {
    PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
    params.setAppPackageName(appPackageName);
    return params;
  }

  @Test
  public void packageInstallerAndGetPackageArchiveInfo() {
    ApplicationInfo appInfo = new ApplicationInfo();
    appInfo.flags = ApplicationInfo.FLAG_INSTALLED;
    appInfo.packageName = TEST_PACKAGE_NAME;
    appInfo.sourceDir = TEST_APP_PATH;
    appInfo.name = TEST_PACKAGE_LABEL;

    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.applicationInfo = appInfo;
    shadowPackageManager.installPackage(packageInfo);

    PackageInfo packageInfoResult = shadowPackageManager.getPackageArchiveInfo(TEST_APP_PATH, 0);
    assertThat(packageInfoResult).isNotNull();
    ApplicationInfo applicationInfo = packageInfoResult.applicationInfo;
    assertThat(applicationInfo).isInstanceOf(ApplicationInfo.class);
    assertThat(applicationInfo.packageName).isEqualTo(TEST_PACKAGE_NAME);
    assertThat(applicationInfo.sourceDir).isEqualTo(TEST_APP_PATH);
  }

  @Test
  public void applicationFlags() throws Exception {
    int flags = packageManager.getApplicationInfo("org.robolectric", 0).flags;
    assertThat((flags & FLAG_ALLOW_BACKUP)).isEqualTo(FLAG_ALLOW_BACKUP);
    assertThat((flags & FLAG_ALLOW_CLEAR_USER_DATA)).isEqualTo(FLAG_ALLOW_CLEAR_USER_DATA);
    assertThat((flags & FLAG_ALLOW_TASK_REPARENTING)).isEqualTo(FLAG_ALLOW_TASK_REPARENTING);
    assertThat((flags & FLAG_DEBUGGABLE)).isEqualTo(FLAG_DEBUGGABLE);
    assertThat((flags & FLAG_HAS_CODE)).isEqualTo(FLAG_HAS_CODE);
    assertThat((flags & FLAG_RESIZEABLE_FOR_SCREENS)).isEqualTo(FLAG_RESIZEABLE_FOR_SCREENS);
    assertThat((flags & FLAG_SUPPORTS_LARGE_SCREENS)).isEqualTo(FLAG_SUPPORTS_LARGE_SCREENS);
    assertThat((flags & FLAG_SUPPORTS_NORMAL_SCREENS)).isEqualTo(FLAG_SUPPORTS_NORMAL_SCREENS);
    assertThat((flags & FLAG_SUPPORTS_SCREEN_DENSITIES)).isEqualTo(FLAG_SUPPORTS_SCREEN_DENSITIES);
    assertThat((flags & FLAG_SUPPORTS_SMALL_SCREENS)).isEqualTo(FLAG_SUPPORTS_SMALL_SCREENS);
    assertThat((flags & FLAG_VM_SAFE_MODE)).isEqualTo(FLAG_VM_SAFE_MODE);
  }

  /**
   * Tests the permission grants of this test package.
   *
   * <p>These grants are defined in the test package's AndroidManifest.xml.
   */
  @Test
  public void testCheckPermission_thisPackage() throws Exception {
    String thisPackage =
        ApplicationProvider.getApplicationContext().getPackageName();
    assertEquals(PERMISSION_GRANTED, packageManager.checkPermission(
        "android.permission.INTERNET", thisPackage));
    assertEquals(PERMISSION_GRANTED, packageManager.checkPermission(
        "android.permission.SYSTEM_ALERT_WINDOW", thisPackage));
    assertEquals(PERMISSION_GRANTED, packageManager.checkPermission(
        "android.permission.GET_TASKS", thisPackage));

    assertEquals(PERMISSION_DENIED, packageManager.checkPermission(
        "android.permission.ACCESS_FINE_LOCATION", thisPackage));
    assertEquals(PERMISSION_DENIED, packageManager.checkPermission(
        "android.permission.ACCESS_FINE_LOCATION", "random-package"));
  }

  /**
   * Tests the permission grants of other packages. These packages are added to the
   * PackageManager by calling {@link ShadowPackageManager#addPackage}.
   */
  @Test
  public void testCheckPermission_otherPackages() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.requestedPermissions = new String[] {
        "android.permission.INTERNET", "android.permission.SEND_SMS" };
    // Grant one of the permissions.
    packageInfo.requestedPermissionsFlags = new int[] {
        REQUESTED_PERMISSION_GRANTED, 0 /* this permission isn't granted */ };
    shadowPackageManager.installPackage(packageInfo);

    assertEquals(PERMISSION_GRANTED, packageManager.checkPermission(
        "android.permission.INTERNET", TEST_PACKAGE_NAME));
    assertEquals(PERMISSION_DENIED, packageManager.checkPermission(
        "android.permission.SEND_SMS", TEST_PACKAGE_NAME));
    assertEquals(PERMISSION_DENIED, packageManager.checkPermission(
        "android.permission.READ_SMS", TEST_PACKAGE_NAME));
  }

  /**
   * Tests the permission grants of other packages. These packages are added to the
   * PackageManager by calling {@link ShadowPackageManager#addPackage}.
   */
  @Test
  public void testCheckPermission_otherPackages_grantedByDefault() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.requestedPermissions = new String[] {
        "android.permission.INTERNET", "android.permission.SEND_SMS" };
    shadowPackageManager.installPackage(packageInfo);

    // Because we didn't specify permission grant state in the PackageInfo object, all requested
    // permissions are automatically granted. See ShadowPackageManager.grantPermissionsByDefault()
    // for the explanation.
    assertEquals(PERMISSION_GRANTED, packageManager.checkPermission(
        "android.permission.INTERNET", TEST_PACKAGE_NAME));
    assertEquals(PERMISSION_GRANTED, packageManager.checkPermission(
        "android.permission.SEND_SMS", TEST_PACKAGE_NAME));
    assertEquals(PERMISSION_DENIED, packageManager.checkPermission(
        "android.permission.READ_SMS", TEST_PACKAGE_NAME));
  }

  @Test
  @Config(minSdk = M)
  public void testGrantRuntimePermission() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.requestedPermissions =
        new String[] {"android.permission.SEND_SMS", "android.permission.READ_SMS"};
    packageInfo.requestedPermissionsFlags = new int[] {0, 0}; // Not granted by default
    shadowPackageManager.installPackage(packageInfo);

    packageManager.grantRuntimePermission(
        TEST_PACKAGE_NAME, "android.permission.SEND_SMS", Process.myUserHandle());

    assertThat(packageInfo.requestedPermissionsFlags[0]).isEqualTo(REQUESTED_PERMISSION_GRANTED);
    assertThat(packageInfo.requestedPermissionsFlags[1]).isEqualTo(0);

    packageManager.grantRuntimePermission(
        TEST_PACKAGE_NAME, "android.permission.READ_SMS", Process.myUserHandle());

    assertThat(packageInfo.requestedPermissionsFlags[0]).isEqualTo(REQUESTED_PERMISSION_GRANTED);
    assertThat(packageInfo.requestedPermissionsFlags[1]).isEqualTo(REQUESTED_PERMISSION_GRANTED);
  }

  @Test
  @Config(minSdk = M)
  public void testGrantRuntimePermission_packageNotFound() throws Exception {
    try {
      packageManager.grantRuntimePermission(
          "com.unknown.package", "android.permission.SEND_SMS", Process.myUserHandle());
      fail("Exception expected");
    } catch (SecurityException expected) {
    }
  }

  @Test
  @Config(minSdk = M)
  public void testGrantRuntimePermission_doesntRequestPermission() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.requestedPermissions =
        new String[] {"android.permission.SEND_SMS", "android.permission.READ_SMS"};
    packageInfo.requestedPermissionsFlags = new int[] {0, 0}; // Not granted by default
    shadowPackageManager.installPackage(packageInfo);

    try {
      packageManager.grantRuntimePermission(
          // This permission is not granted to the package.
          TEST_PACKAGE_NAME, "android.permission.RECEIVE_SMS", Process.myUserHandle());
      fail("Exception expected");
    } catch (SecurityException expected) {
    }
  }

  @Test
  @Config(minSdk = M)
  public void testRevokeRuntimePermission() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.requestedPermissions =
        new String[] {"android.permission.SEND_SMS", "android.permission.READ_SMS"};
    packageInfo.requestedPermissionsFlags =
        new int[] {REQUESTED_PERMISSION_GRANTED, REQUESTED_PERMISSION_GRANTED};
    shadowPackageManager.installPackage(packageInfo);

    packageManager.revokeRuntimePermission(
        TEST_PACKAGE_NAME, "android.permission.SEND_SMS", Process.myUserHandle());

    assertThat(packageInfo.requestedPermissionsFlags[0]).isEqualTo(0);
    assertThat(packageInfo.requestedPermissionsFlags[1]).isEqualTo(REQUESTED_PERMISSION_GRANTED);

    packageManager.revokeRuntimePermission(
        TEST_PACKAGE_NAME, "android.permission.READ_SMS", Process.myUserHandle());

    assertThat(packageInfo.requestedPermissionsFlags[0]).isEqualTo(0);
    assertThat(packageInfo.requestedPermissionsFlags[1]).isEqualTo(0);
  }

  @Test
  public void testQueryBroadcastReceiverSucceeds() {
    Intent intent = new Intent("org.robolectric.ACTION_RECEIVER_PERMISSION_PACKAGE");
    intent.setPackage(ApplicationProvider.getApplicationContext().getPackageName());

    List<ResolveInfo> receiverInfos = packageManager.queryBroadcastReceivers(intent, PackageManager.GET_INTENT_FILTERS);
    assertThat(receiverInfos).isNotEmpty();
    assertThat(receiverInfos.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.ConfigTestReceiverPermissionsAndActions");
    assertThat(receiverInfos.get(0).activityInfo.permission)
        .isEqualTo("org.robolectric.CUSTOM_PERM");
    assertThat(receiverInfos.get(0).filter.getAction(0))
        .isEqualTo("org.robolectric.ACTION_RECEIVER_PERMISSION_PACKAGE");
  }

  @Test
  public void testQueryBroadcastReceiverFailsForMissingPackageName() {
    Intent intent = new Intent("org.robolectric.ACTION_ONE_MORE_PACKAGE");
    List<ResolveInfo> receiverInfos = packageManager.queryBroadcastReceivers(intent, PackageManager.GET_INTENT_FILTERS);
    assertThat(receiverInfos).hasSize(0);
  }

  @Test
  public void testQueryBroadcastReceiver_matchAllWithoutIntentFilter() {
    Intent intent = new Intent();
    intent.setPackage(ApplicationProvider.getApplicationContext().getPackageName());
    List<ResolveInfo> receiverInfos = packageManager.queryBroadcastReceivers(intent, PackageManager.GET_INTENT_FILTERS);
    assertThat(receiverInfos).hasSize(7);

    for (ResolveInfo receiverInfo : receiverInfos) {
      assertThat(receiverInfo.activityInfo.name)
          .isNotEqualTo("com.bar.ReceiverWithoutIntentFilter");
    }
  }

  @Test
  public void testGetPackageInfo_ForReceiversSucceeds() throws Exception {
    PackageInfo receiverInfos =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            PackageManager.GET_RECEIVERS);

    assertThat(receiverInfos.receivers).isNotEmpty();
    assertThat(receiverInfos.receivers[0].name).isEqualTo("org.robolectric.ConfigTestReceiver.InnerReceiver");
    assertThat(receiverInfos.receivers[0].permission).isEqualTo("com.ignored.PERM");
  }

  public static class ActivityWithConfigChanges extends Activity { }

  @Test
  public void getActivityMetaData_configChanges() throws Exception {
    Activity activity = setupActivity(ShadowPackageManagerTest.ActivityWithConfigChanges.class);

    ActivityInfo activityInfo = activity.getPackageManager().getActivityInfo(activity.getComponentName(), 0);

    int configChanges = activityInfo.configChanges;
    assertThat(configChanges & ActivityInfo.CONFIG_SCREEN_LAYOUT).isEqualTo(ActivityInfo.CONFIG_SCREEN_LAYOUT);
    assertThat(configChanges & ActivityInfo.CONFIG_ORIENTATION).isEqualTo(ActivityInfo.CONFIG_ORIENTATION);

    // Spot check a few other possible values that shouldn't be in the flags.
    assertThat(configChanges & ActivityInfo.CONFIG_FONT_SCALE).isEqualTo(0);
    assertThat(configChanges & ActivityInfo.CONFIG_SCREEN_SIZE).isEqualTo(0);
  }

  /** MCC + MNC are always present in config changes since Oreo. */
  @Test
  @Config(minSdk = O)
  public void getActivityMetaData_configChangesAlwaysIncludesMccAndMnc() throws Exception {
    Activity activity = setupActivity(ShadowPackageManagerTest.ActivityWithConfigChanges.class);

    ActivityInfo activityInfo =
        activity.getPackageManager().getActivityInfo(activity.getComponentName(), 0);

    int configChanges = activityInfo.configChanges;
    assertThat(configChanges & ActivityInfo.CONFIG_MCC).isEqualTo(ActivityInfo.CONFIG_MCC);
    assertThat(configChanges & ActivityInfo.CONFIG_MNC).isEqualTo(ActivityInfo.CONFIG_MNC);
  }

  @Test
  public void getPermissionInfo_withMinimalFields() throws Exception {
    PermissionInfo permission =
        packageManager.getPermissionInfo("org.robolectric.permission_with_minimal_fields", 0);
    assertThat(permission.labelRes).isEqualTo(0);
    assertThat(permission.descriptionRes).isEqualTo(0);
    assertThat(permission.protectionLevel).isEqualTo(PermissionInfo.PROTECTION_NORMAL);
  }

  @Test
  public void getPermissionInfo_addedPermissions() throws Exception {
    PermissionInfo permissionInfo = new PermissionInfo();
    permissionInfo.name = "manually_added_permission";
    shadowPackageManager.addPermissionInfo(permissionInfo);
    PermissionInfo permission = packageManager.getPermissionInfo("manually_added_permission", 0);
    assertThat(permission.name).isEqualTo("manually_added_permission");
  }

  @Test
  public void getPermissionGroupInfo_fromManifest() throws Exception {
    PermissionGroupInfo permissionGroupInfo =
        ApplicationProvider.getApplicationContext()
            .getPackageManager()
            .getPermissionGroupInfo("org.robolectric.package_permission_group", 0);
    assertThat(permissionGroupInfo.name).isEqualTo("org.robolectric.package_permission_group");
  }

  @Test
  public void getPermissionGroupInfo_extraPermissionGroup() throws Exception {
    PermissionGroupInfo newCameraPermission = new PermissionGroupInfo();
    newCameraPermission.name = permission_group.CAMERA;
    shadowPackageManager.addPermissionGroupInfo(newCameraPermission);

    assertThat(packageManager.getPermissionGroupInfo(permission_group.CAMERA, 0).name)
        .isEqualTo(newCameraPermission.name);
  }

  @Test
  public void getAllPermissionGroups_fromManifest() throws Exception {
    List<PermissionGroupInfo> allPermissionGroups = packageManager.getAllPermissionGroups(0);
    assertThat(allPermissionGroups).hasSize(1);
    assertThat(allPermissionGroups.get(0).name).isEqualTo("org.robolectric.package_permission_group");
  }

  @Test
  public void getAllPermissionGroups_duplicateInExtraPermissions() throws Exception {
    assertThat(packageManager.getAllPermissionGroups(0)).hasSize(1);

    PermissionGroupInfo overriddenPermission = new PermissionGroupInfo();
    overriddenPermission.name = "org.robolectric.package_permission_group";
    shadowPackageManager.addPermissionGroupInfo(overriddenPermission);
    PermissionGroupInfo newCameraPermission = new PermissionGroupInfo();
    newCameraPermission.name = permission_group.CAMERA;
    shadowPackageManager.addPermissionGroupInfo(newCameraPermission);

    List<PermissionGroupInfo> allPermissionGroups = packageManager.getAllPermissionGroups(0);
    assertThat(allPermissionGroups).hasSize(2);
  }

  @Test
  public void getAllPermissionGroups_duplicatePermission() throws Exception {
    assertThat(packageManager.getAllPermissionGroups(0)).hasSize(1);

    // Package 1
    Package pkg = new Package(TEST_PACKAGE_NAME);
    ApplicationInfo appInfo = pkg.applicationInfo;
    appInfo.flags = ApplicationInfo.FLAG_INSTALLED;
    appInfo.packageName = TEST_PACKAGE_NAME;
    appInfo.sourceDir = TEST_APP_PATH;
    appInfo.name = TEST_PACKAGE_LABEL;
    PermissionGroupInfo contactsPermissionGroupInfoApp1 = new PermissionGroupInfo();
    contactsPermissionGroupInfoApp1.name = Manifest.permission_group.CONTACTS;
    PermissionGroup contactsPermissionGroupApp1 =
        new PermissionGroup(pkg, contactsPermissionGroupInfoApp1);
    pkg.permissionGroups.add(contactsPermissionGroupApp1);
    PermissionGroupInfo storagePermissionGroupInfoApp1 = new PermissionGroupInfo();
    storagePermissionGroupInfoApp1.name = permission_group.STORAGE;
    PermissionGroup storagePermissionGroupApp1 =
        new PermissionGroup(pkg, storagePermissionGroupInfoApp1);
    pkg.permissionGroups.add(storagePermissionGroupApp1);

    shadowPackageManager.addPackageInternal(pkg);

    // Package 2, contains one permission group that is the same
    Package pkg2 = new Package(TEST_PACKAGE2_NAME);
    ApplicationInfo appInfo2 = pkg2.applicationInfo;
    appInfo2.flags = ApplicationInfo.FLAG_INSTALLED;
    appInfo2.packageName = TEST_PACKAGE2_NAME;
    appInfo2.sourceDir = TEST_APP2_PATH;
    appInfo2.name = TEST_PACKAGE2_LABEL;
    PermissionGroupInfo contactsPermissionGroupInfoApp2 = new PermissionGroupInfo();
    contactsPermissionGroupInfoApp2.name = Manifest.permission_group.CONTACTS;
    PermissionGroup contactsPermissionGroupApp2 =
        new PermissionGroup(pkg2, contactsPermissionGroupInfoApp2);
    pkg2.permissionGroups.add(contactsPermissionGroupApp2);
    PermissionGroupInfo calendarPermissionGroupInfoApp2 = new PermissionGroupInfo();
    calendarPermissionGroupInfoApp2.name = permission_group.CALENDAR;
    PermissionGroup calendarPermissionGroupApp2 =
        new PermissionGroup(pkg2, calendarPermissionGroupInfoApp2);
    pkg2.permissionGroups.add(calendarPermissionGroupApp2);

    shadowPackageManager.addPackageInternal(pkg2);

    // Make sure that the duplicate permission group does not show up in the list
    // Total list should be: contacts, storage, calendar, "org.robolectric.package_permission_group"
    List<PermissionGroupInfo> allPermissionGroups = packageManager.getAllPermissionGroups(0);
    assertThat(allPermissionGroups).hasSize(4);
  }

  @Test
  public void getPackageArchiveInfo() {
    ApplicationInfo appInfo = new ApplicationInfo();
    appInfo.flags = ApplicationInfo.FLAG_INSTALLED;
    appInfo.packageName = TEST_PACKAGE_NAME;
    appInfo.sourceDir = TEST_APP_PATH;
    appInfo.name = TEST_PACKAGE_LABEL;

    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.applicationInfo = appInfo;
    shadowPackageManager.installPackage(packageInfo);

    PackageInfo packageInfoResult = shadowPackageManager.getPackageArchiveInfo(TEST_APP_PATH, 0);
    assertThat(packageInfoResult).isNotNull();
    ApplicationInfo applicationInfo = packageInfoResult.applicationInfo;
    assertThat(applicationInfo).isInstanceOf(ApplicationInfo.class);
    assertThat(applicationInfo.packageName).isEqualTo(TEST_PACKAGE_NAME);
    assertThat(applicationInfo.sourceDir).isEqualTo(TEST_APP_PATH);
  }

  @Test
  public void getApplicationInfo_ThisApplication() throws Exception {
    ApplicationInfo info =
        packageManager.getApplicationInfo(
            ApplicationProvider.getApplicationContext().getPackageName(), 0);
    assertThat(info).isNotNull();
    assertThat(info.packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test
  public void getApplicationInfo_uninstalledApplication_includeUninstalled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);
    ApplicationInfo info =
        packageManager.getApplicationInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            MATCH_UNINSTALLED_PACKAGES);
    assertThat(info).isNotNull();
    assertThat(info.packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test
  public void getApplicationInfo_uninstalledApplication_dontIncludeUninstalled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);

    try {
      packageManager.getApplicationInfo(
          ApplicationProvider.getApplicationContext().getPackageName(), 0);
      fail("PackageManager.NameNotFoundException not thrown");
    } catch (PackageManager.NameNotFoundException e) {
      // expected
    }
  }

  @Test
  public void getApplicationInfo_disabledApplication_includeDisabled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);
    ApplicationInfo info =
        packageManager.getApplicationInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            MATCH_DISABLED_COMPONENTS);
    assertThat(info).isNotNull();
    assertThat(info.packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test(expected = PackageManager.NameNotFoundException.class)
  public void getApplicationInfo_whenUnknown_shouldThrowNameNotFoundException() throws Exception {
    try {
      packageManager.getApplicationInfo("unknown_package", 0);
      fail("should have thrown NameNotFoundException");
    } catch (PackageManager.NameNotFoundException e) {
      assertThat(e.getMessage()).contains("unknown_package");
      throw e;
    }
  }

  @Test
  public void getApplicationInfo_OtherApplication() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.applicationInfo = new ApplicationInfo();
    packageInfo.applicationInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.applicationInfo.name = TEST_PACKAGE_LABEL;
    shadowPackageManager.installPackage(packageInfo);

    ApplicationInfo info = packageManager.getApplicationInfo(TEST_PACKAGE_NAME, 0);
    assertThat(info).isNotNull();
    assertThat(info.packageName).isEqualTo(TEST_PACKAGE_NAME);
    assertThat(packageManager.getApplicationLabel(info).toString()).isEqualTo(TEST_PACKAGE_LABEL);
  }

  @Test
  public void removePackage_shouldHideItFromGetApplicationInfo() {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.applicationInfo = new ApplicationInfo();
    packageInfo.applicationInfo.packageName = TEST_PACKAGE_NAME;
    packageInfo.applicationInfo.name = TEST_PACKAGE_LABEL;
    shadowPackageManager.installPackage(packageInfo);
    shadowPackageManager.removePackage(TEST_PACKAGE_NAME);

    try {
      packageManager.getApplicationInfo(TEST_PACKAGE_NAME, 0);
      fail("NameNotFoundException not thrown");
    } catch (NameNotFoundException e) {
      // expected
    }
  }

  @Test
  public void queryIntentActivities_EmptyResult() throws Exception {
    Intent i = new Intent(Intent.ACTION_APP_ERROR, null);
    i.addCategory(Intent.CATEGORY_APP_BROWSER);

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isEmpty();
  }

  @Test
  public void queryIntentActivities_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    i.addCategory(Intent.CATEGORY_LAUNCHER);

    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;
    info.activityInfo = new ActivityInfo();
    info.activityInfo.name = "name";
    info.activityInfo.packageName = TEST_PACKAGE_NAME;

    shadowPackageManager.addResolveInfoForIntent(i, info);

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(2);
    assertThat(activities.get(0).nonLocalizedLabel.toString()).isEqualTo(TEST_PACKAGE_LABEL);
  }

  @Test
  public void queryIntentActivities_ServiceMatch() throws Exception {
    Intent i = new Intent("SomeStrangeAction");

    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;
    info.serviceInfo = new ServiceInfo();
    info.serviceInfo.name = "name";
    info.serviceInfo.packageName = TEST_PACKAGE_NAME;

    shadowPackageManager.addResolveInfoForIntent(i, info);

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isNotNull();
    assertThat(activities).isEmpty();
  }

  @Test
  @Config(minSdk = JELLY_BEAN_MR1)
  public void queryIntentActivitiesAsUser_EmptyResult() throws Exception {
    Intent i = new Intent(Intent.ACTION_APP_ERROR, null);
    i.addCategory(Intent.CATEGORY_APP_BROWSER);

    List<ResolveInfo> activities = packageManager.queryIntentActivitiesAsUser(i, 0, -1);
    assertThat(activities).isEmpty();
  }

  @Test
  @Config(minSdk = JELLY_BEAN_MR1)
  public void queryIntentActivitiesAsUser_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    i.addCategory(Intent.CATEGORY_LAUNCHER);

    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;

    shadowPackageManager.addResolveInfoForIntent(i, info);

    List<ResolveInfo> activities = packageManager.queryIntentActivitiesAsUser(i, 0, -1);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(2);
    assertThat(activities.get(0).nonLocalizedLabel.toString()).isEqualTo(TEST_PACKAGE_LABEL);
  }

  @Test
  public void queryIntentActivities_launcher() {
    Intent intent = new Intent(Intent.ACTION_MAIN);
    intent.addCategory(Intent.CATEGORY_LAUNCHER);

    List<ResolveInfo> resolveInfos =
        packageManager.queryIntentActivities(intent, PackageManager.MATCH_ALL);
    assertThat(resolveInfos).hasSize(1);

    assertThat(resolveInfos.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.shadows.TestActivityAlias");
    assertThat(resolveInfos.get(0).activityInfo.targetActivity)
        .isEqualTo("org.robolectric.shadows.TestActivity");
  }

  @Test
  public void queryIntentActivities_MatchSystemOnly() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    i.addCategory(Intent.CATEGORY_LAUNCHER);

    ResolveInfo info1 = ShadowResolveInfo.newResolveInfo(TEST_PACKAGE_LABEL, TEST_PACKAGE_NAME);
    ResolveInfo info2 = ShadowResolveInfo.newResolveInfo("System App", "system.launcher");
    info2.activityInfo.applicationInfo.flags |= ApplicationInfo.FLAG_SYSTEM;
    info2.nonLocalizedLabel = "System App";

    shadowPackageManager.addResolveInfoForIntent(i, info1);
    shadowPackageManager.addResolveInfoForIntent(i, info2);

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, PackageManager.MATCH_SYSTEM_ONLY);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).nonLocalizedLabel.toString()).isEqualTo("System App");
  }

  @Test
  public void queryIntentActivities_EmptyResultWithNoMatchingImplicitIntents() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    i.addCategory(Intent.CATEGORY_LAUNCHER);
    i.setDataAndType(Uri.parse("content://testhost1.com:1/testPath/test.jpeg"), "image/jpeg");

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isEmpty();
  }

  @Test
  public void queryIntentActivities_MatchWithExplicitIntent() throws Exception {
    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.shadows.TestActivity");

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).resolvePackageName).isEqualTo("org.robolectric");
    assertThat(activities.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.shadows.TestActivity");
  }

  @Test
  public void queryIntentActivities_MatchWithImplicitIntents() throws Exception {
    Uri uri = Uri.parse("content://testhost1.com:1/testPath/test.jpeg");
    Intent i = new Intent(Intent.ACTION_VIEW);
    i.addCategory(Intent.CATEGORY_DEFAULT);
    i.setDataAndType(uri, "image/jpeg");

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).resolvePackageName).isEqualTo("org.robolectric");
    assertThat(activities.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.shadows.TestActivity");
  }

  @Test
  public void queryIntentActivities_MatchWithAliasIntents() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN);
    i.addCategory(Intent.CATEGORY_LAUNCHER);

    List<ResolveInfo> activities = packageManager.queryIntentActivities(i, 0);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).resolvePackageName).isEqualTo("org.robolectric");
    assertThat(activities.get(0).activityInfo.targetActivity).isEqualTo("org.robolectric.shadows.TestActivity");
    assertThat(activities.get(0).activityInfo.name).isEqualTo("org.robolectric.shadows.TestActivityAlias");
  }

  @Test
  public void queryIntentActivities_DisabledComponentExplicitIntent() throws Exception {
    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.shadows.TestActivity");

    ComponentName componentToDisable =
        new ComponentName(
            ApplicationProvider.getApplicationContext(),
            "org.robolectric.shadows.TestActivity");
    packageManager.setComponentEnabledSetting(
        componentToDisable,
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
        PackageManager.DONT_KILL_APP);

    List<ResolveInfo> resolveInfos = packageManager.queryIntentActivities(i, 0);
    assertThat(resolveInfos).isEmpty();
  }

  @Test
  public void queryIntentActivities_DisabledComponentImplicitIntent() throws Exception {
    Uri uri = Uri.parse("content://testhost1.com:1/testPath/test.jpeg");
    Intent i = new Intent(Intent.ACTION_VIEW);
    i.addCategory(Intent.CATEGORY_DEFAULT);
    i.setDataAndType(uri, "image/jpeg");

    ComponentName componentToDisable =
        new ComponentName(
            ApplicationProvider.getApplicationContext(),
            "org.robolectric.shadows.TestActivity");
    packageManager.setComponentEnabledSetting(
        componentToDisable,
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
        PackageManager.DONT_KILL_APP);

    List<ResolveInfo> resolveInfos = packageManager.queryIntentActivities(i, 0);
    assertThat(resolveInfos).isEmpty();
  }

  @Test
  public void queryIntentActivities_MatchDisabledComponents() throws Exception {
    Uri uri = Uri.parse("content://testhost1.com:1/testPath/test.jpeg");
    Intent i = new Intent(Intent.ACTION_VIEW);
    i.addCategory(Intent.CATEGORY_DEFAULT);
    i.setDataAndType(uri, "image/jpeg");

    ComponentName componentToDisable =
        new ComponentName(
            ApplicationProvider.getApplicationContext(),
            "org.robolectric.shadows.TestActivity");
    packageManager.setComponentEnabledSetting(
        componentToDisable,
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
        PackageManager.DONT_KILL_APP);

    List<ResolveInfo> resolveInfos =
        packageManager.queryIntentActivities(i, PackageManager.MATCH_DISABLED_COMPONENTS);
    assertThat(resolveInfos).isNotNull();
    assertThat(resolveInfos).hasSize(1);
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentActivities_appHidden_includeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.shadows.TestActivity");

    List<ResolveInfo> activities =
        packageManager.queryIntentActivities(i, MATCH_UNINSTALLED_PACKAGES);
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).resolvePackageName).isEqualTo(packageName);
    assertThat(activities.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.shadows.TestActivity");
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentActivities_appHidden_dontIncludeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.shadows.TestActivity");

    assertThat(packageManager.queryIntentActivities(i, /* flags= */ 0)).isEmpty();
  }

  @Test
  public void resolveActivity_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null).addCategory(Intent.CATEGORY_LAUNCHER);
    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;
    info.activityInfo = new ActivityInfo();
    info.activityInfo.name = "name";
    info.activityInfo.packageName = TEST_PACKAGE_NAME;
    shadowPackageManager.addResolveInfoForIntent(i, info);

    assertThat(packageManager.resolveActivity(i, 0)).isNotNull();
    assertThat(packageManager.resolveActivity(i, 0).activityInfo.name).isEqualTo("name");
    assertThat(packageManager.resolveActivity(i, 0).activityInfo.packageName)
        .isEqualTo(TEST_PACKAGE_NAME);
  }

  @Test
  public void resolveActivity_NoMatch() throws Exception {
    Intent i = new Intent();
    i.setComponent(new ComponentName("foo.bar", "No Activity"));
    assertThat(packageManager.resolveActivity(i, 0)).isNull();
  }

  @Test
  @Config(minSdk = JELLY_BEAN_MR1)
  public void resolveActivityAsUser_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null).addCategory(Intent.CATEGORY_LAUNCHER);
    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;
    info.activityInfo = new ActivityInfo();
    info.activityInfo.name = "name";
    info.activityInfo.packageName = TEST_PACKAGE_NAME;
    shadowPackageManager.addResolveInfoForIntent(i, info);

    ResolveInfo resolvedActivity =
            ReflectionHelpers.callInstanceMethod(packageManager, "resolveActivityAsUser",
                    ClassParameter.from(Intent.class, i),
                    ClassParameter.from(int.class, 0),
                    ClassParameter.from(int.class, USER_ID));

    assertThat(resolvedActivity).isNotNull();
    assertThat(resolvedActivity.activityInfo.name).isEqualTo("name");
    assertThat(resolvedActivity.activityInfo.packageName).isEqualTo(TEST_PACKAGE_NAME);
  }

  @Test
  @Config(minSdk = JELLY_BEAN_MR1)
  public void resolveActivityAsUser_NoMatch() throws Exception {
    Intent i = new Intent();
    i.setComponent(new ComponentName("foo.bar", "No Activity"));

    ResolveInfo resolvedActivity =
            ReflectionHelpers.callInstanceMethod(packageManager, "resolveActivityAsUser",
                    ClassParameter.from(Intent.class, i),
                    ClassParameter.from(int.class, 0),
                    ClassParameter.from(int.class, USER_ID));

    assertThat(resolvedActivity).isNull();
  }

  @Test
  public void queryIntentServices_EmptyResult() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    i.addCategory(Intent.CATEGORY_LAUNCHER);

    List<ResolveInfo> activities = packageManager.queryIntentServices(i, 0);
    assertThat(activities).isEmpty();
  }

  @Test
  public void queryIntentServices_MatchWithExplicitIntent() throws Exception {
    Intent i = new Intent();
    i.setClassName(ApplicationProvider.getApplicationContext(), "com.foo.Service");

    List<ResolveInfo> services = packageManager.queryIntentServices(i, 0);
    assertThat(services).isNotNull();
    assertThat(services).hasSize(1);
    assertThat(services.get(0).resolvePackageName).isEqualTo("org.robolectric");
    assertThat(services.get(0).serviceInfo.name).isEqualTo("com.foo.Service");
  }

  @Test
  public void queryIntentServices_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);

    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;

    shadowPackageManager.addResolveInfoForIntent(i, info);

    List<ResolveInfo> services = packageManager.queryIntentServices(i, 0);
    assertThat(services).hasSize(1);
    assertThat(services.get(0).nonLocalizedLabel.toString()).isEqualTo(TEST_PACKAGE_LABEL);
  }

  @Test
  public void queryIntentServices_fromManifest() {
    Intent i = new Intent("org.robolectric.ACTION_DIFFERENT_PACKAGE");
    i.addCategory(Intent.CATEGORY_LAUNCHER);
    i.setType("image/jpeg");
    List<ResolveInfo> services = packageManager.queryIntentServices(i, 0);
    assertThat(services).isNotEmpty();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentServices_appHidden_includeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent();
    i.setClassName(ApplicationProvider.getApplicationContext(), "com.foo.Service");

    List<ResolveInfo> services = packageManager.queryIntentServices(i, MATCH_UNINSTALLED_PACKAGES);
    assertThat(services).hasSize(1);
    assertThat(services.get(0).resolvePackageName).isEqualTo(packageName);
    assertThat(services.get(0).serviceInfo.name).isEqualTo("com.foo.Service");
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentServices_appHidden_dontIncludeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent();
    i.setClassName(ApplicationProvider.getApplicationContext(), "com.foo.Service");

    assertThat(packageManager.queryIntentServices(i, /* flags= */ 0)).isEmpty();
  }

  @Test
  @Config(minSdk = JELLY_BEAN_MR1)
  public void queryIntentServicesAsUser() {
    Intent i = new Intent("org.robolectric.ACTION_DIFFERENT_PACKAGE");
    i.addCategory(Intent.CATEGORY_LAUNCHER);
    i.setType("image/jpeg");
    List<ResolveInfo> services = packageManager.queryIntentServicesAsUser(i, 0, 0);
    assertThat(services).isNotEmpty();
  }

  @Test
  public void queryBroadcastReceivers_EmptyResult() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    i.addCategory(Intent.CATEGORY_LAUNCHER);

    List<ResolveInfo> broadCastReceivers = packageManager.queryBroadcastReceivers(i, 0);
    assertThat(broadCastReceivers).isEmpty();
  }

  @Test
  public void queryBroadcastReceivers_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);

    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;

    shadowPackageManager.addResolveInfoForIntent(i, info);

    List<ResolveInfo> broadCastReceivers = packageManager.queryBroadcastReceivers(i, 0);
    assertThat(broadCastReceivers).hasSize(1);
    assertThat(broadCastReceivers.get(0).nonLocalizedLabel.toString())
        .isEqualTo(TEST_PACKAGE_LABEL);
  }

  @Test
  public void queryBroadcastReceivers_MatchWithExplicitIntent() throws Exception {
    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.fakes.ConfigTestReceiver");

    List<ResolveInfo> receivers = packageManager.queryBroadcastReceivers(i, 0);
    assertThat(receivers).isNotNull();
    assertThat(receivers).hasSize(1);
    assertThat(receivers.get(0).resolvePackageName).isEqualTo("org.robolectric");
    assertThat(receivers.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.fakes.ConfigTestReceiver");
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryBroadcastReceivers_appHidden_includeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.fakes.ConfigTestReceiver");

    List<ResolveInfo> activities =
        packageManager.queryBroadcastReceivers(i, MATCH_UNINSTALLED_PACKAGES);
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).resolvePackageName).isEqualTo(packageName);
    assertThat(activities.get(0).activityInfo.name)
        .isEqualTo("org.robolectric.fakes.ConfigTestReceiver");
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryBroadcastReceivers_appHidden_dontIncludeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent();
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.fakes.ConfigTestReceiver");

    assertThat(packageManager.queryBroadcastReceivers(i, /* flags= */ 0)).isEmpty();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentContentProviders_EmptyResult() throws Exception {
    Intent i = new Intent(DocumentsContract.PROVIDER_INTERFACE);

    List<ResolveInfo> broadCastReceivers = packageManager.queryIntentContentProviders(i, 0);
    assertThat(broadCastReceivers).isEmpty();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentContentProviders_Match() throws Exception {
    Intent i = new Intent(DocumentsContract.PROVIDER_INTERFACE);

    ResolveInfo resolveInfo = new ResolveInfo();
    ProviderInfo providerInfo = new ProviderInfo();
    providerInfo.authority = "com.robolectric";
    resolveInfo.providerInfo = providerInfo;

    shadowPackageManager.addResolveInfoForIntent(i, resolveInfo);

    List<ResolveInfo> contentProviders = packageManager.queryIntentContentProviders(i, 0);
    assertThat(contentProviders).hasSize(1);
    assertThat(contentProviders.get(0).providerInfo.authority).isEqualTo(providerInfo.authority);
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentContentProviders_MatchSystemOnly() throws Exception {
    Intent i = new Intent(DocumentsContract.PROVIDER_INTERFACE);

    ResolveInfo info1 = new ResolveInfo();
    info1.providerInfo = new ProviderInfo();
    info1.providerInfo.applicationInfo = new ApplicationInfo();

    ResolveInfo info2 = new ResolveInfo();
    info2.providerInfo = new ProviderInfo();
    info2.providerInfo.applicationInfo = new ApplicationInfo();
    info2.providerInfo.applicationInfo.flags |= ApplicationInfo.FLAG_SYSTEM;
    info2.nonLocalizedLabel = "System App";

    shadowPackageManager.addResolveInfoForIntent(i, info1);
    shadowPackageManager.addResolveInfoForIntent(i, info2);

    List<ResolveInfo> activities =
        packageManager.queryIntentContentProviders(i, PackageManager.MATCH_SYSTEM_ONLY);
    assertThat(activities).isNotNull();
    assertThat(activities).hasSize(1);
    assertThat(activities.get(0).nonLocalizedLabel.toString()).isEqualTo("System App");
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentContentProviders_MatchDisabledComponents() throws Exception {
    Intent i = new Intent(DocumentsContract.PROVIDER_INTERFACE);

    ComponentName componentToDisable =
        new ComponentName(
            "org.robolectric.shadows.TestPackageName", "org.robolectric.shadows.TestProvider");
    packageManager.setComponentEnabledSetting(
        componentToDisable,
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
        PackageManager.DONT_KILL_APP);

    ResolveInfo resolveInfo = new ResolveInfo();
    resolveInfo.providerInfo = new ProviderInfo();
    resolveInfo.providerInfo.applicationInfo = new ApplicationInfo();
    resolveInfo.providerInfo.applicationInfo.packageName =
        "org.robolectric.shadows.TestPackageName";
    resolveInfo.providerInfo.name = "org.robolectric.shadows.TestProvider";

    shadowPackageManager.addResolveInfoForIntent(i, resolveInfo);

    List<ResolveInfo> resolveInfos = packageManager.queryIntentContentProviders(i, 0);
    assertThat(resolveInfos).isEmpty();

    resolveInfos =
        packageManager.queryIntentContentProviders(i, PackageManager.MATCH_DISABLED_COMPONENTS);
    assertThat(resolveInfos).isNotNull();
    assertThat(resolveInfos).hasSize(1);
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void queryIntentContentProviders_appHidden_includeUninstalled() {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    Intent i = new Intent(DocumentsContract.PROVIDER_INTERFACE);
    i.setClassName(
        ApplicationProvider.getApplicationContext(),
        "org.robolectric.shadows.TestActivity");

    ResolveInfo resolveInfo = new ResolveInfo();
    resolveInfo.providerInfo = new ProviderInfo();
    resolveInfo.providerInfo.applicationInfo = new ApplicationInfo();
    resolveInfo.providerInfo.applicationInfo.packageName = packageName;
    resolveInfo.providerInfo.name = "org.robolectric.shadows.TestProvider";

    shadowPackageManager.addResolveInfoForIntent(i, resolveInfo);

    List<ResolveInfo> resolveInfos = packageManager.queryIntentContentProviders(i, 0);
    assertThat(resolveInfos).isEmpty();

    resolveInfos = packageManager.queryIntentContentProviders(i, MATCH_UNINSTALLED_PACKAGES);

    assertThat(resolveInfos).hasSize(1);
    assertThat(resolveInfos.get(0).providerInfo.applicationInfo.packageName).isEqualTo(packageName);
    assertThat(resolveInfos.get(0).providerInfo.name)
        .isEqualTo("org.robolectric.shadows.TestProvider");
  }

  @Test
  public void resolveService_Match() throws Exception {
    Intent i = new Intent(Intent.ACTION_MAIN, null);
    ResolveInfo info = new ResolveInfo();
    info.serviceInfo = new ServiceInfo();
    info.serviceInfo.name = "name";
    shadowPackageManager.addResolveInfoForIntent(i, info);
    assertThat(packageManager.resolveService(i, 0)).isNotNull();
    assertThat(packageManager.resolveService(i, 0).serviceInfo.name).isEqualTo("name");
  }

  @Test
  public void removeResolveInfosForIntent_shouldCauseResolveActivityToReturnNull() throws Exception {
    Intent intent =
        new Intent(Intent.ACTION_APP_ERROR, null).addCategory(Intent.CATEGORY_APP_BROWSER);
    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;
    info.activityInfo = new ActivityInfo();
    info.activityInfo.packageName = "com.org";
    shadowPackageManager.addResolveInfoForIntent(intent, info);

    shadowPackageManager.removeResolveInfosForIntent(intent, "com.org");

    assertThat(packageManager.resolveActivity(intent, 0)).isNull();
  }

  @Test
  public void removeResolveInfosForIntent_forService() throws Exception {
    Intent intent =
        new Intent(Intent.ACTION_APP_ERROR, null).addCategory(Intent.CATEGORY_APP_BROWSER);
    ResolveInfo info = new ResolveInfo();
    info.nonLocalizedLabel = TEST_PACKAGE_LABEL;
    info.serviceInfo = new ServiceInfo();
    info.serviceInfo.packageName = "com.org";
    shadowPackageManager.addResolveInfoForIntent(intent, info);

    shadowPackageManager.removeResolveInfosForIntent(intent, "com.org");

    assertThat(packageManager.resolveService(intent, 0)).isNull();
  }

  @Test
  public void resolveService_NoMatch() throws Exception {
    Intent i = new Intent();
    i.setComponent(new ComponentName("foo.bar", "No Activity"));
    assertThat(packageManager.resolveService(i, 0)).isNull();
  }

  @Test
  public void queryActivityIcons_Match() throws Exception {
    Intent i = new Intent();
    i.setComponent(new ComponentName(TEST_PACKAGE_NAME, ""));
    Drawable d = new BitmapDrawable();

    shadowPackageManager.addActivityIcon(i, d);

    assertThat(packageManager.getActivityIcon(i)).isSameAs(d);
    assertThat(packageManager.getActivityIcon(i.getComponent())).isSameAs(d);
  }

  @Test
  public void hasSystemFeature() throws Exception {
    // uninitialized
    assertThat(packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)).isFalse();

    // positive
    shadowPackageManager.setSystemFeature(PackageManager.FEATURE_CAMERA, true);
    assertThat(packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)).isTrue();

    // negative
    shadowPackageManager.setSystemFeature(PackageManager.FEATURE_CAMERA, false);
    assertThat(packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)).isFalse();
  }

  @Test
  public void addSystemSharedLibraryName() {
    shadowPackageManager.addSystemSharedLibraryName("com.foo.system_library_1");
    shadowPackageManager.addSystemSharedLibraryName("com.foo.system_library_2");

    assertThat(packageManager.getSystemSharedLibraryNames())
        .asList()
        .containsExactly("com.foo.system_library_1", "com.foo.system_library_2");
  }

  @Test
  public void clearSystemSharedLibraryName() {
    shadowPackageManager.addSystemSharedLibraryName("com.foo.system_library_1");
    shadowPackageManager.clearSystemSharedLibraryNames();

    assertThat(packageManager.getSystemSharedLibraryNames()).isEmpty();
  }

  @Test
  public void getPackageInfo_shouldReturnActivityInfos() throws Exception {
    PackageInfo packageInfo =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            PackageManager.GET_ACTIVITIES);
    ActivityInfo activityInfoWithFilters =
        findActivity(packageInfo.activities, ActivityWithFilters.class.getName());
    assertThat(activityInfoWithFilters.packageName).isEqualTo("org.robolectric");
    assertThat(activityInfoWithFilters.exported).isEqualTo(true);
    assertThat(activityInfoWithFilters.permission).isEqualTo("com.foo.MY_PERMISSION");
  }

  private static ActivityInfo findActivity(ActivityInfo[] activities, String name) {
    for (ActivityInfo activityInfo : activities) {
      if (activityInfo.name.equals(name)) {
        return activityInfo;
      }
    }
    return null;
  }

  @Test
  public void getPackageInfo_getProvidersShouldReturnProviderInfos() throws Exception {
    PackageInfo packageInfo =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            PackageManager.GET_PROVIDERS);
    ProviderInfo[] providers = packageInfo.providers;
    assertThat(providers).isNotEmpty();
    assertThat(providers.length).isEqualTo(2);
    assertThat(providers[0].packageName).isEqualTo("org.robolectric");
    assertThat(providers[1].packageName).isEqualTo("org.robolectric");
  }

  @Test
  public void getProviderInfo_shouldReturnProviderInfos() throws Exception {
    ProviderInfo providerInfo1 =
        packageManager.getProviderInfo(
            new ComponentName(
                ApplicationProvider.getApplicationContext(),
                ".shadows.testing.TestContentProvider1"),
            0);
    assertThat(providerInfo1.packageName).isEqualTo("org.robolectric");
    assertThat(providerInfo1.authority).isEqualTo("org.robolectric.authority1");

    ProviderInfo providerInfo2 =
        packageManager.getProviderInfo(
            new ComponentName(
                ApplicationProvider.getApplicationContext(),
                "org.robolectric.shadows.testing.TestContentProvider2"),
            0);
    assertThat(providerInfo2.packageName).isEqualTo("org.robolectric");
    assertThat(providerInfo2.authority).isEqualTo("org.robolectric.authority2");
  }

  @Test
  public void getProviderInfo_packageNotFoundShouldThrowException() {
    try {
      packageManager.getProviderInfo(new ComponentName("non.existent.package", ".tester.DoesntExist"), 0);
      fail("should have thrown NameNotFoundException");
    } catch (NameNotFoundException e) {
      // expected
    }
  }

  @Test
  public void getProviderInfo_shouldPopulatePermissionsInProviderInfos() throws Exception {
    ProviderInfo providerInfo =
        packageManager.getProviderInfo(
            new ComponentName(
                ApplicationProvider.getApplicationContext(),
                "org.robolectric.shadows.testing.TestContentProvider1"),
            0);
    assertThat(providerInfo.authority).isEqualTo("org.robolectric.authority1");

    assertThat(providerInfo.readPermission).isEqualTo("READ_PERMISSION");
    assertThat(providerInfo.writePermission).isEqualTo("WRITE_PERMISSION");

    assertThat(providerInfo.pathPermissions).asList().hasSize(1);
    assertThat(providerInfo.pathPermissions[0].getType())
        .isEqualTo(PathPermission.PATTERN_SIMPLE_GLOB);
    assertThat(providerInfo.pathPermissions[0].getPath()).isEqualTo("/path/*");
    assertThat(providerInfo.pathPermissions[0].getReadPermission()).isEqualTo("PATH_READ_PERMISSION");
    assertThat(providerInfo.pathPermissions[0].getWritePermission()).isEqualTo("PATH_WRITE_PERMISSION");
  }

  @Test
  public void getProviderInfo_shouldMetaDataInProviderInfos() throws Exception {
    ProviderInfo providerInfo =
        packageManager.getProviderInfo(
            new ComponentName(
                ApplicationProvider.getApplicationContext(),
                "org.robolectric.shadows.testing.TestContentProvider1"),
            PackageManager.GET_META_DATA);
    assertThat(providerInfo.authority).isEqualTo("org.robolectric.authority1");

    assertThat(providerInfo.metaData.getString("greeting")).isEqualTo("Hello");
  }

  @Test
  public void resolveContentProvider_shouldResolveByPackageName() throws Exception {
    ProviderInfo providerInfo = packageManager.resolveContentProvider("org.robolectric.authority1", 0);
    assertThat(providerInfo.packageName).isEqualTo("org.robolectric");
    assertThat(providerInfo.authority).isEqualTo("org.robolectric.authority1");
  }

  @Test
  public void testReceiverInfo() throws Exception {
    ActivityInfo info =
        packageManager.getReceiverInfo(
            new ComponentName(
                ApplicationProvider.getApplicationContext(),
                ".test.ConfigTestReceiver"),
            PackageManager.GET_META_DATA);
    assertThat(info.metaData.getInt("numberOfSheep")).isEqualTo(42);
  }

  @Test
  public void testGetPackageInfo_ForReceiversIncorrectPackage() {
    try {
      packageManager.getPackageInfo("unknown_package", PackageManager.GET_RECEIVERS);
      fail("should have thrown NameNotFoundException");
    } catch (PackageManager.NameNotFoundException e) {
      assertThat(e.getMessage()).contains("unknown_package");
    }
  }

  @Test
  public void getPackageInfo_shouldReturnRequestedPermissions() throws Exception {
    PackageInfo packageInfo =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            PackageManager.GET_PERMISSIONS);
    String[] permissions = packageInfo.requestedPermissions;
    assertThat(permissions).isNotNull();
    assertThat(permissions.length).isEqualTo(4);
  }

  @Test
  public void getPackageInfo_uninstalledPackage_includeUninstalled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);
    PackageInfo info =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            MATCH_UNINSTALLED_PACKAGES);
    assertThat(info).isNotNull();
    assertThat(info.packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test
  public void getPackageInfo_uninstalledPackage_dontIncludeUninstalled() {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);

    try {
      packageManager.getPackageInfo(
          ApplicationProvider.getApplicationContext().getPackageName(), 0);
      fail("should have thrown NameNotFoundException");
    } catch (NameNotFoundException e) {
      // expected
    }
  }

  @Test
  public void getPackageInfo_disabledPackage_includeDisabled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);
    PackageInfo info =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(),
            MATCH_DISABLED_COMPONENTS);
    assertThat(info).isNotNull();
    assertThat(info.packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test
  public void getInstalledPackages_uninstalledPackage_includeUninstalled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);

    assertThat(packageManager.getInstalledPackages(MATCH_UNINSTALLED_PACKAGES)).isNotEmpty();
    assertThat(packageManager.getInstalledPackages(MATCH_UNINSTALLED_PACKAGES).get(0).packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test
  public void getInstalledPackages_uninstalledPackage_dontIncludeUninstalled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);

    assertThat(packageManager.getInstalledPackages(0)).isEmpty();
  }

  @Test
  public void getInstalledPackages_disabledPackage_includeDisabled() throws Exception {
    packageManager.setApplicationEnabledSetting(
        ApplicationProvider.getApplicationContext().getPackageName(),
        COMPONENT_ENABLED_STATE_DISABLED,
        0);

    assertThat(packageManager.getInstalledPackages(MATCH_DISABLED_COMPONENTS)).isNotEmpty();
    assertThat(packageManager.getInstalledPackages(MATCH_DISABLED_COMPONENTS).get(0).packageName)
        .isEqualTo(ApplicationProvider.getApplicationContext().getPackageName());
  }

  @Test
  public void testGetPreferredActivities() throws Exception {
    // Setup an intentfilter and add to packagemanager
    IntentFilter filter = new IntentFilter(Intent.ACTION_MAIN);
    filter.addCategory(Intent.CATEGORY_HOME);
    final String packageName = "com.example.dummy";
    ComponentName name = new ComponentName(packageName, "LauncherActivity");
    packageManager.addPreferredActivity(filter, 0, null, name);

    // Test match
    List<IntentFilter> filters = new ArrayList<>();
    filters.add(filter);

    List<ComponentName> activities = new ArrayList<>();
    packageManager.getPreferredActivities(filters, activities, null);

    assertThat(activities.size()).isEqualTo(1);
    assertThat(activities.get(0).getPackageName()).isEqualTo(packageName);

    // Test not match
    IntentFilter filter1 = new IntentFilter(Intent.ACTION_VIEW);
    filters.add(filter1);
    filters.clear();
    activities.clear();
    filters.add(filter1);

    packageManager.getPreferredActivities(filters, activities, null);

    assertThat(activities.size()).isEqualTo(0);
  }

  @Test
  public void canResolveDrawableGivenPackageAndResourceId() throws Exception {
    Drawable drawable = ShadowDrawable.createFromStream(new ByteArrayInputStream(new byte[0]), "my_source");
    shadowPackageManager.addDrawableResolution("com.example.foo", 4334, drawable);
    Drawable actual = packageManager.getDrawable("com.example.foo", 4334, null);
    assertThat(actual).isSameAs(drawable);
  }

  @Test
  public void shouldAssignTheApplicationClassNameFromTheManifest() throws Exception {
    ApplicationInfo applicationInfo = packageManager.getApplicationInfo("org.robolectric", 0);
    assertThat(applicationInfo.className).isEqualTo("org.robolectric.shadows.testing.TestApplication");
  }

  @Test
  @Config(minSdk = N_MR1)
  public void shouldAssignTheApplicationNameFromTheManifest() throws Exception {
    ApplicationInfo applicationInfo = packageManager.getApplicationInfo("org.robolectric", 0);
    assertThat(applicationInfo.name).isEqualTo("org.robolectric.shadows.testing.TestApplication");
  }

  @Test
  public void testLaunchIntentForPackage() {
    Intent intent = packageManager.getLaunchIntentForPackage(TEST_PACKAGE_LABEL);
    assertThat(intent).isNull();

    Intent launchIntent = new Intent(Intent.ACTION_MAIN);
    launchIntent.setPackage(TEST_PACKAGE_LABEL);
    launchIntent.addCategory(Intent.CATEGORY_LAUNCHER);
    launchIntent.addCategory(Intent.CATEGORY_LAUNCHER);
    ResolveInfo resolveInfo = new ResolveInfo();
    resolveInfo.activityInfo = new ActivityInfo();
    resolveInfo.activityInfo.packageName = TEST_PACKAGE_LABEL;
    resolveInfo.activityInfo.name = "LauncherActivity";
    shadowPackageManager.addResolveInfoForIntent(launchIntent, resolveInfo);

    intent = packageManager.getLaunchIntentForPackage(TEST_PACKAGE_LABEL);
    assertThat(intent.getComponent().getClassName()).isEqualTo("LauncherActivity");
  }

  @Test
  public void shouldAssignTheAppMetaDataFromTheManifest() throws Exception {
    ApplicationInfo info =
        packageManager.getApplicationInfo(
            ApplicationProvider.getApplicationContext().getPackageName(), 0);
    Bundle meta = info.metaData;

    assertThat(meta.getString("org.robolectric.metaName1")).isEqualTo("metaValue1");
    assertThat(meta.getString("org.robolectric.metaName2")).isEqualTo("metaValue2");

    assertThat(meta.getBoolean("org.robolectric.metaFalseLiteral")).isEqualTo(false);
    assertThat(meta.getBoolean("org.robolectric.metaTrueLiteral")).isEqualTo(true);

    assertThat(meta.getInt("org.robolectric.metaInt")).isEqualTo(123);
    assertThat(meta.getFloat("org.robolectric.metaFloat")).isEqualTo(1.23f);

    assertThat(meta.getInt("org.robolectric.metaColor")).isEqualTo(Color.WHITE);

    assertThat(meta.getBoolean("org.robolectric.metaBooleanFromRes"))
        .isEqualTo(
            ApplicationProvider.getApplicationContext()
                .getResources()
                .getBoolean(R.bool.false_bool_value));

    assertThat(meta.getInt("org.robolectric.metaIntFromRes"))
        .isEqualTo(
            ApplicationProvider.getApplicationContext()
                .getResources()
                .getInteger(R.integer.test_integer1));

    assertThat(meta.getInt("org.robolectric.metaColorFromRes"))
        .isEqualTo(
            ApplicationProvider.getApplicationContext()
                .getResources()
                .getColor(R.color.clear));

    assertThat(meta.getString("org.robolectric.metaStringFromRes"))
        .isEqualTo(
            ApplicationProvider.getApplicationContext()
                .getString(R.string.app_name));

    assertThat(meta.getString("org.robolectric.metaStringOfIntFromRes"))
        .isEqualTo(
            ApplicationProvider.getApplicationContext()
                .getString(R.string.str_int));

    assertThat(meta.getInt("org.robolectric.metaStringRes")).isEqualTo(R.string.app_name);
  }

  @Test
  public void testResolveDifferentIntentObjects() {
    Intent intent1 = new Intent(Intent.ACTION_MAIN);
    intent1.setPackage(TEST_PACKAGE_LABEL);
    intent1.addCategory(Intent.CATEGORY_APP_BROWSER);

    assertThat(packageManager.resolveActivity(intent1, -1)).isNull();
    ResolveInfo resolveInfo = new ResolveInfo();
    resolveInfo.activityInfo = new ActivityInfo();
    resolveInfo.activityInfo.packageName = TEST_PACKAGE_LABEL;
    resolveInfo.activityInfo.name = "BrowserActivity";
    shadowPackageManager.addResolveInfoForIntent(intent1, resolveInfo);

    // the original intent object should yield a result
    ResolveInfo result  = packageManager.resolveActivity(intent1, -1);
    assertThat(result.activityInfo.name).isEqualTo("BrowserActivity");

    // AND a new, functionally equivalent intent should also yield a result
    Intent intent2 = new Intent(Intent.ACTION_MAIN);
    intent2.setPackage(TEST_PACKAGE_LABEL);
    intent2.addCategory(Intent.CATEGORY_APP_BROWSER);
    result = packageManager.resolveActivity(intent2, -1);
    assertThat(result.activityInfo.name).isEqualTo("BrowserActivity");
  }

  @Test
  public void testResolvePartiallySimilarIntents() {
    Intent intent1 = new Intent(Intent.ACTION_APP_ERROR);
    intent1.setPackage(TEST_PACKAGE_LABEL);
    intent1.addCategory(Intent.CATEGORY_APP_BROWSER);

    assertThat(packageManager.resolveActivity(intent1, -1)).isNull();

    ResolveInfo resolveInfo = new ResolveInfo();
    resolveInfo.activityInfo = new ActivityInfo();
    resolveInfo.activityInfo.packageName = TEST_PACKAGE_LABEL;
    resolveInfo.activityInfo.name = "BrowserActivity";
    shadowPackageManager.addResolveInfoForIntent(intent1, resolveInfo);

    // the original intent object should yield a result
    ResolveInfo result  = packageManager.resolveActivity(intent1, -1);
    assertThat(result.activityInfo.name).isEqualTo("BrowserActivity");

    // an intent with just the same action should not be considered the same
    Intent intent2 = new Intent(Intent.ACTION_APP_ERROR);
    result = packageManager.resolveActivity(intent2, -1);
    assertThat(result).isNull();

    // an intent with just the same category should not be considered the same
    Intent intent3 = new Intent();
    intent3.addCategory(Intent.CATEGORY_APP_BROWSER);
    result = packageManager.resolveActivity(intent3, -1);
    assertThat(result).isNull();

    // an intent without the correct package restriction should not be the same
    Intent intent4 = new Intent(Intent.ACTION_APP_ERROR);
    intent4.addCategory(Intent.CATEGORY_APP_BROWSER);
    result = packageManager.resolveActivity(intent4, -1);
    assertThat(result).isNull();
  }

  @Test
  public void testSetApplicationEnabledSetting() {
    assertThat(packageManager.getApplicationEnabledSetting("org.robolectric")).isEqualTo(PackageManager.COMPONENT_ENABLED_STATE_DEFAULT);

    packageManager.setApplicationEnabledSetting("org.robolectric", PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0);

    assertThat(packageManager.getApplicationEnabledSetting("org.robolectric")).isEqualTo(PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
  }

  @Test
  public void testSetComponentEnabledSetting() {
    ComponentName componentName =
        new ComponentName(
            ApplicationProvider.getApplicationContext(), "org.robolectric.component");
    assertThat(packageManager.getComponentEnabledSetting(componentName)).isEqualTo(PackageManager.COMPONENT_ENABLED_STATE_DEFAULT);

    packageManager.setComponentEnabledSetting(componentName, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0);

    assertThat(packageManager.getComponentEnabledSetting(componentName)).isEqualTo(PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
  }

  public static class ActivityWithMetadata extends Activity { }

  @Test
  public void getActivityInfo_disabledActivity() throws Exception {
    ActivityInfo activityInfo =
        packageManager.getActivityInfo(
            new ComponentName("org.robolectric", "org.robolectric.shadows.DisabledActivity"),
            PackageManager.MATCH_DISABLED_COMPONENTS);

    assertThat(activityInfo).isNotNull();
    assertThat(activityInfo.enabled).isFalse();
  }

  @Test
  public void getServiceInfo_disabledService() throws Exception {
    ServiceInfo activityInfo =
        packageManager.getServiceInfo(
            new ComponentName("org.robolectric", "org.robolectric.shadows.DisabledService"),
            PackageManager.MATCH_DISABLED_COMPONENTS);

    assertThat(activityInfo).isNotNull();
    assertThat(activityInfo.enabled).isFalse();
  }

  @Test
  public void getActivityMetaData() throws Exception {
    Activity activity = setupActivity(ActivityWithMetadata.class);

    ActivityInfo activityInfo = packageManager.getActivityInfo(activity.getComponentName(), PackageManager.GET_ACTIVITIES|PackageManager.GET_META_DATA);
    assertThat(activityInfo.metaData.get("someName")).isEqualTo("someValue");
  }

  @Test
  public void shouldAssignLabelResFromTheManifest() throws Exception {
    ApplicationInfo applicationInfo = packageManager.getApplicationInfo("org.robolectric", 0);
    assertThat(applicationInfo.labelRes).isEqualTo(R.string.app_name);
    assertThat(applicationInfo.nonLocalizedLabel).isNull();
  }

  @Test
  public void getServiceInfo_shouldReturnServiceInfoIfExists() throws Exception {
    ComponentName component = new ComponentName("org.robolectric", "com.foo.Service");
    ServiceInfo serviceInfo = packageManager.getServiceInfo(component, 0);
    assertThat(serviceInfo.packageName).isEqualTo("org.robolectric");
    assertThat(serviceInfo.processName).isEqualTo("org.robolectric");
    assertThat(serviceInfo.name).isEqualTo("com.foo.Service");
    assertThat(serviceInfo.permission).isEqualTo("com.foo.MY_PERMISSION");
    assertThat(serviceInfo.applicationInfo).isNotNull();
  }

  @Test
  public void getServiceInfo_shouldReturnServiceInfoWithMetaDataWhenFlagsSet() throws Exception {
    ComponentName component = new ComponentName("org.robolectric", "com.foo.Service");
    ServiceInfo serviceInfo = packageManager.getServiceInfo(component, PackageManager.GET_META_DATA);
    assertThat(serviceInfo.metaData).isNotNull();
  }

  @Test
  public void getServiceInfo_shouldReturnServiceInfoWithoutMetaDataWhenFlagsNotSet() throws Exception {
    ComponentName component = new ComponentName("org.robolectric", "com.foo.Service");
    ServiceInfo serviceInfo = packageManager.getServiceInfo(component, 0);
    assertThat(serviceInfo.metaData).isNull();
  }

  @Test
  public void getServiceInfo_shouldThrowNameNotFoundExceptionIfNotExist() {
    ComponentName nonExistComponent = new ComponentName("org.robolectric", "com.foo.NonExistService");
    try {
      packageManager.getServiceInfo(nonExistComponent, PackageManager.GET_SERVICES);
      fail("should have thrown NameNotFoundException");
    } catch (PackageManager.NameNotFoundException e) {
      assertThat(e.getMessage()).contains("com.foo.NonExistService");
    }
  }

  @Test
  public void getNameForUid() {
    assertThat(packageManager.getNameForUid(10)).isNull();

    shadowPackageManager.setNameForUid(10, "a_name");

    assertThat(packageManager.getNameForUid(10)).isEqualTo("a_name");
  }

  @Test
  public void getPackagesForUid() {
    assertThat(packageManager.getPackagesForUid(10)).isNull();

    shadowPackageManager.setPackagesForUid(10, new String[] {"a_name"});

    assertThat(packageManager.getPackagesForUid(10)).asList().containsExactly("a_name");
  }

  @Test
  @Config(minSdk = N)
  public void getPackageUid() throws NameNotFoundException {
    shadowPackageManager.setPackagesForUid(10, new String[] {"a_name"});
    assertThat(packageManager.getPackageUid("a_name", 0)).isEqualTo(10);
  }

  @Test
  @Config(minSdk = N)
  public void getPackageUid_shouldThrowNameNotFoundExceptionIfNotExist() {
    try {
      packageManager.getPackageUid("a_name", 0);
      fail("should have thrown NameNotFoundException");
    } catch (PackageManager.NameNotFoundException e) {
      assertThat(e.getMessage()).contains("a_name");
    }
  }

  @Test
  public void getPackagesForUid_shouldReturnSetPackageName() {
    shadowPackageManager.setPackagesForUid(10, new String[] {"a_name"});
    assertThat(packageManager.getPackagesForUid(10)).asList().containsExactly("a_name");
  }

  @Test
  public void getResourcesForApplication_currentApplication() throws Exception {
    assertThat(
            packageManager
                .getResourcesForApplication("org.robolectric")
                .getString(R.string.app_name))
        .isEqualTo(
            ApplicationProvider.getApplicationContext()
                .getString(R.string.app_name));
  }

  @Test
  public void getResourcesForApplication_unknownPackage() {
    try {
      packageManager.getResourcesForApplication("non.existent.package");
      fail("should have thrown NameNotFoundException");
    } catch (NameNotFoundException e) {
      // expected
    }
  }

  @Test
  public void getResourcesForApplication_anotherPackage() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "another.package";

    ApplicationInfo applicationInfo = new ApplicationInfo();
    applicationInfo.packageName = "another.package";
    packageInfo.applicationInfo = applicationInfo;
    shadowPackageManager.installPackage(packageInfo);

    assertThat(packageManager.getResourcesForApplication("another.package")).isNotNull();
    assertThat(packageManager.getResourcesForApplication("another.package"))
        .isNotEqualTo(ApplicationProvider.getApplicationContext().getResources());
  }

  @Test @Config(minSdk = M)
  public void shouldShowRequestPermissionRationale() {
    assertThat(packageManager.shouldShowRequestPermissionRationale(Manifest.permission.CAMERA)).isFalse();

    shadowPackageManager.setShouldShowRequestPermissionRationale(Manifest.permission.CAMERA, true);

    assertThat(packageManager.shouldShowRequestPermissionRationale(Manifest.permission.CAMERA)).isTrue();
  }

  @Test
  public void getSystemAvailableFeatures() {
    assertThat(packageManager.getSystemAvailableFeatures()).isNull();

    FeatureInfo feature = new FeatureInfo();
    feature.reqGlEsVersion = 0x20000;
    feature.flags = FeatureInfo.FLAG_REQUIRED;
    shadowPackageManager.addSystemAvailableFeature(feature);

    assertThat(packageManager.getSystemAvailableFeatures()).asList().contains(feature);

    shadowPackageManager.clearSystemAvailableFeatures();

    assertThat(packageManager.getSystemAvailableFeatures()).isNull();
  }

  @Test
  public void verifyPendingInstall() {
    packageManager.verifyPendingInstall(1234, VERIFICATION_ALLOW);

    assertThat(shadowPackageManager.getVerificationResult(1234)).isEqualTo(VERIFICATION_ALLOW);
  }

  @Test
  @Config(minSdk = JELLY_BEAN_MR1)
  public void extendPendingInstallTimeout() {
    packageManager.extendVerificationTimeout(1234, 0, 1000);

    assertThat(shadowPackageManager.getVerificationExtendedTimeout(1234)).isEqualTo(1000);
  }

  @Test
  @Config(minSdk = N, maxSdk = N_MR1) // Functionality removed in O
  public void whenPackageNotPresent_getPackageSizeInfo_callsBackWithFailure() throws Exception {
    IPackageStatsObserver packageStatsObserver = mock(IPackageStatsObserver.class);
    packageManager.getPackageSizeInfo("nonexistant.package", packageStatsObserver);

    verify(packageStatsObserver).onGetStatsCompleted(packageStatsCaptor.capture(), eq(false));
    assertThat(packageStatsCaptor.getValue()).isNull();
  }

  @Test
  @Config(minSdk = N, maxSdk = N_MR1) // Functionality removed in O
  public void whenPackageNotPresentAndPaused_getPackageSizeInfo_callsBackWithFailure() throws Exception {
    Robolectric.getForegroundThreadScheduler().pause();
    IPackageStatsObserver packageStatsObserver = mock(IPackageStatsObserver.class);
    packageManager.getPackageSizeInfo("nonexistant.package", packageStatsObserver);

    verifyZeroInteractions(packageStatsObserver);

    Robolectric.getForegroundThreadScheduler().advanceToLastPostedRunnable();
    verify(packageStatsObserver).onGetStatsCompleted(packageStatsCaptor.capture(), eq(false));
    assertThat(packageStatsCaptor.getValue()).isNull();
  }

  @Test
  @Config(minSdk = N, maxSdk = N_MR1) // Functionality removed in O
  public void whenNotPreconfigured_getPackageSizeInfo_callsBackWithDefaults() throws Exception {
    IPackageStatsObserver packageStatsObserver = mock(IPackageStatsObserver.class);
    packageManager.getPackageSizeInfo("org.robolectric", packageStatsObserver);

    verify(packageStatsObserver).onGetStatsCompleted(packageStatsCaptor.capture(), eq(true));
    assertThat(packageStatsCaptor.getValue().packageName).isEqualTo("org.robolectric");
  }

  @Test
  @Config(minSdk = N, maxSdk = N_MR1) // Functionality removed in O
  public void whenPreconfigured_getPackageSizeInfo_callsBackWithConfiguredValues() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "org.robolectric";
    PackageStats packageStats = new PackageStats("org.robolectric");
    shadowPackageManager.addPackage(packageInfo, packageStats);

    IPackageStatsObserver packageStatsObserver = mock(IPackageStatsObserver.class);
    packageManager.getPackageSizeInfo("org.robolectric", packageStatsObserver);

    verify(packageStatsObserver).onGetStatsCompleted(packageStatsCaptor.capture(), eq(true));
    assertThat(packageStatsCaptor.getValue().packageName).isEqualTo("org.robolectric");
    assertThat(packageStatsCaptor.getValue().toString()).isEqualTo(packageStats.toString());
  }

  @Test
  @Config(minSdk = N, maxSdk = N_MR1) // Functionality removed in O
  public void whenPreconfiguredForAnotherPackage_getPackageSizeInfo_callsBackWithConfiguredValues() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "org.other";
    PackageStats packageStats = new PackageStats("org.other");
    shadowPackageManager.addPackage(packageInfo, packageStats);

    IPackageStatsObserver packageStatsObserver = mock(IPackageStatsObserver.class);
    packageManager.getPackageSizeInfo("org.other", packageStatsObserver);

    verify(packageStatsObserver).onGetStatsCompleted(packageStatsCaptor.capture(), eq(true));
    assertThat(packageStatsCaptor.getValue().packageName).isEqualTo("org.other");
    assertThat(packageStatsCaptor.getValue().toString()).isEqualTo(packageStats.toString());
  }

  @Test
  @Config(minSdk = N, maxSdk = N_MR1) // Functionality removed in O
  public void whenPaused_getPackageSizeInfo_callsBackWithConfiguredValuesAfterIdle() throws Exception {
    Robolectric.getForegroundThreadScheduler().pause();

    IPackageStatsObserver packageStatsObserver = mock(IPackageStatsObserver.class);
    packageManager.getPackageSizeInfo("org.robolectric", packageStatsObserver);

    verifyZeroInteractions(packageStatsObserver);

    Robolectric.getForegroundThreadScheduler().advanceToLastPostedRunnable();
    verify(packageStatsObserver).onGetStatsCompleted(packageStatsCaptor.capture(), eq(true));
    assertThat(packageStatsCaptor.getValue().packageName).isEqualTo("org.robolectric");
  }

  @Test
  public void currentToCanonicalPackageNames() {
    shadowPackageManager.addCurrentToCannonicalName("current_name_1", "cannonical_name_1");
    shadowPackageManager.addCurrentToCannonicalName("current_name_2", "cannonical_name_2");

    packageManager.currentToCanonicalPackageNames(new String[] {"current_name_1", "current_name_2"});
  }

  @Test
  public void getInstalledApplications() {
    List<ApplicationInfo> installedApplications = packageManager.getInstalledApplications(0);

    // Default should include the application under test
    assertThat(installedApplications).hasSize(1);
    assertThat(installedApplications.get(0).packageName).isEqualTo("org.robolectric");

    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "org.other";
    packageInfo.applicationInfo = new ApplicationInfo();
    packageInfo.applicationInfo.packageName = "org.other";
    shadowPackageManager.installPackage(packageInfo);

    installedApplications = packageManager.getInstalledApplications(0);
    assertThat(installedApplications).hasSize(2);
    assertThat(installedApplications.get(1).packageName).isEqualTo("org.other");
  }

  @Test
  public void getPermissionInfo() throws Exception {
    PermissionInfo permission =
        ApplicationProvider.getApplicationContext()
            .getPackageManager()
            .getPermissionInfo("org.robolectric.some_permission", 0);
    assertThat(permission.labelRes).isEqualTo(R.string.test_permission_label);
    assertThat(permission.descriptionRes).isEqualTo(R.string.test_permission_description);
    assertThat(permission.name).isEqualTo("org.robolectric.some_permission");
  }

  @Test
  public void checkSignatures_same() throws Exception {
    shadowPackageManager.installPackage(newPackageInfo("first.package", new Signature("00000000")));
    shadowPackageManager.installPackage(
        newPackageInfo("second.package", new Signature("00000000")));
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_MATCH);
  }

  @Test
  public void checkSignatures_firstNotSigned() throws Exception {
    shadowPackageManager.installPackage(newPackageInfo("first.package", (Signature[]) null));
    shadowPackageManager.installPackage(
        newPackageInfo("second.package", new Signature("00000000")));
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_FIRST_NOT_SIGNED);
  }

  @Test
  public void checkSignatures_secondNotSigned() throws Exception {
    shadowPackageManager.installPackage(newPackageInfo("first.package", new Signature("00000000")));
    shadowPackageManager.installPackage(newPackageInfo("second.package", (Signature[]) null));
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_SECOND_NOT_SIGNED);
  }

  @Test
  public void checkSignatures_neitherSigned() throws Exception {
    shadowPackageManager.installPackage(newPackageInfo("first.package", (Signature[]) null));
    shadowPackageManager.installPackage(newPackageInfo("second.package", (Signature[]) null));
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_NEITHER_SIGNED);
  }

  @Test
  public void checkSignatures_noMatch() throws Exception {
    shadowPackageManager.installPackage(newPackageInfo("first.package", new Signature("00000000")));
    shadowPackageManager.installPackage(
        newPackageInfo("second.package", new Signature("FFFFFFFF")));
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_NO_MATCH);
  }

  @Test
  public void checkSignatures_noMatch_mustBeExact() throws Exception {
    shadowPackageManager.installPackage(newPackageInfo("first.package", new Signature("00000000")));
    shadowPackageManager.installPackage(
        newPackageInfo("second.package", new Signature("00000000"), new Signature("FFFFFFFF")));
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_NO_MATCH);
  }

  @Test
  public void checkSignatures_unknownPackage() throws Exception {
    assertThat(packageManager.checkSignatures("first.package", "second.package")).isEqualTo(SIGNATURE_UNKNOWN_PACKAGE);
  }

  private static PackageInfo newPackageInfo(String packageName, Signature... signatures) {
    PackageInfo firstPackageInfo = new PackageInfo();
    firstPackageInfo.packageName = packageName;
    firstPackageInfo.signatures = signatures;
    return firstPackageInfo;
  }

  @Test
  public void getPermissionInfo_notFound(){
    try {
      packageManager.getPermissionInfo("non_existant_permission", 0);
      fail("should have thrown NameNotFoundException");
    } catch (NameNotFoundException e) {
      // expected
    }
  }

  @Test
  public void getPermissionInfo_noMetaData() throws Exception {
    PermissionInfo permission =
        packageManager.getPermissionInfo("org.robolectric.some_permission", 0);
    assertThat(permission.metaData).isNull();
    assertThat(permission.name).isEqualTo("org.robolectric.some_permission");
    assertThat(permission.descriptionRes).isEqualTo(R.string.test_permission_description);
    assertThat(permission.labelRes).isEqualTo(R.string.test_permission_label);
    assertThat(permission.nonLocalizedLabel).isNull();
    assertThat(permission.group).isEqualTo("my_permission_group");
    assertThat(permission.protectionLevel).isEqualTo(PermissionInfo.PROTECTION_DANGEROUS);
  }

  @Test
  public void getPermissionInfo_withMetaData() throws Exception {
    PermissionInfo permission =
        packageManager.getPermissionInfo(
            "org.robolectric.some_permission", PackageManager.GET_META_DATA);
    assertThat(permission.metaData).isNotNull();
    assertThat(permission.metaData.getString("meta_data_name")).isEqualTo("meta_data_value");
  }

  @Test
  public void getPermissionInfo_withLiteralLabel() throws Exception {
    PermissionInfo permission =
        packageManager.getPermissionInfo("org.robolectric.permission_with_literal_label", 0);
    assertThat(permission.labelRes).isEqualTo(0);
    assertThat(permission.nonLocalizedLabel).isEqualTo("Literal label");
    assertThat(permission.protectionLevel).isEqualTo(PermissionInfo.PROTECTION_NORMAL);
  }

  @Test
  public void queryPermissionsByGroup_groupNotFound() throws Exception {
    try {
      packageManager.queryPermissionsByGroup("nonexistent_permission_group", 0);
      fail("Exception expected");
    } catch (NameNotFoundException expected) {
    }
  }

  @Test
  public void queryPermissionsByGroup_noMetaData() throws Exception {
    List<PermissionInfo> permissions = packageManager.queryPermissionsByGroup("my_permission_group", 0);
    assertThat(permissions).hasSize(1);

    PermissionInfo permission = permissions.get(0);

    assertThat(permission.group).isEqualTo("my_permission_group");
    assertThat(permission.name).isEqualTo("org.robolectric.some_permission");
    assertThat(permission.metaData).isNull();
  }

  @Test
  public void queryPermissionsByGroup_withMetaData() throws Exception {
    List<PermissionInfo> permissions = packageManager.queryPermissionsByGroup("my_permission_group", PackageManager.GET_META_DATA);
    assertThat(permissions).hasSize(1);

    PermissionInfo permission = permissions.get(0);

    assertThat(permission.group).isEqualTo("my_permission_group");
    assertThat(permission.name).isEqualTo("org.robolectric.some_permission");
    assertThat(permission.metaData).isNotNull();
    assertThat(permission.metaData.getString("meta_data_name")).isEqualTo("meta_data_value");
  }

  @Test
  public void queryPermissionsByGroup_nullMatchesPermissionsNotAssociatedWithGroup() throws Exception {
    List<PermissionInfo> permissions = packageManager.queryPermissionsByGroup(null, 0);

    assertThat(Iterables.transform(permissions, getPermissionNames())).containsExactly(
            "org.robolectric.permission_with_minimal_fields",
            "org.robolectric.permission_with_literal_label");
  }

  @Test
  public void queryPermissionsByGroup_nullMatchesPermissionsNotAssociatedWithGroup_with_addPermissionInfo() throws Exception {
    PermissionInfo permissionInfo = new PermissionInfo();
    permissionInfo.name = "some_name";
    shadowPackageManager.addPermissionInfo(permissionInfo);

    List<PermissionInfo> permissions = packageManager.queryPermissionsByGroup(null, 0);
    assertThat(permissions).isNotEmpty();

    assertThat(permissions.get(0).name).isEqualTo(permissionInfo.name);
  }

  @Test
  public void queryPermissionsByGroup_with_addPermissionInfo() throws Exception {
    PermissionInfo permissionInfo = new PermissionInfo();
    permissionInfo.name = "some_name";
    permissionInfo.group = "some_group";
    shadowPackageManager.addPermissionInfo(permissionInfo);

    List<PermissionInfo> permissions = packageManager.queryPermissionsByGroup(permissionInfo.group, 0);
    assertThat(permissions).hasSize(1);

    assertThat(permissions.get(0).name).isEqualTo(permissionInfo.name);
    assertThat(permissions.get(0).group).isEqualTo(permissionInfo.group);
  }

  @Test
  public void getDefaultActivityIcon() {
    assertThat(packageManager.getDefaultActivityIcon()).isNotNull();
  }

  @Test
  public void addPackageShouldUseUidToProvidePackageName() throws Exception {
    PackageInfo packageInfoOne = new PackageInfo();
    packageInfoOne.packageName = "package.one";
    packageInfoOne.applicationInfo = new ApplicationInfo();
    packageInfoOne.applicationInfo.uid = 1234;
    packageInfoOne.applicationInfo.packageName = packageInfoOne.packageName;
    shadowPackageManager.installPackage(packageInfoOne);

    PackageInfo packageInfoTwo = new PackageInfo();
    packageInfoTwo.packageName = "package.two";
    packageInfoTwo.applicationInfo = new ApplicationInfo();
    packageInfoTwo.applicationInfo.uid = 1234;
    packageInfoTwo.applicationInfo.packageName = packageInfoTwo.packageName;
    shadowPackageManager.installPackage(packageInfoTwo);

    assertThat(packageManager.getPackagesForUid(1234))
        .asList()
        .containsExactly("package.one", "package.two");
  }

  @Test
  public void installerPackageName() throws Exception {
    packageManager.setInstallerPackageName("target.package", "installer.package");

    assertThat(packageManager.getInstallerPackageName("target.package")).isEqualTo("installer.package");
  }

  @Test
  public void getXml() throws Exception {
    XmlResourceParser in =
        packageManager.getXml(
            ApplicationProvider.getApplicationContext().getPackageName(),
            R.xml.dialog_preferences,
            ApplicationProvider.getApplicationContext().getApplicationInfo());
    assertThat(in).isNotNull();
  }

  @Test @Config(minSdk = LOLLIPOP)
  public void addPackageShouldNotCreateSessions() {

    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "test.package";
    shadowPackageManager.installPackage(packageInfo);

    assertThat(packageManager.getPackageInstaller().getAllSessions()).isEmpty();
  }

  @Test
  public void addPackageMultipleTimesShouldWork() throws Exception {
    shadowPackageManager.addPackage("test.package");

    // Shouldn't throw exception
    shadowPackageManager.addPackage("test.package");
  }

  @Test
  public void addPackageSetsStorage() throws Exception {
    shadowPackageManager.addPackage("test.package");

    PackageInfo packageInfo = packageManager.getPackageInfo("test.package", 0);
    assertThat(packageInfo.applicationInfo.sourceDir).isNotNull();
    assertThat(new File(packageInfo.applicationInfo.sourceDir).exists()).isTrue();
    assertThat(packageInfo.applicationInfo.publicSourceDir)
        .isEqualTo(packageInfo.applicationInfo.sourceDir);
  }

  @Test
  public void deletePackage() throws Exception {
    // Apps must have the android.permission.DELETE_PACKAGES set to delete packages.
    PackageManager packageManager =
        ApplicationProvider.getApplicationContext().getPackageManager();
    shadowPackageManager.getInternalMutablePackageInfo(
                ApplicationProvider.getApplicationContext().getPackageName())
            .requestedPermissions =
        new String[] {android.Manifest.permission.DELETE_PACKAGES};

    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "test.package";
    shadowPackageManager.installPackage(packageInfo);

    IPackageDeleteObserver mockObserver = mock(IPackageDeleteObserver.class);
    packageManager.deletePackage(packageInfo.packageName, mockObserver, 0);

    shadowPackageManager.doPendingUninstallCallbacks();

    assertThat(shadowPackageManager.getDeletedPackages()).contains(packageInfo.packageName);
    verify(mockObserver).packageDeleted(packageInfo.packageName, PackageManager.DELETE_SUCCEEDED);
  }

  @Test
  public void deletePackage_missingRequiredPermission() throws Exception {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = "test.package";
    shadowPackageManager.installPackage(packageInfo);

    IPackageDeleteObserver mockObserver = mock(IPackageDeleteObserver.class);
    packageManager.deletePackage(packageInfo.packageName, mockObserver, 0);

    shadowPackageManager.doPendingUninstallCallbacks();

    assertThat(shadowPackageManager.getDeletedPackages()).hasSize(0);
    verify(mockObserver).packageDeleted(packageInfo.packageName, PackageManager.DELETE_FAILED_INTERNAL_ERROR);
  }

  public static class ActivityWithFilters extends Activity {}

  @Test
  public void getIntentFiltersForActivity() throws NameNotFoundException {
    List<IntentFilter> intentFilters =
        shadowPackageManager.getIntentFiltersForActivity(
            new ComponentName(
                ApplicationProvider.getApplicationContext(),
                ActivityWithFilters.class));
    assertThat(intentFilters).hasSize(1);
    IntentFilter intentFilter = intentFilters.get(0);
    assertThat(intentFilter.getCategory(0)).isEqualTo(Intent.CATEGORY_DEFAULT);
    assertThat(intentFilter.getAction(0)).isEqualTo(Intent.ACTION_VIEW);
    assertThat(intentFilter.getDataPath(0).getPath()).isEqualTo("/testPath/test.jpeg");
  }

  @Test
  public void getPackageInfo_shouldHaveWritableDataDirs() throws Exception {
    PackageInfo packageInfo =
        packageManager.getPackageInfo(
            ApplicationProvider.getApplicationContext().getPackageName(), 0);

    File dataDir = new File(packageInfo.applicationInfo.dataDir);
    assertThat(dataDir.isDirectory()).isTrue();
    assertThat(dataDir.exists()).isTrue();
  }

  private static Function<PermissionInfo, String> getPermissionNames() {
    return new Function<PermissionInfo, String>() {
      @Nullable
      @Override
      public String apply(@Nullable PermissionInfo permissionInfo) {
        return permissionInfo.name;
      }
    };
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void getApplicationHiddenSettingAsUser_hidden() throws Exception {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();

    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    assertThat(packageManager.getApplicationHiddenSettingAsUser(packageName, /* user= */ null))
        .isTrue();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void getApplicationHiddenSettingAsUser_notHidden() throws Exception {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();

    assertThat(packageManager.getApplicationHiddenSettingAsUser(packageName, /* user= */ null))
        .isFalse();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void getApplicationHiddenSettingAsUser_unknownPackage() throws Exception {
    assertThat(packageManager.getApplicationHiddenSettingAsUser("not.a.package", /* user= */ null))
        .isTrue();
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void setApplicationHiddenSettingAsUser_includeUninstalled() throws Exception {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();

    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    assertThat(packageManager.getPackageInfo(packageName, MATCH_UNINSTALLED_PACKAGES)).isNotNull();
    assertThat(packageManager.getApplicationInfo(packageName, MATCH_UNINSTALLED_PACKAGES))
        .isNotNull();
    List<PackageInfo> installedPackages =
        packageManager.getInstalledPackages(MATCH_UNINSTALLED_PACKAGES);
    assertThat(installedPackages).hasSize(1);
    assertThat(installedPackages.get(0).packageName).isEqualTo(packageName);
  }

  @Test
  @Config(minSdk = LOLLIPOP)
  public void setApplicationHiddenSettingAsUser_dontIncludeUninstalled() throws Exception {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();

    packageManager.setApplicationHiddenSettingAsUser(
        packageName, /* hidden= */ true, /* user= */ null);

    try {
      packageManager.getPackageInfo(packageName, /* flags= */ 0);
      fail("PackageManager.NameNotFoundException not thrown");
    } catch (NameNotFoundException e) {
      // Expected
    }

    try {
      packageManager.getApplicationInfo(packageName, /* flags= */ 0);
      fail("PackageManager.NameNotFoundException not thrown");
    } catch (NameNotFoundException e) {
      // Expected
    }

    assertThat(packageManager.getInstalledPackages(/* flags= */ 0)).isEmpty();
  }

  @Test
  @Config(minSdk = LOLLIPOP_MR1)
  public void setUnbadgedApplicationIcon() throws Exception {
    String packageName =
        ApplicationProvider.getApplicationContext().getPackageName();
    Drawable d = new BitmapDrawable();

    shadowPackageManager.setUnbadgedApplicationIcon(packageName, d);

    assertThat(
            packageManager
                .getApplicationInfo(packageName, PackageManager.GET_SHARED_LIBRARY_FILES)
                .loadUnbadgedIcon(packageManager))
        .isSameAs(d);
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void isPackageSuspended_nonExistentPackage_shouldThrow() {
    try {
      packageManager.isPackageSuspended(TEST_PACKAGE_NAME);
      fail("Should have thrown NameNotFoundException");
    } catch (Exception expected) {
      // The compiler thinks that isPackageSuspended doesn't throw NameNotFoundException because the
      // test is compiled against the publicly released SDK.
      assertThat(expected).isInstanceOf(NameNotFoundException.class);
    }
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void isPackageSuspended_callersPackage_shouldReturnFalse() throws NameNotFoundException {
    assertThat(packageManager.isPackageSuspended(ApplicationProvider.getApplicationContext().getPackageName()))
        .isFalse();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void isPackageSuspended_installedNeverSuspendedPackage_shouldReturnFalse()
      throws NameNotFoundException {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));
    assertThat(packageManager.isPackageSuspended(TEST_PACKAGE_NAME)).isFalse();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void isPackageSuspended_installedSuspendedPackage_shouldReturnTrue()
      throws NameNotFoundException {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));
    packageManager.setPackagesSuspended(
        new String[] {TEST_PACKAGE_NAME},
        /* suspended= */ true,
        /* appExtras= */ null,
        /* launcherExtras= */ null,
        /* dialogMessage= */ (String) null);
    assertThat(packageManager.isPackageSuspended(TEST_PACKAGE_NAME)).isTrue();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void isPackageSuspended_installedUnsuspendedPackage_shouldReturnFalse()
      throws NameNotFoundException {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));
    packageManager.setPackagesSuspended(
        new String[] {TEST_PACKAGE_NAME},
        /* suspended= */ true,
        /* appExtras= */ null,
        /* launcherExtras= */ null,
        /* dialogMessage= */ (String) null);
    packageManager.setPackagesSuspended(
        new String[] {TEST_PACKAGE_NAME},
        /* suspended= */ false,
        /* appExtras= */ null,
        /* launcherExtras= */ null,
        /* dialogMessage= */ (String) null);
    assertThat(packageManager.isPackageSuspended(TEST_PACKAGE_NAME)).isFalse();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void setPackagesSuspended_withProfileOwner_shouldThrow() {
    DevicePolicyManager devicePolicyManager =
        (DevicePolicyManager)
            ApplicationProvider.getApplicationContext()
                .getSystemService(Context.DEVICE_POLICY_SERVICE);
    shadowOf(devicePolicyManager)
        .setProfileOwner(new ComponentName("com.profile.owner", "ProfileOwnerClass"));
    try {
      packageManager.setPackagesSuspended(
          new String[] {TEST_PACKAGE_NAME},
          /* suspended= */ true,
          /* appExtras= */ null,
          /* launcherExtras= */ null,
          /* dialogMessage= */ (String) null);
      fail("Should have thrown UnsupportedOperationException");
    } catch (UnsupportedOperationException expected) {
    }
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void setPackagesSuspended_withDeviceOwner_shouldThrow() {
    DevicePolicyManager devicePolicyManager =
        (DevicePolicyManager)
            ApplicationProvider.getApplicationContext()
                .getSystemService(Context.DEVICE_POLICY_SERVICE);
    shadowOf(devicePolicyManager)
        .setDeviceOwner(new ComponentName("com.device.owner", "DeviceOwnerClass"));
    // Robolectric uses a random UID (see ShadowProcess#getRandomApplicationUid) that falls within
    // the range of the system user, so the device owner is on the current user and hence apps
    // cannot be suspended.
    try {
      packageManager.setPackagesSuspended(
          new String[] {TEST_PACKAGE_NAME},
          /* suspended= */ true,
          /* appExtras= */ null,
          /* launcherExtras= */ null,
          /* dialogMessage= */ (String) null);
      fail("Should have thrown UnsupportedOperationException");
    } catch (UnsupportedOperationException expected) {
    }
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void setPackagesSuspended_shouldSuspendSuspendablePackagesAndReturnTheRest()
      throws NameNotFoundException {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName("android"));
    shadowPackageManager.installPackage(
        createPackageInfoWithPackageName("com.suspendable.package1"));
    shadowPackageManager.installPackage(
        createPackageInfoWithPackageName("com.suspendable.package2"));

    assertThat(
            packageManager.setPackagesSuspended(
                new String[] {
                  "com.nonexistent.package", // Unsuspenable (app doesn't exist).
                  "com.suspendable.package1",
                  "android", // Unsuspendable (platform package).
                  "com.suspendable.package2",
                  ApplicationProvider.getApplicationContext()
                      .getPackageName() // Unsuspendable (caller's package).
                },
                /* suspended= */ true,
                /* appExtras= */ null,
                /* launcherExtras= */ null,
                /* dialogMessage= */ (String) null))
        .asList()
        .containsExactly(
            "com.nonexistent.package", "android",
            ApplicationProvider.getApplicationContext().getPackageName());

    assertThat(packageManager.isPackageSuspended("com.suspendable.package1")).isTrue();
    assertThat(packageManager.isPackageSuspended("android")).isFalse();
    assertThat(packageManager.isPackageSuspended("com.suspendable.package2")).isTrue();
    assertThat(packageManager.isPackageSuspended(
                   ApplicationProvider.getApplicationContext().getPackageName()))
        .isFalse();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.O)
  public void getChangedPackages_negativeSequenceNumber_returnsNull() {
    shadowPackageManager.addChangedPackage(-5, TEST_PACKAGE_NAME);

    assertThat(packageManager.getChangedPackages(-5)).isNull();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.O)
  public void getChangedPackages_validSequenceNumber_withChangedPackages() {
    shadowPackageManager.addChangedPackage(0, TEST_PACKAGE_NAME);
    shadowPackageManager.addChangedPackage(0, TEST_PACKAGE2_NAME);
    shadowPackageManager.addChangedPackage(1, "appPackageName");

    ChangedPackages changedPackages = packageManager.getChangedPackages(0);
    assertThat(changedPackages.getSequenceNumber()).isEqualTo(1);
    assertThat(changedPackages.getPackageNames())
        .containsExactly(TEST_PACKAGE_NAME, TEST_PACKAGE2_NAME);
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.O)
  public void getChangedPackages_validSequenceNumber_noChangedPackages() {
    assertThat(packageManager.getChangedPackages(0)).isNull();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void setPackagesSuspended_shouldUnsuspendSuspendablePackagesAndReturnTheRest()
      throws NameNotFoundException {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName("android"));
    shadowPackageManager.installPackage(
        createPackageInfoWithPackageName("com.suspendable.package1"));
    shadowPackageManager.installPackage(
        createPackageInfoWithPackageName("com.suspendable.package2"));
    packageManager.setPackagesSuspended(
        new String[] {
          "com.suspendable.package1", "com.suspendable.package2",
        },
        /* suspended= */ false,
        /* appExtras= */ null,
        /* launcherExtras= */ null,
        /* dialogMessage= */ (String) null);

    assertThat(
            packageManager.setPackagesSuspended(
                new String[] {
                  "com.nonexistent.package", // Unsuspenable (app doesn't exist).
                  "com.suspendable.package1",
                  "android", // Unsuspendable (platform package).
                  "com.suspendable.package2",
                  ApplicationProvider.getApplicationContext()
                      .getPackageName() // Unsuspendable (caller's package).
                },
                /* suspended= */ false,
                /* appExtras= */ null,
                /* launcherExtras= */ null,
                /* dialogMessage= */ (String) null))
        .asList()
        .containsExactly(
            "com.nonexistent.package", "android", ApplicationProvider.getApplicationContext().getPackageName());

    assertThat(packageManager.isPackageSuspended("com.suspendable.package1")).isFalse();
    assertThat(packageManager.isPackageSuspended("android")).isFalse();
    assertThat(packageManager.isPackageSuspended("com.suspendable.package2")).isFalse();
    assertThat(packageManager.isPackageSuspended(ApplicationProvider.getApplicationContext().getPackageName()))
        .isFalse();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void getPackageSetting_nonExistentPackage_shouldReturnNull() {
    assertThat(shadowPackageManager.getPackageSetting(TEST_PACKAGE_NAME)).isNull();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void getPackageSetting_removedPackage_shouldReturnNull() {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));
    shadowPackageManager.removePackage(TEST_PACKAGE_NAME);

    assertThat(shadowPackageManager.getPackageSetting(TEST_PACKAGE_NAME)).isNull();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void getPackageSetting_installedNeverSuspendedPackage_shouldReturnUnsuspendedSetting() {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));

    PackageSetting setting = shadowPackageManager.getPackageSetting(TEST_PACKAGE_NAME);

    assertThat(setting.isSuspended()).isFalse();
    assertThat(setting.getDialogMessage()).isNull();
    assertThat(setting.getSuspendedAppExtras()).isNull();
    assertThat(setting.getSuspendedLauncherExtras()).isNull();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void getPackageSetting_installedSuspendedPackage_shouldReturnSuspendedSetting() {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));
    PersistableBundle appExtras = new PersistableBundle();
    appExtras.putString("key", "value");
    PersistableBundle launcherExtras = new PersistableBundle();
    launcherExtras.putInt("number", 7);
    packageManager.setPackagesSuspended(
        new String[] {TEST_PACKAGE_NAME}, true, appExtras, launcherExtras, "Dialog message");

    PackageSetting setting = shadowPackageManager.getPackageSetting(TEST_PACKAGE_NAME);

    assertThat(setting.isSuspended()).isTrue();
    assertThat(setting.getDialogMessage()).isEqualTo("Dialog message");
    assertThat(setting.getSuspendedAppExtras().getString("key")).isEqualTo("value");
    assertThat(setting.getSuspendedLauncherExtras().getInt("number")).isEqualTo(7);
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.P)
  public void getPackageSetting_installedUnsuspendedPackage_shouldReturnUnsuspendedSetting() {
    shadowPackageManager.installPackage(createPackageInfoWithPackageName(TEST_PACKAGE_NAME));
    PersistableBundle appExtras = new PersistableBundle();
    appExtras.putString("key", "value");
    PersistableBundle launcherExtras = new PersistableBundle();
    launcherExtras.putInt("number", 7);
    packageManager.setPackagesSuspended(
        new String[] {TEST_PACKAGE_NAME}, true, appExtras, launcherExtras, "Dialog message");
    packageManager.setPackagesSuspended(
        new String[] {TEST_PACKAGE_NAME}, false, appExtras, launcherExtras, "Dialog message");

    PackageSetting setting = shadowPackageManager.getPackageSetting(TEST_PACKAGE_NAME);

    assertThat(setting.isSuspended()).isFalse();
    assertThat(setting.getDialogMessage()).isNull();
    assertThat(setting.getSuspendedAppExtras()).isNull();
    assertThat(setting.getSuspendedLauncherExtras()).isNull();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.O)
  public void canRequestPackageInstalls_shouldReturnFalseByDefault() throws Exception {
    assertThat(packageManager.canRequestPackageInstalls()).isFalse();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.O)
  public void canRequestPackageInstalls_shouldReturnTrue_whenSetToTrue() throws Exception {
    shadowPackageManager.setCanRequestPackageInstalls(true);
    assertThat(packageManager.canRequestPackageInstalls()).isTrue();
  }

  @Test
  @Config(minSdk = android.os.Build.VERSION_CODES.O)
  public void canRequestPackageInstalls_shouldReturnFalse_whenSetToFalse() throws Exception {
    shadowPackageManager.setCanRequestPackageInstalls(false);
    assertThat(packageManager.canRequestPackageInstalls()).isFalse();
  }

  @Test
  public void loadIcon_default() {
    ActivityInfo info = new ActivityInfo();
    info.applicationInfo = new ApplicationInfo();
    info.packageName = "testPackage";
    info.name = "testName";

    Drawable icon = info.loadIcon(packageManager);

    assertThat(icon).isNotNull();
  }

  @Test
  public void loadIcon_specified() {
    ActivityInfo info = new ActivityInfo();
    info.applicationInfo = new ApplicationInfo();
    info.packageName = "testPackage";
    info.name = "testName";
    info.icon = R.drawable.an_image;

    Drawable icon = info.loadIcon(packageManager);

    assertThat(icon).isNotNull();
  }

  private static PackageInfo createPackageInfoWithPackageName(String packageName) {
    PackageInfo packageInfo = new PackageInfo();
    packageInfo.packageName = packageName;
    packageInfo.applicationInfo = new ApplicationInfo();
    packageInfo.applicationInfo.packageName = packageName;
    packageInfo.applicationInfo.name = TEST_PACKAGE_LABEL;
    return packageInfo;
  }

}
