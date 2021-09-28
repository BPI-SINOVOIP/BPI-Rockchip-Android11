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

package com.android.helpers;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.support.test.uiautomator.UiDevice;
import androidx.test.runner.AndroidJUnit4;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.HashMap;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

/** Android Unit tests for {@link BinderCollectionHelper}. */
@RunWith(AndroidJUnit4.class)
public class BinderCollectionHelperTest {

    private @Mock UiDevice mUiDevice;
    private BinderCollectionHelper mHelper;
    private String fakeTrace =
            "Binder transaction traces for all processes.\n"
                    + "\n"
                    + "Traces for process: system\n"
                    + "Count: 2\n"
                    + "Trace: java.lang.Throwable\n"
                    + "\tat android.os.BinderProxy.transact(BinderProxy.java:486)\n"
                    + "\tat android.os.ServiceManagerProxy.getService(ServiceManagerNative.java:128)\n"
                    + "\tat android.os.ServiceManager.rawGetService(ServiceManager.java:263)\n"
                    + "\tat android.os.ServiceManager.getService(ServiceManager.java:129)\n"
                    + "\tat android.os.StrictMode.isUserKeyUnlocked(StrictMode.java:2137)\n"
                    + "\tat android.os.StrictMode.onCredentialProtectedPathAccess(StrictMode.java:2159)\n"
                    + "\tat android.os.StrictMode.access$1700(StrictMode.java:150)\n"
                    + "\tat android.os.StrictMode$5.onPathAccess(StrictMode.java:1813)\n"
                    + "\tat java.io.UnixFileSystem.list(UnixFileSystem.java:345)\n"
                    + "\tat java.io.File.list(File.java:1131)\n"
                    + "\tat java.io.File.listFiles(File.java:1216)\n"
                    + "\tat com.android.server.wm.TaskPersister.removeObsoleteFiles(TaskPersister.java:477)\n"
                    + "\tat com.android.server.wm.TaskPersister.onPreProcessItem(TaskPersister.java:414)\n"
                    + "\tat com.android.server.wm.PersisterQueue$LazyTaskWriterThread.run(PersisterQueue.java:295)\n"
                    + "\n"
                    + "\n"
                    + "Count: 18\n"
                    + "Trace: java.lang.Throwable\n"
                    + "\tat android.os.BinderProxy.transact(BinderProxy.java:486)\n"
                    + "\tat android.app.IApplicationThread$Stub$Proxy.scheduleBindService(IApplicationThread.java:1589)\n"
                    + "\tat com.android.server.am.ActiveServices.requestServiceBindingLocked(ActiveServices.java:2297)\n"
                    + "\tat com.android.server.am.ActiveServices.requestServiceBindingsLocked(ActiveServices.java:2641)\n"
                    + "\tat com.android.server.am.ActiveServices.realStartServiceLocked(ActiveServices.java:2719)\n"
                    + "\tat com.android.server.am.ActiveServices.bringUpServiceLocked(ActiveServices.java:2564)\n"
                    + "\tat com.android.server.am.ActiveServices.bindServiceLocked(ActiveServices.java:1779)\n"
                    + "\tat com.android.server.am.ActivityManagerService.bindIsolatedService(ActivityManagerService.java:14064)\n"
                    + "\tat android.app.ContextImpl.bindServiceCommon(ContextImpl.java:1735)\n"
                    + "\tat android.app.ContextImpl.bindServiceAsUser(ContextImpl.java:1675)\n"
                    + "\tat com.android.server.accounts.AccountManagerService$Session.bindToAuthenticator(AccountManagerService.java:5043)\n"
                    + "\tat com.android.server.accounts.AccountManagerService$Session.bind(AccountManagerService.java:4825)\n"
                    + "\tat com.android.server.accounts.AccountManagerService.hasFeatures(AccountManagerService.java:1891)\n"
                    + "\tat android.accounts.IAccountManager$Stub.onTransact(IAccountManager.java:577)\n"
                    + "\tat com.android.server.accounts.AccountManagerService.onTransact(AccountManagerService.java:1072)\n"
                    + "\tat android.os.Binder.execTransactInternal(Binder.java:1021)\n"
                    + "\tat android.os.Binder.execTransact(Binder.java:994)\n"
                    + "Traces for process: com.breel.wallpapers18\n"
                    + "Traces for process: com.android.se\n";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHelper = Mockito.spy(new BinderCollectionHelper());
        when(mHelper.getDevice()).thenReturn(mUiDevice);
    }

    @Test
    public void testParser_zeroCount() throws Exception {
        String process = "com.breel.wallpapers18";
        mHelper.addTrackedProcesses(process);
        Map<String, Integer> result = new HashMap<>();
        mHelper.parseMetrics(getBufferedReader(), result);
        assertTrue(result.get(process) == 0);
    }

    @Test
    public void testParser_multiCount() throws Exception {
        String process = "system";
        mHelper.addTrackedProcesses(process);
        Map<String, Integer> result = new HashMap<>();
        mHelper.parseMetrics(getBufferedReader(), result);
        assertTrue(result.get(process) == 20);
    }

    private BufferedReader getBufferedReader() throws Exception {
        File temp = File.createTempFile("tempFile", "txt");
        BufferedWriter writer = new BufferedWriter(new FileWriter(temp));
        writer.write(fakeTrace);
        writer.close();
        return new BufferedReader(new FileReader(temp));
    }
}
