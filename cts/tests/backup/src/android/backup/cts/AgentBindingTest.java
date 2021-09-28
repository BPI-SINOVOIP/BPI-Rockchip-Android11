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
 * limitations under the License
 */

package android.backup.cts;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.testng.Assert.expectThrows;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.IBinder;
import android.platform.test.annotations.AppModeFull;

import java.lang.reflect.Method;

@AppModeFull
public class AgentBindingTest extends BaseBackupCtsTest {
    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
    }

    public void testUnbindBackupAgent_isNotCallableFromCts() throws Exception {
        if (!isBackupSupported()) {
            return;
        }
        expectThrows(Exception.class, () -> unbindBackupAgent(mContext.getApplicationInfo()));
    }

    public void testBindBackupAgent_isNotCallableFromCts() throws Exception {
        if (!isBackupSupported()) {
            return;
        }
        expectThrows(Exception.class, () -> bindBackupAgent(mContext.getPackageName(), 0, 0));
    }

    private static void unbindBackupAgent(ApplicationInfo applicationInfo) throws Exception {
        callActivityManagerMethod(
                "unbindBackupAgent",
                new Class<?>[] {ApplicationInfo.class},
                new Object[] {applicationInfo});
    }

    private static void bindBackupAgent(String packageName, int backupRestoreMode, int userId)
            throws Exception {
        callActivityManagerMethod(
                "bindBackupAgent",
                new Class<?>[] {String.class, int.class, int.class},
                new Object[] {packageName, backupRestoreMode, userId});
    }

    private static void callActivityManagerMethod(
            String methodName, Class<?>[] types, Object[] args) throws Exception {
        Class<?> activityManagerClass = Class.forName("android.app.IActivityManager");
        Object activityManager = getActivityManager();
        Method bindBackupAgentMethod = activityManagerClass.getMethod(methodName, types);
        bindBackupAgentMethod.invoke(activityManager, args);
    }

    private static Object getActivityManager() throws Exception {
        Class<?> serviceManagerClass = Class.forName("android.os.ServiceManager");
        Class<?> stubClass = Class.forName("android.app.IActivityManager$Stub");
        Method asInterfaceMethod = stubClass.getMethod("asInterface", IBinder.class);
        Method getServiceMethod = serviceManagerClass.getMethod("getService", String.class);
        return asInterfaceMethod.invoke(
                null, (IBinder) getServiceMethod.invoke(serviceManagerClass, "activity"));
    }
}
