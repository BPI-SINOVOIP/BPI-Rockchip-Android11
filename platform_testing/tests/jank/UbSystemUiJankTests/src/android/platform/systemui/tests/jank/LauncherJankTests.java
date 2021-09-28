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

package android.platform.systemui.tests.jank;

import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.os.RemoteException;
import android.support.test.jank.GfxMonitor;
import android.support.test.jank.JankTest;
import android.support.test.jank.JankTestBase;
import android.support.test.launcherhelper.ILauncherStrategy;
import android.support.test.launcherhelper.LauncherStrategyFactory;
import android.support.test.timeresulthelper.TimeResultLogger;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.Until;
import android.system.helpers.OverviewHelper;

import com.android.launcher3.tapl.AllApps;
import com.android.launcher3.tapl.LauncherInstrumentation;
import com.android.launcher3.tapl.Overview;
import com.android.launcher3.tapl.Widgets;
import com.android.launcher3.tapl.Workspace;

import java.io.File;
import java.io.IOException;

/*
 * LauncherTwoJankTests cover the old launcher, and
 * LauncherJankTests cover the new GEL Launcher.
 */
public class LauncherJankTests extends JankTestBase {

    // short transitions should be repeated within the test function, otherwise frame stats
    // captured are not really meaningful in a statistical sense
    private static final int INNER_LOOP = 3;
    private UiDevice mDevice;
    private LauncherInstrumentation mLauncher;
    private ILauncherStrategy mLauncherStrategy = null;
    private static final File TIMESTAMP_FILE = new File(Environment.getExternalStorageDirectory()
            .getAbsolutePath(),"autotester.log");
    private static final File RESULTS_FILE = new File(Environment.getExternalStorageDirectory()
            .getAbsolutePath(),"results.log");

    @Override
    public void setUp() throws Exception {
        androidx.test.InstrumentationRegistry.registerInstance(getInstrumentation(), new Bundle());
        mDevice = UiDevice.getInstance(getInstrumentation());
        try {
            mDevice.setOrientationNatural();
        } catch (RemoteException e) {
            throw new RuntimeException("failed to freeze device orientaion", e);
        }
        mLauncherStrategy = LauncherStrategyFactory.getInstance(mDevice).getLauncherStrategy();
        mLauncher = new LauncherInstrumentation(getInstrumentation());
        mDevice.executeShellCommand("pm disable com.google.android.music");
        mDevice.pressHome();
    }

    public String getLauncherPackage() {
        return mDevice.getLauncherPackageName();
    }

    @Override
    protected void tearDown() throws Exception {
        mDevice.executeShellCommand("pm enable com.google.android.music");
        mDevice.unfreezeRotation();
        super.tearDown();
    }

    public void goHome() throws UiObjectNotFoundException {
        mLauncherStrategy.open();
    }

    public void resetAndOpenRecents() throws UiObjectNotFoundException, RemoteException {
        mLauncher.pressHome().switchToOverview();
    }

