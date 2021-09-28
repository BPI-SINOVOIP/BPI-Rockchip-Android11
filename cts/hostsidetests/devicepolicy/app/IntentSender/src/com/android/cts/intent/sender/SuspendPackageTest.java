package com.android.cts.intent.sender;

import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.os.UserHandle;
import android.provider.Settings;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.test.InstrumentationTestCase;

public class SuspendPackageTest extends InstrumentationTestCase {
    private static final int WAIT_DIALOG_TIMEOUT_IN_MS = 5000;
    private static final BySelector POPUP_TITLE_WATCH_SELECTOR = By
            .clazz(android.widget.TextView.class.getName())
            .res("android:id/alertTitle")
            .pkg("com.google.android.apps.wearable.settings");

    private static final BySelector SUSPEND_BUTTON_SELECTOR = By
            .clazz(android.widget.Button.class.getName())
            .res("android:id/button1");

    private IntentSenderActivity mActivity;
    private Context mContext;
    private PackageManager mPackageManager;
    private UiAutomation mUiAutomation;

    private static final String INTENT_RECEIVER_PKG = "com.android.cts.intent.receiver";
    private static final String TARGET_ACTIVITY_NAME
            = "com.android.cts.intent.receiver.SimpleIntentReceiverActivity";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        mActivity = launchActivity(mContext.getPackageName(), IntentSenderActivity.class, null);
        mPackageManager = mContext.getPackageManager();
        mUiAutomation = getInstrumentation().getUiAutomation();
    }

    @Override
    public void tearDown() throws Exception {
        mActivity.finish();
        super.tearDown();
    }

    public void testPackageSuspended() throws Exception {
        assertPackageSuspended(true, false);
    }

    public void testPackageNotSuspended() throws Exception {
        assertPackageSuspended(false, false);
    }

    public void testPackageSuspendedWithPackageManager() throws Exception {
        assertPackageSuspended(true, true);
    }

    /**
     * Verify that the package is suspended by trying to start the activity inside it. If the
     * package is not suspended, the target activity will return the result.
     */
    private void assertPackageSuspended(boolean suspended, boolean customDialog) throws Exception {
        Intent intent = new Intent();
        intent.setClassName(INTENT_RECEIVER_PKG, TARGET_ACTIVITY_NAME);
        Intent result = mActivity.getResult(intent);
        if (suspended) {
            if (customDialog) {
                dismissCustomDialog();
            } else {
                dismissPolicyTransparencyDialog();
            }
            assertNull(result);
        } else {
            assertNotNull(result);
        }
        // No matter if it is suspended or not, we should be able to resolve the activity.
        assertNotNull(mPackageManager.resolveActivity(intent, 0));
    }

    /**
     * Wait for the policy transparency dialog and dismiss it.
     */
    private void dismissPolicyTransparencyDialog() {
        final UiDevice device = UiDevice.getInstance(getInstrumentation());
        if (isWatch()) {
            device.wait(Until.hasObject(POPUP_TITLE_WATCH_SELECTOR), WAIT_DIALOG_TIMEOUT_IN_MS);
            final UiObject2 title = device.findObject(POPUP_TITLE_WATCH_SELECTOR);
            assertNotNull("Policy transparency dialog title not found", title);
            title.swipe(Direction.RIGHT, 1.0f);
        } else {
            device.wait(Until.hasObject(getPopUpImageSelector()), WAIT_DIALOG_TIMEOUT_IN_MS);
            final UiObject2 icon = device.findObject(getPopUpImageSelector());
            assertNotNull("Policy transparency dialog icon not found", icon);
            // "OK" button only present in the dialog if it is blocked by policy.
            final UiObject2 button = device.findObject(getPopUpButtonSelector());
            assertNotNull("OK button not found", button);
            button.click();
        }
    }

    private void dismissCustomDialog() {
        final UiDevice device = UiDevice.getInstance(getInstrumentation());
        device.wait(Until.hasObject(SUSPEND_BUTTON_SELECTOR), WAIT_DIALOG_TIMEOUT_IN_MS);

        final UiObject2 button = device.findObject(SUSPEND_BUTTON_SELECTOR);
        assertNotNull("OK button not found", button);
    }

    private boolean isWatch() {
        return (getInstrumentation().getContext().getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_WATCH;
    }

    private String getSettingsPackageName() {
        String settingsPackageName = "com.android.settings";
        try {
            mUiAutomation.adoptShellPermissionIdentity("android.permission.INTERACT_ACROSS_USERS");
            ResolveInfo resolveInfo = mPackageManager.resolveActivityAsUser(
                    new Intent(Settings.ACTION_SETTINGS), PackageManager.MATCH_SYSTEM_ONLY,
                    UserHandle.USER_SYSTEM);
            if (resolveInfo != null && resolveInfo.activityInfo != null) {
                settingsPackageName = resolveInfo.activityInfo.packageName;
            }
        } finally {
            mUiAutomation.dropShellPermissionIdentity();
        }
        return settingsPackageName;
    }

    private BySelector getPopUpButtonSelector() {
        return By.clazz(android.widget.Button.class.getName())
                .res("android:id/button1")
                .pkg(getSettingsPackageName());
    }

    private BySelector getPopUpImageSelector() {
        final String settingsPackageName = getSettingsPackageName();
        return By.clazz(android.widget.ImageView.class.getName())
                .res(settingsPackageName + ":id/admin_support_icon")
                .pkg(settingsPackageName);
    }
}
