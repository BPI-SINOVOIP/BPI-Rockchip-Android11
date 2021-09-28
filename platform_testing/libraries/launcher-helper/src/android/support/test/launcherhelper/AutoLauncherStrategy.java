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
package android.support.test.launcherhelper;

import android.app.Instrumentation;
import android.os.SystemClock;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.system.helpers.CommandsHelper;

import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class AutoLauncherStrategy implements IAutoLauncherStrategy {
    private static final String LOG_TAG = AutoLauncherStrategy.class.getSimpleName();
    private static final String CAR_LENSPICKER = "com.android.car.carlauncher";
    private static final String SYSTEM_UI_PACKAGE = "com.android.systemui";
    private static final String MAPS_PACKAGE = "com.google.android.apps.maps";
    private static final String MEDIA_PACKAGE = "com.android.car.media";
    private static final String RADIO_PACKAGE = "com.android.car.radio";
    private static final String DIAL_PACKAGE = "com.android.car.dialer";
    private static final String ASSISTANT_PACKAGE = "com.google.android.googlequicksearchbox";
    private static final String SETTINGS_PACKAGE = "com.android.car.settings";
    private static final String APP_SWITCH_ID = "car_ui_toolbar_menu_item_icon_container";
    private static final String APP_LIST_ID = "apps_grid";

    private static final long APP_LAUNCH_TIMEOUT = 30000;
    private static final long UI_WAIT_TIMEOUT = 5000;
    private static final long POLL_INTERVAL = 100;

    private static final BySelector UP_BTN =
            By.res(Pattern.compile(".*:id/car_ui_scrollbar_page_up"));
    private static final BySelector DOWN_BTN =
            By.res(Pattern.compile(".*:id/car_ui_scrollbar_page_down"));
    private static final BySelector APP_SWITCH = By.res(Pattern.compile(".*:id/" + APP_SWITCH_ID));
    private static final BySelector APP_LIST = By.res(Pattern.compile(".*:id/" + APP_LIST_ID));
    private static final BySelector SCROLLABLE_APP_LIST =
            By.res(Pattern.compile(".*:id/" + APP_LIST_ID)).scrollable(true);
    private static final BySelector QUICK_SETTINGS = By.res(SYSTEM_UI_PACKAGE, "qs");
    private static final BySelector LEFT_HVAC = By.res(SYSTEM_UI_PACKAGE, "hvacleft");
    private static final BySelector RIGHT_HVAC = By.res(SYSTEM_UI_PACKAGE, "hvacright");

    private static final Map<String, BySelector> FACET_MAP =
            Stream.of(new Object[][] {
                { "Home", By.res(SYSTEM_UI_PACKAGE, "home").clickable(true) },
                { "Maps", By.res(SYSTEM_UI_PACKAGE, "maps_nav").clickable(true) },
                { "Media", By.res(SYSTEM_UI_PACKAGE, "music_nav").clickable(true) },
                { "Dial", By.res(SYSTEM_UI_PACKAGE, "phone_nav").clickable(true) },
                { "App Grid", By.res(SYSTEM_UI_PACKAGE, "grid_nav").clickable(true) },
                { "Notification", By.res(SYSTEM_UI_PACKAGE, "notifications").clickable(true) },
                { "Google Assistant", By.res(SYSTEM_UI_PACKAGE, "assist").clickable(true) },
            }).collect(Collectors.toMap(data -> (String) data[0], data -> (BySelector) data[1]));

    private static final Map<String, BySelector> APP_OPEN_VERIFIERS =
            Stream.of(new Object[][] {
                { "Home", By.hasDescendant(By.res(CAR_LENSPICKER, "maps"))
                            .hasDescendant(By.res(CAR_LENSPICKER, "contextual"))
                            .hasDescendant(By.res(CAR_LENSPICKER, "playback"))
                },
                { "Maps", By.pkg(MAPS_PACKAGE).depth(0) },
                { "Media", By.pkg(MEDIA_PACKAGE).depth(0) },
                { "Radio", By.pkg(RADIO_PACKAGE).depth(0) },
                { "Dial",  By.pkg(DIAL_PACKAGE).depth(0) },
                { "App Grid", By.res(CAR_LENSPICKER, "apps_grid") },
                { "Notification", By.res(SYSTEM_UI_PACKAGE, "notifications") },
                { "Google Assistant", By.pkg(ASSISTANT_PACKAGE) },
                { "Settings", By.pkg(SETTINGS_PACKAGE).depth(0) },
            }).collect(Collectors.toMap(data -> (String) data[0], data -> (BySelector) data[1]));

    protected UiDevice mDevice;
    private Instrumentation mInstrumentation;
    protected CommandsHelper mCommandsHelper;

    /**
     * {@inheritDoc}
     */
    @Override
    public String getSupportedLauncherPackage() {
        return CAR_LENSPICKER;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setUiDevice(UiDevice uiDevice) {
        mDevice = uiDevice;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setInstrumentation(Instrumentation instrumentation) {
        mInstrumentation = instrumentation;
        mCommandsHelper = CommandsHelper.getInstance(mInstrumentation);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void open() {
        openHomeFacet();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openHomeFacet() {
        openFacet("Home");
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openMapsFacet() {
        openFacet("Maps");
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openMediaFacet() {
        openFacet("Media");
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openMediaFacet(String appName) {
        openMediaFacet();

        // Click on app switch to open app list.
        List<UiObject2> buttons = mDevice.wait(
                Until.findObjects(APP_SWITCH), APP_LAUNCH_TIMEOUT);
        int lastIndex = buttons.size() - 1;
        /*
         * On some media app page, there are two buttons with the same ID, 
         * while on other media app page, only the app switch button presents.
         * The app switch button is always the last button if not the only button.
         */
        UiObject2 appSwitch = buttons.get(lastIndex);
        if (appSwitch == null) {
            throw new RuntimeException("Failed to find app switch.");
        }
        appSwitch.clickAndWait(Until.newWindow(), UI_WAIT_TIMEOUT);
        mDevice.waitForIdle();

        // Click the targeted app in app list.
        UiObject2 app = findApplication(appName);
        if (app == null) {
            throw new RuntimeException(String.format("Failed to find %s app in media.", appName));
        }
        app.click();
        mDevice.wait(Until.gone(APP_LIST), UI_WAIT_TIMEOUT);

        // Verify either a Radio or Media app is in foreground for success.
        if (appName.equals("Radio")) {
            waitUntilAppOpen("Radio", APP_LAUNCH_TIMEOUT);
        } else {
            waitUntilAppOpen("Media", APP_LAUNCH_TIMEOUT);
        }

    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openDialFacet() {
        openFacet("Dial");
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openAppGridFacet() {
        openFacet("App Grid");
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openNotificationFacet() {
        openFacet("Notification");
    }

    /** {@inheritDoc} */
    @Override
    public void openNotifications() {
        openNotificationFacet();
    }

    /** {@inheritDoc} */
    @Override
    public void pressHome() {
        openHomeFacet();
    }

    /** {@inheritDoc} */
    @Override
    public void openAssistantFacet() {
        openFacet("Google Assistant");
    }

    private void openFacet(String facetName) {
        BySelector facetSelector = FACET_MAP.get(facetName);
        UiObject2 facet = mDevice.findObject(facetSelector);
        if (facet != null) {
            facet.click();
            if (!facetName.equals("Media")) {
                // Verify the corresponding app has been open in the foregorund for success.
                waitUntilAppOpen(facetName, APP_LAUNCH_TIMEOUT);
            } else {
                // For Media facet, it could open either radio app or some media app.
                waitUntilOneOfTheAppOpen(Arrays.asList("Radio", "Media"), APP_LAUNCH_TIMEOUT);
            }
        } else {
            throw new RuntimeException(String.format("Failed to find %s facet.", facetName));
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void openQuickSettings() {
        UiObject2 quickSettings = mDevice.findObject(QUICK_SETTINGS);
        if (quickSettings != null) {
            quickSettings.click();
            waitUntilAppOpen("Settings", APP_LAUNCH_TIMEOUT);
        } else {
            throw new RuntimeException("Failed to find quick settings.");
        }
    }

    /**
     * Wait for <code>timeout</code> milliseconds until the BySelector which can verify that the app
     * corresponding to <code>appName</code> is in foreground has been found. If the BySelector
     * isn't found after timeout, throw an error.
     */
    private void waitUntilAppOpen(String appName, long timeout) {
        waitUntilOneOfTheAppOpen(Arrays.asList(appName), timeout);
    }

    /**
     * Wait for <code>time</code> milliseconds until one of the app in <code>appNames</code> has
     * been found in the foreground.
     */
    private void waitUntilOneOfTheAppOpen(List<String> appNames, long timeout) {
        long startTime = SystemClock.uptimeMillis();

        boolean isAnyOpen = false;
        while (SystemClock.uptimeMillis() - startTime < timeout) {
            isAnyOpen =
                    appNames.stream()
                            .map(appName -> mDevice.hasObject(APP_OPEN_VERIFIERS.get(appName)))
                            .anyMatch(isOpen -> isOpen == true);
            if (isAnyOpen) {
                break;
            }

            SystemClock.sleep(POLL_INTERVAL);
        }

        if (!isAnyOpen) {
            throw new IllegalStateException(
                    String.format(
                            "Did not find any app of %s in foreground after %d ms.",
                            String.join(", ", appNames), timeout));
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void clickLeftHvac() {
        clickHvac(LEFT_HVAC);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void clickRightHvac() {
        clickHvac(RIGHT_HVAC);
    }

    private void clickHvac(BySelector hvacSelector) {
        UiObject2 hvac = mDevice.findObject(hvacSelector);
        if (hvac != null) {
            // Hvac is not verifiable from uiautomator. It does not starts a new window, nor does
            // it spawn any specific identifiable ui components from uiautomator's perspective.
            // Therefore, this wait does not verify a new window, but rather it always timeout
            // after <code>APP_LAUNCH_TIMEOUT</code> amount of time so that hvac has sufficient
            // time to be opened/closed.
            hvac.clickAndWait(Until.newWindow(), APP_LAUNCH_TIMEOUT);
            mDevice.waitForIdle();
        } else {
            throw new RuntimeException("Failed to find hvac.");
        }
    }

    @Override
    public boolean checkApplicationExists(String appName) {
        openAppGridFacet();
        UiObject2 app = findApplication(appName);
        return app != null;
    }

    @Override
    public void openApp(String appName) {
        if (checkApplicationExists(appName)) {
            UiObject2 app = mDevice.findObject(By.clickable(true).hasDescendant(By.text(appName)));
            app.clickAndWait(Until.newWindow(), APP_LAUNCH_TIMEOUT);
            mDevice.waitForIdle();
        } else {
            throw new RuntimeException(String.format("Application %s not found", appName));
        }
    }

    @Override
    public void openBluetoothAudioApp() {
        String appName = "Bluetooth Audio";
        if (checkApplicationExists(appName)) {
            UiObject2 app = mDevice.findObject(By.clickable(true).hasDescendant(By.text(appName)));
            app.clickAndWait(Until.newWindow(), APP_LAUNCH_TIMEOUT);
            mDevice.waitForIdle();
        } else {
            throw new RuntimeException(String.format("Application %s not found", appName));
        }
    }

    @Override
    public void openGooglePlayStore() {
        openApp("Play Store");
        SystemClock.sleep(APP_LAUNCH_TIMEOUT);
    }

    private UiObject2 findApplication(String appName) {
        BySelector appSelector = By.clickable(true).hasDescendant(By.text(appName));
        if (mDevice.hasObject(SCROLLABLE_APP_LIST)) {
            // App list has more than 1 page. Scroll if necessary when searching for app.
            UiObject2 down = mDevice.findObject(DOWN_BTN);
            UiObject2 up = mDevice.findObject(UP_BTN);

            while (up.isEnabled()) {
                up.click();
                mDevice.waitForIdle();
            }

            UiObject2 app = mDevice.wait(Until.findObject(appSelector), UI_WAIT_TIMEOUT);
            while (app == null && down.isEnabled()) {
                down.click();
                mDevice.waitForIdle();
                app = mDevice.wait(Until.findObject(appSelector), UI_WAIT_TIMEOUT);
            }
            return app;
        } else {
            // App list only has 1 page.
            return mDevice.findObject(appSelector);
        }
    }

    @SuppressWarnings("unused")
    @Override
    public UiObject2 openAllApps(boolean reset) {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public BySelector getAllAppsButtonSelector() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public BySelector getAllAppsSelector() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public Direction getAllAppsScrollDirection() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public UiObject2 openAllWidgets(boolean reset) {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public BySelector getAllWidgetsSelector() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public Direction getAllWidgetsScrollDirection() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public BySelector getWorkspaceSelector() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public BySelector getHotSeatSelector() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public Direction getWorkspaceScrollDirection() {
        throw new UnsupportedOperationException(
                "The feature not supported on Auto");
    }

    @SuppressWarnings("unused")
    @Override
    public long launch(String appName, String packageName) {
        openApp(appName);
        return 0;
    }
}
