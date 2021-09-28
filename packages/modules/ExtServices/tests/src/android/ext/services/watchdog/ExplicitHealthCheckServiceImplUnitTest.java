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

package android.ext.services.watchdog;

import static android.service.watchdog.ExplicitHealthCheckService.PackageConfig;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.net.ConnectivityManager;
import android.test.ServiceTestCase;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.util.Arrays;
import java.util.List;

/**
 * Contains the base unit tests for ExplicitHealthCheckServiceImpl service.
 */
public class ExplicitHealthCheckServiceImplUnitTest
        extends ServiceTestCase<ExplicitHealthCheckServiceImpl> {
    private Context mContext;
    @Mock
    private ConnectivityManager mConnectivityManager;
    @Mock
    private PackageManager mPackageManager;

    public ExplicitHealthCheckServiceImplUnitTest() {
        super(ExplicitHealthCheckServiceImpl.class);
    }

    @Before
    public void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.spy(new ContextWrapper(getSystemContext()));
        setContext(mContext);

        when(mContext.getSystemService(ConnectivityManager.class)).thenReturn(mConnectivityManager);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .adoptShellPermissionIdentity(Manifest.permission.READ_DEVICE_CONFIG);

        Intent intent = new Intent(getContext(), ExplicitHealthCheckServiceImpl.class);
        startService(intent);
    }

    @After
    public void tearDown() {
        getService().mSupportedCheckers.clear();

        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Test
    public void testInitHealthCheckersWhileNoResolveInfo() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt())).thenReturn(null);

        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();

        assertThat(service.mSupportedCheckers).hasSize(0);
    }

    @Test
    public void testInitHealthCheckersWhileApplicationInfoFlagNotSystem() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_DEBUGGABLE));

        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();

        assertThat(service.mSupportedCheckers).hasSize(0);
    }

    @Test
    public void testInitHealthCheckersWhileHasResolveInfoAndFlagIsSystem() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_SYSTEM));

        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();

        assertThat(service.mSupportedCheckers).hasSize(1);
        assertThat(service.mSupportedCheckers.get("pkg").getSupportedPackageName())
                .isEqualTo("pkg");
    }

    @Test
    public void testOnRequestHealthCheck() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_SYSTEM));
        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();

        assertThat(service.mSupportedCheckers.get("pkg").isPending()).isFalse();

        service.onRequestHealthCheck(/* packageName */ "pkg");

        assertThat(service.mSupportedCheckers.get("pkg").isPending()).isTrue();
    }

    @Test
    public void testOnCancelHealthCheck() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_SYSTEM));
        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();
        service.onRequestHealthCheck(/* packageName */ "pkg");

        service.onCancelHealthCheck(/* packageName */ "pkg");

        assertThat(service.mSupportedCheckers.get("pkg").isPending()).isFalse();
    }

    @Test
    public void testOnGetSupportedPackages() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_SYSTEM));
        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();

        final List<PackageConfig> packageConfigs = service.onGetSupportedPackages();

        assertThat(packageConfigs).hasSize(1);
        assertThat(packageConfigs.get(0).getPackageName()).isEqualTo("pkg");
    }

    @Test
    public void testOnGetRequestedPackagesWhileNoRequest() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_SYSTEM));
        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();

        final List<String> packages = service.onGetRequestedPackages();

        assertThat(packages).hasSize(0);
    }

    @Test
    public void testOnGetRequestedPackagesWhileHasRequest() throws Exception {
        when(mPackageManager.queryIntentServices(any(), anyInt()))
                .thenReturn(createFakeResolveInfo(ApplicationInfo.FLAG_SYSTEM));
        final ExplicitHealthCheckServiceImpl service = getService();
        service.onCreate();
        service.onRequestHealthCheck(/* packageName */ "pkg");

        final List<String> packages = service.onGetRequestedPackages();

        assertThat(packages).hasSize(1);
        assertThat(packages.get(0)).isEqualTo("pkg");
    }

    private List<ResolveInfo> createFakeResolveInfo(int flags) {
        return Arrays.asList(
                makeNewResolveInfo(/* name */ "service", /* packageName */ "pkg", flags));
    }

    private ResolveInfo makeNewResolveInfo(String name, String packageName, int flags) {
        final ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.packageName = packageName;
        applicationInfo.flags = flags;
        final ServiceInfo serviceInfo = new ServiceInfo();
        serviceInfo.name = name;
        serviceInfo.applicationInfo = applicationInfo;
        final ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.serviceInfo = serviceInfo;
        return resolveInfo;
    }
}
