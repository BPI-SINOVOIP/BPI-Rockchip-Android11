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

package android.platform.helpers;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Instrumentation;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Environment;
import android.os.RemoteException;
import android.os.SystemClock;
import android.platform.helpers.exceptions.AccountException;
import android.platform.helpers.exceptions.TestHelperException;
import android.platform.helpers.exceptions.UnknownUiException;
import android.platform.helpers.watchers.AppIsNotRespondingWatcher;
import android.support.test.launcherhelper.ILauncherStrategy;
import android.support.test.launcherhelper.LauncherStrategyFactory;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiWatcher;
import android.support.test.uiautomator.Until;
import androidx.test.InstrumentationRegistry;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;

public abstract class AbstractStandardAppHelper implements IAppHelper {
    private static final String LOG_TAG = AbstractStandardAppHelper.class.getSimpleName();
    private static final String SCREENSHOT_DIR = "apphelper-screenshots";
    private static final String FAVOR_CMD = "favor-shell-commands";
    private static final String USE_HOME_CMD = "press-home-to-exit";
    private static final String APP_IDLE_OPTION = "app-idle_ms";
    private static final String LAUNCH_TIMEOUT_OPTION = "app-launch-timeout_ms";
    private static final String ERROR_NOT_FOUND =
        "Element %s %s is not found in the application %s";

    private static final long EXIT_WAIT_TIMEOUT = TimeUnit.SECONDS.toMillis(5);

    private static File sScreenshotDirectory;

    public UiDevice mDevice;
    public Instrumentation mInstrumentation;
    public ILauncherStrategy mLauncherStrategy;
    private final KeyCharacterMap mKeyCharacterMap =
            KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
    private final boolean mFavorShellCommands;
    private final boolean mPressHomeToExit;
    private final long mAppIdle;
    private final long mLaunchTimeout;