    public void prepareOpenAllAppsContainer() throws IOException {
        TimeResultLogger.writeTimeStampLogStart(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
        OverviewHelper.getInstance().populateManyRecentApps();
    }

    public void afterTestOpenAllAppsContainer(Bundle metrics) throws IOException {
        TimeResultLogger.writeTimeStampLogEnd(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
        TimeResultLogger.writeResultToFile(String.format("%s-%s",
                getClass().getSimpleName(), getName()), RESULTS_FILE, metrics);
        super.afterTest(metrics);
    }

    /** Starts from the home screen, and measures jank while opening the all apps container. */
    @JankTest(expectedFrames=100, beforeTest="prepareOpenAllAppsContainer",
            beforeLoop="resetAndOpenRecents", afterTest="afterTestOpenAllAppsContainer")
    @GfxMonitor(processName="#getLauncherPackage")
    public void testOpenAllAppsContainer() throws UiObjectNotFoundException {
        Overview overview = mLauncher.getOverview();
        for (int i = 0; i < INNER_LOOP * 2; i++) {
            overview = overview.switchToAllApps().switchBackToOverview();
        }
    }

    public void openAllApps() throws UiObjectNotFoundException, IOException {
        mLauncher.pressHome().switchToAllApps();
        TimeResultLogger.writeTimeStampLogStart(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
    }

    public void afterTestAllAppsContainerSwipe(Bundle metrics) throws IOException {
        TimeResultLogger.writeTimeStampLogEnd(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
        TimeResultLogger.writeResultToFile(String.format("%s-%s",
                getClass().getSimpleName(), getName()), RESULTS_FILE, metrics);
        super.afterTest(metrics);
    }

    /** Starts from the all apps container, and measures jank while swiping between pages */
    @JankTest(beforeTest="openAllApps", afterTest="afterTestAllAppsContainerSwipe",
            expectedFrames=100)
    @GfxMonitor(processName="#getLauncherPackage")
    public void testAllAppsContainerSwipe() {
        final AllApps allApps = mLauncher.getAllApps();
        for (int i = 0; i < INNER_LOOP * 2; i++) {
            allApps.flingForward();
            allApps.flingBackward();
        }
    }

    public void makeHomeScrollable() throws UiObjectNotFoundException, IOException {
        mLauncher.pressHome().ensureWorkspaceIsScrollable();
        TimeResultLogger.writeTimeStampLogStart(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
    }

    public void afterTestHomeScreenSwipe(Bundle metrics) throws IOException {
        TimeResultLogger.writeTimeStampLogEnd(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
        TimeResultLogger.writeResultToFile(String.format("%s-%s",
                getClass().getSimpleName(), getName()), RESULTS_FILE, metrics);
        super.afterTest(metrics);
    }

    /** Starts from the home screen, and measures jank while swiping between pages */
    @JankTest(beforeTest="makeHomeScrollable", afterTest="afterTestHomeScreenSwipe",
              expectedFrames=100)
    @GfxMonitor(processName="#getLauncherPackage")
    public void testHomeScreenSwipe() {
        final Workspace workspace = mLauncher.getWorkspace();
        for (int i = 0; i < INNER_LOOP * 2; i++) {
            workspace.flingForward();
            workspace.flingBackward();
        }
    }

    public void openAllWidgets() throws UiObjectNotFoundException, IOException {
        mLauncher.pressHome().openAllWidgets();
        TimeResultLogger.writeTimeStampLogStart(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
    }

    public void afterTestWidgetsContainerFling(Bundle metrics) throws IOException {
        TimeResultLogger.writeTimeStampLogEnd(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
        TimeResultLogger.writeResultToFile(String.format("%s-%s",
                getClass().getSimpleName(), getName()), RESULTS_FILE, metrics);
        super.afterTest(metrics);
    }

    /** Starts from the widgets container, and measures jank while swiping between pages */
    @JankTest(beforeTest="openAllWidgets", afterTest="afterTestWidgetsContainerFling",
              expectedFrames=100)
    @GfxMonitor(processName="#getLauncherPackage")
    public void testWidgetsContainerFling() {
        final Widgets widgets = mLauncher.getAllWidgets();
        for (int i = 0; i < INNER_LOOP; i++) {
            widgets.flingForward();
            widgets.flingBackward();
        }
    }

    public void beforeOpenCloseMessagesApp() throws UiObjectNotFoundException, IOException {
        TimeResultLogger.writeTimeStampLogStart(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
    }

    public void afterOpenCloseMessagesApp(Bundle metrics) throws IOException {
        TimeResultLogger.writeTimeStampLogEnd(String.format("%s-%s",
                getClass().getSimpleName(), getName()), TIMESTAMP_FILE);
        TimeResultLogger.writeResultToFile(String.format("%s-%s",
                getClass().getSimpleName(), getName()), RESULTS_FILE, metrics);
        super.afterTest(metrics);
    }

    /**
     * Opens and closes the Messages app repeatedly, measuring jank for synchronized app
     * transitions.
     */
    @JankTest(beforeTest="beforeOpenCloseMessagesApp", afterTest="afterOpenCloseMessagesApp",
            expectedFrames=90)
    @GfxMonitor(processName="#getLauncherPackage")
    public void testOpenCloseMessagesApp() throws Exception {
        for (int i = 0; i < INNER_LOOP; i++) {
            mLauncherStrategy.launch("Messages", "com.google.android.apps.messaging");
            mLauncher.pressHome();
        }
    }

    private void startAppFast(String packageName) {
        final android.app.Instrumentation instrumentation = getInstrumentation();
        final Intent intent =
                instrumentation
                        .getContext()
                        .getPackageManager()
                        .getLaunchIntentForPackage(packageName);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        instrumentation.getTargetContext().startActivity(intent);
        assertTrue(
                packageName + " didn't start",
                mDevice.wait(Until.hasObject(By.pkg(packageName).depth(0)), 60000));
    }

    /**
     * Fast-opens the Messages app repeatedly and goes to home, measuring jank for going from app to
     * home.
     */
    @JankTest(
            beforeTest = "beforeOpenCloseMessagesApp",
            afterTest = "afterOpenCloseMessagesApp",
            expectedFrames = 90)
    @GfxMonitor(processName = "#getLauncherPackage")
    public void testAppToHome() throws Exception {
        for (int i = 0; i < INNER_LOOP; i++) {
            startAppFast("com.google.android.apps.messaging");
            mLauncher.pressHome();
        }
    }
}
