/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.appwidget.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.cts.common.Constants;
import android.content.Context;
import android.content.Intent;
import android.content.pm.LauncherApps;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.CddTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;

@AppModeFull(reason = "Instant apps cannot provide or host app widgets")
public class RequestPinAppWidgetTest extends AppWidgetTestCase {

    private static final String LAUNCHER_CLASS = "android.appwidget.cts.packages.Launcher";
    private static final String ACTION_PIN_RESULT = "android.appwidget.cts.ACTION_PIN_RESULT";

    private String mDefaultLauncher;

    @Before
    public void setUpLauncher() throws Exception {
        mDefaultLauncher = getDefaultLauncher();
    }

    @After
    public void tearDownLauncher() throws Exception {
        // Set the launcher back
        setLauncher(mDefaultLauncher);
    }

    @CddTest(requirement="3.8.2/C-2-2")
    private void runPinWidgetTest(final String launcherPkg) throws Exception {
        setLauncher(launcherPkg + "/" + LAUNCHER_CLASS);

        Context context = getInstrumentation().getContext();

        // Request to pin widget
        BlockingBroadcastReceiver setupReceiver = new BlockingBroadcastReceiver()
                .register(Constants.ACTION_SETUP_REPLY);

        Bundle extras = new Bundle();
        extras.putString("dummy", launcherPkg + "-dummy");

        PendingIntent pinResult = PendingIntent.getBroadcast(context, 0,
                new Intent(ACTION_PIN_RESULT), PendingIntent.FLAG_ONE_SHOT);
        AppWidgetManager.getInstance(context).requestPinAppWidget(
                getFirstWidgetComponent(), extras, pinResult);

        setupReceiver.await();
        // Verify that the confirmation dialog was opened
        assertTrue(setupReceiver.result.getBooleanExtra(Constants.EXTRA_SUCCESS, false));
        assertEquals(launcherPkg, setupReceiver.result.getStringExtra(Constants.EXTRA_PACKAGE));

        LauncherApps.PinItemRequest req =
                setupReceiver.result.getParcelableExtra(Constants.EXTRA_REQUEST);
        assertNotNull(req);
        // Verify that multiple calls to getAppWidgetProviderInfo have proper dimension.
        boolean[] providerInfo = verifyInstalledProviders(Arrays.asList(
                req.getAppWidgetProviderInfo(context), req.getAppWidgetProviderInfo(context)));
        assertTrue(providerInfo[0]);
        assertNotNull(req.getExtras());
        assertEquals(launcherPkg + "-dummy", req.getExtras().getString("dummy"));

        // Accept the request
        BlockingBroadcastReceiver resultReceiver = new BlockingBroadcastReceiver()
                .register(ACTION_PIN_RESULT);
        context.sendBroadcast(new Intent(Constants.ACTION_CONFIRM_PIN)
                .setPackage(launcherPkg)
                .putExtra("dummy", "dummy-2"));
        resultReceiver.await();

        // Verify that the result contain the extras
        assertEquals("dummy-2", resultReceiver.result.getStringExtra("dummy"));
    }

    @Test
    public void testPinWidget_launcher1() throws Exception {
        runPinWidgetTest("android.appwidget.cts.packages.launcher1");
    }

    @Test
    public void testPinWidget_launcher2() throws Exception {
        runPinWidgetTest("android.appwidget.cts.packages.launcher2");
    }

    @CddTest(requirement="3.8.2/C-2-1")
    public void verifyIsRequestPinAppWidgetSupported(String launcherPkg, boolean expectedSupport)
        throws Exception {
        setLauncher(launcherPkg + "/" + LAUNCHER_CLASS);

        Context context = getInstrumentation().getContext();
        assertEquals(expectedSupport,
                AppWidgetManager.getInstance(context).isRequestPinAppWidgetSupported());
    }

    @Test
    public void testIsRequestPinAppWidgetSupported_launcher1() throws Exception {
        verifyIsRequestPinAppWidgetSupported("android.appwidget.cts.packages.launcher1", true);
    }

    @Test
    public void testIsRequestPinAppWidgetSupported_launcher2() throws Exception {
        verifyIsRequestPinAppWidgetSupported("android.appwidget.cts.packages.launcher2", true);
    }

    @Test
    public void testIsRequestPinAppWidgetSupported_launcher3() throws Exception {
        verifyIsRequestPinAppWidgetSupported("android.appwidget.cts.packages.launcher3", false);
    }

    private String getDefaultLauncher() throws Exception {
        final String PREFIX = "Launcher: ComponentInfo{";
        final String POSTFIX = "}";
        for (String s : runShellCommand("cmd shortcut get-default-launcher")) {
            if (s.startsWith(PREFIX) && s.endsWith(POSTFIX)) {
                return s.substring(PREFIX.length(), s.length() - POSTFIX.length());
            }
        }
        throw new Exception("Default launcher not found");
    }

    private void setLauncher(String component) throws Exception {
        runShellCommand("cmd package set-home-activity --user "
                + getInstrumentation().getContext().getUserId() + " " + component);
    }
}