    public AbstractStandardAppHelper(Instrumentation instr) {
        mInstrumentation = instr;
        mDevice = UiDevice.getInstance(instr);
        mFavorShellCommands =
                Boolean.valueOf(
                        InstrumentationRegistry.getArguments().getString(FAVOR_CMD, "false"));
        mPressHomeToExit =
                Boolean.valueOf(
                        InstrumentationRegistry.getArguments().getString(USE_HOME_CMD, "false"));
        mAppIdle =
                Long.valueOf(
                        InstrumentationRegistry.getArguments()
                                .getString(
                                        APP_IDLE_OPTION,
                                        String.valueOf(TimeUnit.SECONDS.toMillis(0))));
        //TODO(b/127356533): Choose a sensible default for app launch timeout after b/125356281.
        mLaunchTimeout =
                Long.valueOf(
                        InstrumentationRegistry.getArguments()
                                .getString(
                                        LAUNCH_TIMEOUT_OPTION,
                                        String.valueOf(TimeUnit.SECONDS.toMillis(30))));
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void open() {
        // Turn on the screen if necessary.
        try {
            if (!mDevice.isScreenOn()) {
                mDevice.wakeUp();
            }
        } catch (RemoteException e) {
            throw new TestHelperException("Could not unlock the device.", e);
        }
        // Unlock the screen if necessary.
        if (mDevice.hasObject(By.res("com.android.systemui", "keyguard_bottom_area"))) {
            mDevice.pressMenu();
            mDevice.waitForIdle();
        }
        // Launch the application as normal.
        String pkg = getPackage();
        long launchInitiationTimeMs = System.currentTimeMillis();

        registerDialogWatchers();
        if (mFavorShellCommands) {
            String output = null;
            try {
                Log.i(LOG_TAG, String.format("Sending command to launch: %s", pkg));
                mInstrumentation.getContext().startActivity(getOpenAppIntent());
            } catch (ActivityNotFoundException e) {
                removeDialogWatchers();
                throw new TestHelperException(String.format("Failed to find package: %s", pkg), e);
            }
        } else {
            // Launch using the UI and launcher strategy.
            String id = getLauncherName();
            if (!mDevice.hasObject(By.pkg(pkg).depth(0))) {
                getLauncherStrategy().launch(id, pkg);
                Log.i(LOG_TAG, "Launched package: id=" + id + ", pkg=" + pkg);
            }
        }

        // Ensure the package is in the foreground for success.
        if (!mDevice.wait(Until.hasObject(By.pkg(pkg).depth(0)), mLaunchTimeout)) {
            removeDialogWatchers();
            throw new IllegalStateException(
                    String.format(
                            "Did not find package, %s, in foreground after %d ms.",
                            pkg, System.currentTimeMillis() - launchInitiationTimeMs));
        }
        removeDialogWatchers();
        // Idle for specified time after app launch
        idleApp();
    }

    private void idleApp() {
        if (mAppIdle != 0) {
            Log.v(LOG_TAG, String.format("Idle app for %d ms", mAppIdle));
            SystemClock.sleep(mAppIdle);
        }
    }

    /**
     * Returns the {@code Intent} used by {@code open()} to launch an {@code Activity}. The default
     * implementation launches the default {@code Activity} of the package. Override this method to
     * launch a different {@code Activity}.
     */
    public Intent getOpenAppIntent() {
        Intent intent =
                mInstrumentation
                        .getContext()
                        .getPackageManager()
                        .getLaunchIntentForPackage(getPackage());
        if (intent == null) {
            throw new IllegalStateException(
                    String.format("Failed to get intent of package: %s", getPackage()));
        }
        return intent;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void exit() {
        Log.i(LOG_TAG, "Exiting the current application.");
        if (mPressHomeToExit) {
            mDevice.pressHome();
            mDevice.waitForIdle();
        } else {
            int maxBacks = 4;
            while (!mDevice.hasObject(getLauncherStrategy().getWorkspaceSelector())
                    && maxBacks > 0) {
                mDevice.pressBack();
                mDevice.waitForIdle();
                maxBacks--;
            }

            if (maxBacks == 0) {
                mDevice.pressHome();
            }
        }
        if (!mDevice.wait(
                Until.hasObject(getLauncherStrategy().getWorkspaceSelector()), EXIT_WAIT_TIMEOUT)) {
            throw new IllegalStateException("Failed to exit the app to launcher.");
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getVersion() throws NameNotFoundException {
        String pkg = getPackage();

        if (null == pkg || pkg.isEmpty()) {
            throw new TestHelperException("Cannot find version of empty package");
        }
        PackageManager pm = mInstrumentation.getContext().getPackageManager();
        PackageInfo pInfo = pm.getPackageInfo(pkg, 0);
        String version = pInfo.versionName;
        if (null == version || version.isEmpty()) {
            throw new TestHelperException(
                    String.format("Version isn't found for package, %s", pkg));
        }

        return version;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isAppInForeground() {
        return mDevice.hasObject(By.pkg(getPackage()).depth(0));
    }

    protected int getOrientation() {
        return mInstrumentation.getContext().getResources().getConfiguration().orientation;
    }

    protected void requiresGoogleAccount() {
        if (!hasRegisteredGoogleAccount()) {
            throw new AccountException("This method requires a Google account be registered.");
        }
    }

    protected void requiresNoGoogleAccount() {
        if (hasRegisteredGoogleAccount()) {
            throw new AccountException("This method requires no Google account be registered.");
        }
    }

    protected boolean hasRegisteredGoogleAccount() {
        Context context = mInstrumentation.getContext();
        Account[] accounts = AccountManager.get(context).getAccounts();
        for (int i = 0; i < accounts.length; ++i) {
            if (accounts[i].type.equals("com.google")) {
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean captureScreenshot(String name) throws IOException {
        File scrOut = File.createTempFile(name, ".png", getScreenshotDirectory());
        File uixOut = File.createTempFile(name, ".uix", getScreenshotDirectory());
        mDevice.dumpWindowHierarchy(uixOut);
        return mDevice.takeScreenshot(scrOut);
    }

    private static File getScreenshotDirectory() {
        if (sScreenshotDirectory == null) {
            File storage = Environment.getExternalStorageDirectory();
            sScreenshotDirectory = new File(storage, SCREENSHOT_DIR);
            if (!sScreenshotDirectory.exists()) {
                if (!sScreenshotDirectory.mkdirs()) {
                    throw new TestHelperException("Failed to create a screenshot directory.");
                }
            }
        }
        return sScreenshotDirectory;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean sendTextEvents(String text, long delay) {
      Log.v(LOG_TAG, String.format("Sending text events for %s", text));
      KeyEvent[] events = mKeyCharacterMap.getEvents(text.toCharArray());
      for (KeyEvent event : events) {
        if (KeyEvent.ACTION_DOWN == event.getAction()) {
          if (!mDevice.pressKeyCode(event.getKeyCode(), event.getMetaState())) {
            return false;
          }
          SystemClock.sleep(delay);
        }
      }
      return true;
    }

    protected UiObject2 findElementById(String id) {
      UiObject2 element = mDevice.findObject(By.res(getPackage(), id));
      if (element != null) {
        return element;
      } else {
        throw new UnknownUiException(String.format(ERROR_NOT_FOUND, "with id", id, getPackage()));
      }
    }

    protected UiObject2 findElementByText(String text) {
      UiObject2 element = mDevice.findObject(By.text(text));
      if (element != null) {
        return element;
      } else {
        throw new UnknownUiException(
            String.format(ERROR_NOT_FOUND, "with text", text, getPackage()));
      }
    }

    protected UiObject2 findElementByDescription(String description) {
      UiObject2 element = mDevice.findObject(By.desc(description));
      if (element != null) {
        return element;
      } else {
        throw new UnknownUiException(
            String.format(ERROR_NOT_FOUND, "with description", description, getPackage()));
      }
    }

    protected void clickOn(UiObject2 element) {
      if (element != null) {
        element.click();
      } else {
        throw new UnknownUiException(String.format(ERROR_NOT_FOUND, "", "", getPackage()));
      }
    }

    protected void waitAndClickById(String packageStr, String id, long timeout) {
      clickOn(mDevice.wait(Until.findObject(By.res(packageStr, id)), timeout));
    }

    protected void waitAndClickByText(String text, long timeout) {
      clickOn(mDevice.wait(Until.findObject(By.text(text)), timeout));
    }

    protected void waitAndClickByDescription(String description, long timeout) {
      clickOn(mDevice.wait(Until.findObject(By.desc(description)), timeout));
    }


    protected void checkElementWithIdExists(String packageStr, String id, long timeout) {
      if (!mDevice.wait(Until.hasObject(By.res(packageStr, id)), timeout)) {
        throw new UnknownUiException(String.format(ERROR_NOT_FOUND, "with id", id, getPackage()));
        }
    }

    protected void checkElementWithTextExists(String text, long timeout) {
      if (!mDevice.wait(Until.hasObject(By.text(text)), timeout)) {
        throw new UnknownUiException(
            String.format(ERROR_NOT_FOUND, "with text", text, getPackage()));
        }
    }

    protected void checkElementWithDescriptionExists(String description, long timeout) {
      if (!mDevice.wait(Until.hasObject(By.desc(description)), timeout)) {
        throw new UnknownUiException(
            String.format(ERROR_NOT_FOUND, "with description", description, getPackage()));
      }
    }

    protected void checkIfElementChecked(UiObject2 element) {
      if (!element.isChecked()) {
        throw new UnknownUiException("Element " + element + " is not checked");
      }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void registerWatcher(String name, UiWatcher watcher) {
        mDevice.registerWatcher(name, watcher);
    }

    /**
    * {@inheritDoc}
    */
    @Override
    public void removeWatcher(String name) {
      mDevice.removeWatcher(name);
    }

    private void registerDialogWatchers() {
        registerWatcher(
                AppIsNotRespondingWatcher.class.getSimpleName(),
                new AppIsNotRespondingWatcher(InstrumentationRegistry.getInstrumentation()));
    }

    private void removeDialogWatchers() {
        removeWatcher(AppIsNotRespondingWatcher.class.getSimpleName());
    }

    private ILauncherStrategy getLauncherStrategy() {
        if (mLauncherStrategy == null) {
            mLauncherStrategy = LauncherStrategyFactory.getInstance(mDevice).getLauncherStrategy();
        }
        return mLauncherStrategy;
    }
}
