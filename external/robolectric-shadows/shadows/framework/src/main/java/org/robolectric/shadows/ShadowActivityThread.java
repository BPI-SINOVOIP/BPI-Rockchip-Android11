package org.robolectric.shadows;

import static android.os.Build.VERSION_CODES.R;

import android.app.ActivityThread;
import android.content.ComponentName;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ParceledListSlice;
import android.os.RemoteException;
import android.os.UserHandle;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Collections;

import javax.annotation.Nonnull;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

@Implements(value = ActivityThread.class, isInAndroidSdk = false, looseSignatures = true)
public class ShadowActivityThread {
  private static ApplicationInfo applicationInfo;

  @Implementation
  public static Object getPackageManager() {
    ClassLoader classLoader = ShadowActivityThread.class.getClassLoader();
    Class<?> iPackageManagerClass;
    try {
      iPackageManagerClass = classLoader.loadClass("android.content.pm.IPackageManager");
    } catch (ClassNotFoundException e) {
      throw new RuntimeException(e);
    }
    return Proxy.newProxyInstance(
        classLoader,
        new Class[] {iPackageManagerClass},
        new InvocationHandler() {
          @Override
          public Object invoke(Object proxy, @Nonnull Method method, Object[] args)
              throws Exception {
            if (method.getName().equals("getApplicationInfo")) {
              String packageName = (String) args[0];
              int flags = (Integer) args[1];

              if (packageName.equals(ShadowActivityThread.applicationInfo.packageName)) {
                return ShadowActivityThread.applicationInfo;
              }

              try {
                return RuntimeEnvironment.application
                    .getPackageManager()
                    .getApplicationInfo(packageName, flags);
              } catch (PackageManager.NameNotFoundException e) {
                throw new RemoteException(e.getMessage());
              }
            } else if (method.getName().equals("getActivityInfo")) {
              ComponentName className = (ComponentName) args[0];
              int flags = (Integer) args[1];

              try {
                return RuntimeEnvironment.application
                        .getPackageManager()
                        .getActivityInfo(className, flags);
              } catch (PackageManager.NameNotFoundException e) {
                throw new RemoteException(e.getMessage());
              }
            } else if (method.getName().equals("getServiceInfo")) {
              ComponentName className = (ComponentName) args[0];
              int flags = (Integer) args[1];

              try {
                return RuntimeEnvironment.application
                        .getPackageManager()
                        .getServiceInfo(className, flags);
              } catch (PackageManager.NameNotFoundException e) {
                throw new RemoteException(e.getMessage());
              }
            } else if (method.getName().equals("getInstalledApplications")) {
              int flags = (Integer) args[0];
              int userId = (Integer) args[1];
              return new ParceledListSlice<>(
                  RuntimeEnvironment.application
                      .getApplicationContext()
                      .createContextAsUser(UserHandle.of(userId), /* flags= */ 0)
                      .getPackageManager()
                      .getInstalledApplications(flags));
            } else if (method.getName().equals("notifyPackageUse")) {
              return null;
            } else if (method.getName().equals("getPackageInstaller")) {
              return null;
            }
            throw new UnsupportedOperationException("sorry, not supporting " + method + " yet!");
          }
        });
  }

  // BEGIN-INTERNAL
  @Implementation(minSdk = R)
  public static Object getPermissionManager() {
    ClassLoader classLoader = ShadowActivityThread.class.getClassLoader();
    Class<?> iPermissionManagerClass;
    try {
      iPermissionManagerClass = classLoader.loadClass("android.permission.IPermissionManager");
    } catch (ClassNotFoundException e) {
      throw new RuntimeException(e);
    }
    return Proxy.newProxyInstance(
        classLoader,
        new Class[] {iPermissionManagerClass},
        new InvocationHandler() {
          @Override
          public Object invoke(Object proxy, @Nonnull Method method, Object[] args)
              throws Exception {
            if (method.getName().equals("getSplitPermissions")) {
              return Collections.emptyList();
            }
            return method.getDefaultValue();
          }
        });
  }
  // END-INTERNAL

  @Implementation
  public static Object currentActivityThread() {
    return RuntimeEnvironment.getActivityThread();
  }

  /**
   * Internal use only.
   *
   * @deprecated do not use
   */
  @Deprecated
  public static void setApplicationInfo(ApplicationInfo applicationInfo) {
    ShadowActivityThread.applicationInfo = applicationInfo;
  }
}
