/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.autofillservice.cts;

import static android.autofillservice.cts.Timeouts.DATASET_PICKER_NOT_SHOWN_NAPTIME_MS;
import static android.autofillservice.cts.Timeouts.LONG_PRESS_MS;
import static android.autofillservice.cts.Timeouts.SAVE_NOT_SHOWN_NAPTIME_MS;
import static android.autofillservice.cts.Timeouts.SAVE_TIMEOUT;
import static android.autofillservice.cts.Timeouts.UI_DATASET_PICKER_TIMEOUT;
import static android.autofillservice.cts.Timeouts.UI_SCREEN_ORIENTATION_TIMEOUT;
import static android.autofillservice.cts.Timeouts.UI_TIMEOUT;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_ADDRESS;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_CREDIT_CARD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_DEBIT_CARD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_EMAIL_ADDRESS;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_GENERIC;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_GENERIC_CARD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PAYMENT_CARD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_USERNAME;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.service.autofill.SaveInfo;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.SearchCondition;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.text.Html;
import android.text.Spanned;
import android.text.style.URLSpan;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.RetryableException;
import com.android.compatibility.common.util.Timeout;

import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Helper for UI-related needs.
 */
public class UiBot {

    private static final String TAG = "AutoFillCtsUiBot";

    private static final String RESOURCE_ID_DATASET_PICKER = "autofill_dataset_picker";
    private static final String RESOURCE_ID_DATASET_HEADER = "autofill_dataset_header";
    private static final String RESOURCE_ID_SAVE_SNACKBAR = "autofill_save";
    private static final String RESOURCE_ID_SAVE_ICON = "autofill_save_icon";
    private static final String RESOURCE_ID_SAVE_TITLE = "autofill_save_title";
    private static final String RESOURCE_ID_CONTEXT_MENUITEM = "floating_toolbar_menu_item_text";
    private static final String RESOURCE_ID_SAVE_BUTTON_NO = "autofill_save_no";
    private static final String RESOURCE_ID_SAVE_BUTTON_YES = "autofill_save_yes";
    private static final String RESOURCE_ID_OVERFLOW = "overflow";

    private static final String RESOURCE_STRING_SAVE_TITLE = "autofill_save_title";
    private static final String RESOURCE_STRING_SAVE_TITLE_WITH_TYPE =
            "autofill_save_title_with_type";
    private static final String RESOURCE_STRING_SAVE_TYPE_PASSWORD = "autofill_save_type_password";
    private static final String RESOURCE_STRING_SAVE_TYPE_ADDRESS = "autofill_save_type_address";
    private static final String RESOURCE_STRING_SAVE_TYPE_CREDIT_CARD =
            "autofill_save_type_credit_card";
    private static final String RESOURCE_STRING_SAVE_TYPE_USERNAME = "autofill_save_type_username";
    private static final String RESOURCE_STRING_SAVE_TYPE_EMAIL_ADDRESS =
            "autofill_save_type_email_address";
    private static final String RESOURCE_STRING_SAVE_TYPE_DEBIT_CARD =
            "autofill_save_type_debit_card";
    private static final String RESOURCE_STRING_SAVE_TYPE_PAYMENT_CARD =
            "autofill_save_type_payment_card";
    private static final String RESOURCE_STRING_SAVE_TYPE_GENERIC_CARD =
            "autofill_save_type_generic_card";
    private static final String RESOURCE_STRING_SAVE_BUTTON_NEVER = "autofill_save_never";
    private static final String RESOURCE_STRING_SAVE_BUTTON_NOT_NOW = "autofill_save_notnow";
    private static final String RESOURCE_STRING_SAVE_BUTTON_NO_THANKS = "autofill_save_no";
    private static final String RESOURCE_STRING_SAVE_BUTTON_YES = "autofill_save_yes";
    private static final String RESOURCE_STRING_UPDATE_BUTTON_YES = "autofill_update_yes";
    private static final String RESOURCE_STRING_CONTINUE_BUTTON_YES = "autofill_continue_yes";
    private static final String RESOURCE_STRING_UPDATE_TITLE = "autofill_update_title";
    private static final String RESOURCE_STRING_UPDATE_TITLE_WITH_TYPE =
            "autofill_update_title_with_type";

    private static final String RESOURCE_STRING_AUTOFILL = "autofill";
    private static final String RESOURCE_STRING_DATASET_PICKER_ACCESSIBILITY_TITLE =
            "autofill_picker_accessibility_title";
    private static final String RESOURCE_STRING_SAVE_SNACKBAR_ACCESSIBILITY_TITLE =
            "autofill_save_accessibility_title";


    static final BySelector DATASET_PICKER_SELECTOR = By.res("android", RESOURCE_ID_DATASET_PICKER);
    private static final BySelector SAVE_UI_SELECTOR = By.res("android", RESOURCE_ID_SAVE_SNACKBAR);
    private static final BySelector DATASET_HEADER_SELECTOR =
            By.res("android", RESOURCE_ID_DATASET_HEADER);

    // TODO: figure out a more reliable solution that does not depend on SystemUI resources.
    private static final String SPLIT_WINDOW_DIVIDER_ID =
            "com.android.systemui:id/docked_divider_background";

    private static final boolean DUMP_ON_ERROR = true;

    /** Pass to {@link #setScreenOrientation(int)} to change the display to portrait mode */
    public static int PORTRAIT = 0;

    /** Pass to {@link #setScreenOrientation(int)} to change the display to landscape mode */
    public static int LANDSCAPE = 1;

    private final UiDevice mDevice;
    private final Context mContext;
    private final String mPackageName;
    private final UiAutomation mAutoman;
    private final Timeout mDefaultTimeout;

    private boolean mOkToCallAssertNoDatasets;

    public UiBot() {
        this(UI_TIMEOUT);
    }

    public UiBot(Timeout defaultTimeout) {
        mDefaultTimeout = defaultTimeout;
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mDevice = UiDevice.getInstance(instrumentation);
        mContext = instrumentation.getContext();
        mPackageName = mContext.getPackageName();
        mAutoman = instrumentation.getUiAutomation();
    }

    public void waitForIdle() {
        final long before = SystemClock.elapsedRealtimeNanos();
        mDevice.waitForIdle();
        final float delta = ((float) (SystemClock.elapsedRealtimeNanos() - before)) / 1_000_000;
        Log.v(TAG, "device idle in " + delta + "ms");
    }

    public void waitForIdleSync() {
        final long before = SystemClock.elapsedRealtimeNanos();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        final float delta = ((float) (SystemClock.elapsedRealtimeNanos() - before)) / 1_000_000;
        Log.v(TAG, "device idle sync in " + delta + "ms");
    }

    public void reset() {
        mOkToCallAssertNoDatasets = false;
    }

    /**
     * Assumes the device has a minimum height and width of {@code minSize}, throwing a
     * {@code AssumptionViolatedException} if it doesn't (so the test is skiped by the JUnit
     * Runner).
     */
    public void assumeMinimumResolution(int minSize) {
        final int width = mDevice.getDisplayWidth();
        final int heigth = mDevice.getDisplayHeight();
        final int min = Math.min(width, heigth);
        assumeTrue("Screen size is too small (" + width + "x" + heigth + ")", min >= minSize);
        Log.d(TAG, "assumeMinimumResolution(" + minSize + ") passed: screen size is "
                + width + "x" + heigth);
    }

    /**
     * Sets the screen resolution in a way that the IME doesn't interfere with the Autofill UI
     * when the device is rotated to landscape.
     *
     * When called, test must call <p>{@link #resetScreenResolution()} in a {@code finally} block.
     *
     * @deprecated this method should not be necessarily anymore as we're using a MockIme.
     */
    @Deprecated
    // TODO: remove once we're sure no more OEM is getting failure due to screen size
    public void setScreenResolution() {
        if (true) {
            Log.w(TAG, "setScreenResolution(): ignored");
            return;
        }
        assumeMinimumResolution(500);

        runShellCommand("wm size 1080x1920");
        runShellCommand("wm density 320");
    }

    /**
     * Resets the screen resolution.
     *
     * <p>Should always be called after {@link #setScreenResolution()}.
     *
     * @deprecated this method should not be necessarily anymore as we're using a MockIme.
     */
    @Deprecated
    // TODO: remove once we're sure no more OEM is getting failure due to screen size
    public void resetScreenResolution() {
        if (true) {
            Log.w(TAG, "resetScreenResolution(): ignored");
            return;
        }
        runShellCommand("wm density reset");
        runShellCommand("wm size reset");
    }

    /**
     * Asserts the dataset picker is not shown anymore.
     *
     * @throws IllegalStateException if called *before* an assertion was made to make sure the
     * dataset picker is shown - if that's not the case, call
     * {@link #assertNoDatasetsEver()} instead.
     */
    public void assertNoDatasets() throws Exception {
        if (!mOkToCallAssertNoDatasets) {
            throw new IllegalStateException(
                    "Cannot call assertNoDatasets() without calling assertDatasets first");
        }
        mDevice.wait(Until.gone(DATASET_PICKER_SELECTOR), UI_DATASET_PICKER_TIMEOUT.ms());
        mOkToCallAssertNoDatasets = false;
    }

    /**
     * Asserts the dataset picker was never shown.
     *
     * <p>This method is slower than {@link #assertNoDatasets()} and should only be called in the
     * cases where the dataset picker was not previous shown.
     */
    public void assertNoDatasetsEver() throws Exception {
        assertNeverShown("dataset picker", DATASET_PICKER_SELECTOR,
                DATASET_PICKER_NOT_SHOWN_NAPTIME_MS);
    }

    /**
     * Asserts the dataset chooser is shown and contains exactly the given datasets.
     *
     * @return the dataset picker object.
     */
    public UiObject2 assertDatasets(String...names) throws Exception {
        final UiObject2 picker = findDatasetPicker(UI_DATASET_PICKER_TIMEOUT);
        return assertDatasets(picker, names);
    }

    protected UiObject2 assertDatasets(UiObject2 picker, String...names) {
        assertWithMessage("wrong dataset names").that(getChildrenAsText(picker))
                .containsExactlyElementsIn(Arrays.asList(names)).inOrder();
        return picker;
    }

    /**
     * Asserts the dataset chooser is shown and contains the given datasets.
     *
     * @return the dataset picker object.
     */
    public UiObject2 assertDatasetsContains(String...names) throws Exception {
        final UiObject2 picker = findDatasetPicker(UI_DATASET_PICKER_TIMEOUT);
        assertWithMessage("wrong dataset names").that(getChildrenAsText(picker))
                .containsAllIn(Arrays.asList(names)).inOrder();
        return picker;
    }

    /**
     * Asserts the dataset chooser is shown and contains the given datasets, header, and footer.
     * <p>In fullscreen, header view is not under R.id.autofill_dataset_picker.
     *
     * @return the dataset picker object.
     */
    public UiObject2 assertDatasetsWithBorders(String header, String footer, String...names)
            throws Exception {
        final UiObject2 picker = findDatasetPicker(UI_DATASET_PICKER_TIMEOUT);
        final List<String> expectedChild = new ArrayList<>();
        if (header != null) {
            if (Helper.isAutofillWindowFullScreen(mContext)) {
                final UiObject2 headerView = waitForObject(DATASET_HEADER_SELECTOR,
                        UI_DATASET_PICKER_TIMEOUT);
                assertWithMessage("fullscreen wrong dataset header")
                        .that(getChildrenAsText(headerView))
                        .containsExactlyElementsIn(Arrays.asList(header)).inOrder();
            } else {
                expectedChild.add(header);
            }
        }
        expectedChild.addAll(Arrays.asList(names));
        if (footer != null) {
            expectedChild.add(footer);
        }
        assertWithMessage("wrong elements on dataset picker").that(getChildrenAsText(picker))
                .containsExactlyElementsIn(expectedChild).inOrder();
        return picker;
    }

    /**
     * Gets the text of this object children.
     */
    public List<String> getChildrenAsText(UiObject2 object) {
        final List<String> list = new ArrayList<>();
        getChildrenAsText(object, list);
        return list;
    }

    private static void getChildrenAsText(UiObject2 object, List<String> children) {
        final String text = object.getText();
        if (text != null) {
            children.add(text);
        }
        for (UiObject2 child : object.getChildren()) {
            getChildrenAsText(child, children);
        }
    }

    /**
     * Selects a dataset that should be visible in the floating UI and does not need to wait for
     * application become idle.
     */
    public void selectDataset(String name) throws Exception {
        final UiObject2 picker = findDatasetPicker(UI_DATASET_PICKER_TIMEOUT);
        selectDataset(picker, name);
    }

    /**
     * Selects a dataset that should be visible in the floating UI and waits for application become
     * idle if needed.
     */
    public void selectDatasetSync(String name) throws Exception {
        final UiObject2 picker = findDatasetPicker(UI_DATASET_PICKER_TIMEOUT);
        selectDataset(picker, name);
        mDevice.waitForIdle();
    }

    /**
     * Selects a dataset that should be visible in the floating UI.
     */
    public void selectDataset(UiObject2 picker, String name) {
        final UiObject2 dataset = picker.findObject(By.text(name));
        if (dataset == null) {
            throw new AssertionError("no dataset " + name + " in " + getChildrenAsText(picker));
        }
        dataset.click();
    }

    /**
     * Finds the suggestion by name and perform long click on suggestion to trigger attribution
     * intent.
     */
    public void longPressSuggestion(String name) throws Exception {
        throw new UnsupportedOperationException();
    }

    /**
     * Asserts the suggestion chooser is shown in the suggestion view.
     */
    public void assertSuggestion(String name) throws Exception {
        throw new UnsupportedOperationException();
    }

    /**
     * Asserts the suggestion chooser is not shown in the suggestion view.
     */
    public void assertNoSuggestion(String name) throws Exception {
        throw new UnsupportedOperationException();
    }

    /**
     * Scrolls the suggestion view.
     *
     * @param direction The direction to scroll.
     * @param speed The speed to scroll per second.
     */
    public void scrollSuggestionView(Direction direction, int speed) throws Exception {
        throw new UnsupportedOperationException();
    }

    /**
     * Selects a view by text.
     *
     * <p><b>NOTE:</b> when selecting an option in dataset picker is shown, prefer
     * {@link #selectDataset(String)}.
     */
    public void selectByText(String name) throws Exception {
        Log.v(TAG, "selectByText(): " + name);

        final UiObject2 object = waitForObject(By.text(name));
        object.click();
    }

    /**
     * Asserts a text is shown.
     *
     * <p><b>NOTE:</b> when asserting the dataset picker is shown, prefer
     * {@link #assertDatasets(String...)}.
     */
    public UiObject2 assertShownByText(String text) throws Exception {
        return assertShownByText(text, mDefaultTimeout);
    }

    public UiObject2 assertShownByText(String text, Timeout timeout) throws Exception {
        final UiObject2 object = waitForObject(By.text(text), timeout);
        assertWithMessage("No node with text '%s'", text).that(object).isNotNull();
        return object;
    }

    /**
     * Finds a node by text, without waiting for it to be shown (but failing if it isn't).
     */
    @NonNull
    public UiObject2 findRightAwayByText(@NonNull String text) throws Exception {
        final UiObject2 object = mDevice.findObject(By.text(text));
        assertWithMessage("no UIObject for text '%s'", text).that(object).isNotNull();
        return object;
    }

    /**
     * Asserts that the text is not showing for sure in the screen "as is", i.e., without waiting
     * for it.
     *
     * <p>Typically called after another assertion that waits for a condition to be shown.
     */
    public void assertNotShowingForSure(String text) throws Exception {
        final UiObject2 object = mDevice.findObject(By.text(text));
        assertWithMessage("Found node with text '%s'", text).that(object).isNull();
    }

    /**
     * Asserts a node with the given content description is shown.
     *
     */
    public UiObject2 assertShownByContentDescription(String contentDescription) throws Exception {
        final UiObject2 object = waitForObject(By.desc(contentDescription));
        assertWithMessage("No node with content description '%s'", contentDescription).that(object)
                .isNotNull();
        return object;
    }

    /**
     * Checks if a View with a certain text exists.
     */
    public boolean hasViewWithText(String name) {
        Log.v(TAG, "hasViewWithText(): " + name);

        return mDevice.findObject(By.text(name)) != null;
    }

    /**
     * Selects a view by id.
     */
    public UiObject2 selectByRelativeId(String id) throws Exception {
        Log.v(TAG, "selectByRelativeId(): " + id);
        UiObject2 object = waitForObject(By.res(mPackageName, id));
        object.click();
        return object;
    }

    /**
     * Asserts the id is shown on the screen.
     */
    public UiObject2 assertShownById(String id) throws Exception {
        final UiObject2 object = waitForObject(By.res(id));
        assertThat(object).isNotNull();
        return object;
    }

    /**
     * Asserts the id is shown on the screen, using a resource id from the test package.
     */
    public UiObject2 assertShownByRelativeId(String id) throws Exception {
        return assertShownByRelativeId(id, mDefaultTimeout);
    }

    public UiObject2 assertShownByRelativeId(String id, Timeout timeout) throws Exception {
        final UiObject2 obj = waitForObject(By.res(mPackageName, id), timeout);
        assertThat(obj).isNotNull();
        return obj;
    }

    /**
     * Asserts the id is not shown on the screen anymore, using a resource id from the test package.
     *
     * <p><b>Note:</b> this method should only called AFTER the id was previously shown, otherwise
     * it might pass without really asserting anything.
     */
    public void assertGoneByRelativeId(@NonNull String id, @NonNull Timeout timeout) {
        assertGoneByRelativeId(/* parent = */ null, id, timeout);
    }

    public void assertGoneByRelativeId(int resId, @NonNull Timeout timeout) {
        assertGoneByRelativeId(/* parent = */ null, getIdName(resId), timeout);
    }

    private String getIdName(int resId) {
        return mContext.getResources().getResourceEntryName(resId);
    }

    /**
     * Asserts the id is not shown on the parent anymore, using a resource id from the test package.
     *
     * <p><b>Note:</b> this method should only called AFTER the id was previously shown, otherwise
     * it might pass without really asserting anything.
     */
    public void assertGoneByRelativeId(@Nullable UiObject2 parent, @NonNull String id,
            @NonNull Timeout timeout) {
        final SearchCondition<Boolean> condition = Until.gone(By.res(mPackageName, id));
        final boolean gone = parent != null
                ? parent.wait(condition, timeout.ms())
                : mDevice.wait(condition, timeout.ms());
        if (!gone) {
            final String message = "Object with id '" + id + "' should be gone after "
                    + timeout + " ms";
            dumpScreen(message);
            throw new RetryableException(message);
        }
    }

    public UiObject2 assertShownByRelativeId(int resId) throws Exception {
        return assertShownByRelativeId(getIdName(resId));
    }

    public void assertNeverShownByRelativeId(@NonNull String description, int resId, long timeout)
            throws Exception {
        final BySelector selector = By.res(Helper.MY_PACKAGE, getIdName(resId));
        assertNeverShown(description, selector, timeout);
    }

    /**
     * Asserts that a {@code selector} is not showing after {@code timeout} milliseconds.
     */
    protected void assertNeverShown(String description, BySelector selector, long timeout)
            throws Exception {
        SystemClock.sleep(timeout);
        final UiObject2 object = mDevice.findObject(selector);
        if (object != null) {
            throw new AssertionError(
                    String.format("Should not be showing %s after %dms, but got %s",
                            description, timeout, getChildrenAsText(object)));
        }
    }

    /**
     * Gets the text set on a view.
     */
    public String getTextByRelativeId(String id) throws Exception {
        return waitForObject(By.res(mPackageName, id)).getText();
    }

    /**
     * Focus in the view with the given resource id.
     */
    public void focusByRelativeId(String id) throws Exception {
        waitForObject(By.res(mPackageName, id)).click();
    }

    /**
     * Sets a new text on a view.
     */
    public void setTextByRelativeId(String id, String newText) throws Exception {
        waitForObject(By.res(mPackageName, id)).setText(newText);
    }

    /**
     * Asserts the save snackbar is showing and returns it.
     */
    public UiObject2 assertSaveShowing(int type) throws Exception {
        return assertSaveShowing(SAVE_TIMEOUT, type);
    }

    /**
     * Asserts the save snackbar is showing and returns it.
     */
    public UiObject2 assertSaveShowing(Timeout timeout, int type) throws Exception {
        return assertSaveShowing(null, timeout, type);
    }

    /**
     * Asserts the save snackbar is showing with the Update message and returns it.
     */
    public UiObject2 assertUpdateShowing(int... types) throws Exception {
        return assertSaveOrUpdateShowing(/* update= */ true, SaveInfo.NEGATIVE_BUTTON_STYLE_CANCEL,
                null, SAVE_TIMEOUT, types);
    }

    /**
     * Presses the Back button.
     */
    public void pressBack() {
        Log.d(TAG, "pressBack()");
        mDevice.pressBack();
    }

    /**
     * Presses the Home button.
     */
    public void pressHome() {
        Log.d(TAG, "pressHome()");
        mDevice.pressHome();
    }

    /**
     * Asserts the save snackbar is not showing.
     */
    public void assertSaveNotShowing(int type) throws Exception {
        assertNeverShown("save UI for type " + type, SAVE_UI_SELECTOR, SAVE_NOT_SHOWN_NAPTIME_MS);
    }

    public void assertSaveNotShowing() throws Exception {
        assertNeverShown("save UI", SAVE_UI_SELECTOR, SAVE_NOT_SHOWN_NAPTIME_MS);
    }

    private String getSaveTypeString(int type) {
        final String typeResourceName;
        switch (type) {
            case SAVE_DATA_TYPE_PASSWORD:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_PASSWORD;
                break;
            case SAVE_DATA_TYPE_ADDRESS:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_ADDRESS;
                break;
            case SAVE_DATA_TYPE_CREDIT_CARD:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_CREDIT_CARD;
                break;
            case SAVE_DATA_TYPE_USERNAME:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_USERNAME;
                break;
            case SAVE_DATA_TYPE_EMAIL_ADDRESS:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_EMAIL_ADDRESS;
                break;
            case SAVE_DATA_TYPE_DEBIT_CARD:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_DEBIT_CARD;
                break;
            case SAVE_DATA_TYPE_PAYMENT_CARD:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_PAYMENT_CARD;
                break;
            case SAVE_DATA_TYPE_GENERIC_CARD:
                typeResourceName = RESOURCE_STRING_SAVE_TYPE_GENERIC_CARD;
                break;
            default:
                throw new IllegalArgumentException("Unsupported type: " + type);
        }
        return getString(typeResourceName);
    }

    public UiObject2 assertSaveShowing(String description, int... types) throws Exception {
        return assertSaveOrUpdateShowing(/* update= */ false, SaveInfo.NEGATIVE_BUTTON_STYLE_CANCEL,
                description, SAVE_TIMEOUT, types);
    }

    public UiObject2 assertSaveShowing(String description, Timeout timeout, int... types)
            throws Exception {
        return assertSaveOrUpdateShowing(/* update= */ false, SaveInfo.NEGATIVE_BUTTON_STYLE_CANCEL,
                description, timeout, types);
    }

    public UiObject2 assertSaveShowing(int negativeButtonStyle, String description,
            int... types) throws Exception {
        return assertSaveOrUpdateShowing(/* update= */ false, negativeButtonStyle, description,
                SAVE_TIMEOUT, types);
    }

    public UiObject2 assertSaveShowing(int positiveButtonStyle, int... types) throws Exception {
        return assertSaveOrUpdateShowing(/* update= */ false, SaveInfo.NEGATIVE_BUTTON_STYLE_CANCEL,
                positiveButtonStyle, /* description= */ null, SAVE_TIMEOUT, types);
    }

    public UiObject2 assertSaveOrUpdateShowing(boolean update, int negativeButtonStyle,
            String description, Timeout timeout, int... types) throws Exception {
        return assertSaveOrUpdateShowing(update, negativeButtonStyle,
                SaveInfo.POSITIVE_BUTTON_STYLE_SAVE, description, timeout, types);
    }

    public UiObject2 assertSaveOrUpdateShowing(boolean update, int negativeButtonStyle,
            int positiveButtonStyle, String description, Timeout timeout, int... types)
            throws Exception {

        final UiObject2 snackbar = waitForObject(SAVE_UI_SELECTOR, timeout);

        final UiObject2 titleView =
                waitForObject(snackbar, By.res("android", RESOURCE_ID_SAVE_TITLE), timeout);
        assertWithMessage("save title (%s) is not shown", RESOURCE_ID_SAVE_TITLE).that(titleView)
                .isNotNull();

        final UiObject2 iconView =
                waitForObject(snackbar, By.res("android", RESOURCE_ID_SAVE_ICON), timeout);
        assertWithMessage("save icon (%s) is not shown", RESOURCE_ID_SAVE_ICON).that(iconView)
                .isNotNull();

        final String actualTitle = titleView.getText();
        Log.d(TAG, "save title: " + actualTitle);

        final String titleId, titleWithTypeId;
        if (update) {
            titleId = RESOURCE_STRING_UPDATE_TITLE;
            titleWithTypeId = RESOURCE_STRING_UPDATE_TITLE_WITH_TYPE;
        } else {
            titleId = RESOURCE_STRING_SAVE_TITLE;
            titleWithTypeId = RESOURCE_STRING_SAVE_TITLE_WITH_TYPE;
        }

        final String serviceLabel = InstrumentedAutoFillService.getServiceLabel();
        switch (types.length) {
            case 1:
                final String expectedTitle = (types[0] == SAVE_DATA_TYPE_GENERIC)
                        ? Html.fromHtml(getString(titleId, serviceLabel), 0).toString()
                        : Html.fromHtml(getString(titleWithTypeId,
                                getSaveTypeString(types[0]), serviceLabel), 0).toString();
                assertThat(actualTitle).isEqualTo(expectedTitle);
                break;
            case 2:
                // We cannot predict the order...
                assertThat(actualTitle).contains(getSaveTypeString(types[0]));
                assertThat(actualTitle).contains(getSaveTypeString(types[1]));
                break;
            case 3:
                // We cannot predict the order...
                assertThat(actualTitle).contains(getSaveTypeString(types[0]));
                assertThat(actualTitle).contains(getSaveTypeString(types[1]));
                assertThat(actualTitle).contains(getSaveTypeString(types[2]));
                break;
            default:
                throw new IllegalArgumentException("Invalid types: " + Arrays.toString(types));
        }

        if (description != null) {
            final UiObject2 saveSubTitle = snackbar.findObject(By.text(description));
            assertWithMessage("save subtitle(%s)", description).that(saveSubTitle).isNotNull();
        }

        final String positiveButtonStringId;
        switch (positiveButtonStyle) {
            case SaveInfo.POSITIVE_BUTTON_STYLE_CONTINUE:
                positiveButtonStringId = RESOURCE_STRING_CONTINUE_BUTTON_YES;
                break;
            default:
                positiveButtonStringId = update ? RESOURCE_STRING_UPDATE_BUTTON_YES
                        : RESOURCE_STRING_SAVE_BUTTON_YES;
        }
        final String expectedPositiveButtonText = getString(positiveButtonStringId).toUpperCase();
        final UiObject2 positiveButton = waitForObject(snackbar,
                By.res("android", RESOURCE_ID_SAVE_BUTTON_YES), timeout);
        assertWithMessage("wrong text on positive button")
                .that(positiveButton.getText().toUpperCase()).isEqualTo(expectedPositiveButtonText);

        final String negativeButtonStringId;
        if (negativeButtonStyle == SaveInfo.NEGATIVE_BUTTON_STYLE_REJECT) {
            negativeButtonStringId = RESOURCE_STRING_SAVE_BUTTON_NOT_NOW;
        } else if (negativeButtonStyle == SaveInfo.NEGATIVE_BUTTON_STYLE_NEVER) {
            negativeButtonStringId = RESOURCE_STRING_SAVE_BUTTON_NEVER;
        } else {
            negativeButtonStringId = RESOURCE_STRING_SAVE_BUTTON_NO_THANKS;
        }
        final String expectedNegativeButtonText = getString(negativeButtonStringId).toUpperCase();
        final UiObject2 negativeButton = waitForObject(snackbar,
                By.res("android", RESOURCE_ID_SAVE_BUTTON_NO), timeout);
        assertWithMessage("wrong text on negative button")
                .that(negativeButton.getText().toUpperCase()).isEqualTo(expectedNegativeButtonText);

        final String expectedAccessibilityTitle =
                getString(RESOURCE_STRING_SAVE_SNACKBAR_ACCESSIBILITY_TITLE);
        assertAccessibilityTitle(snackbar, expectedAccessibilityTitle);

        return snackbar;
    }

    /**
     * Taps an option in the save snackbar.
     *
     * @param yesDoIt {@code true} for 'YES', {@code false} for 'NO THANKS'.
     * @param types expected types of save info.
     */
    public void saveForAutofill(boolean yesDoIt, int... types) throws Exception {
        final UiObject2 saveSnackBar = assertSaveShowing(
                SaveInfo.NEGATIVE_BUTTON_STYLE_CANCEL, null, types);
        saveForAutofill(saveSnackBar, yesDoIt);
    }

    public void updateForAutofill(boolean yesDoIt, int... types) throws Exception {
        final UiObject2 saveUi = assertUpdateShowing(types);
        saveForAutofill(saveUi, yesDoIt);
    }

    /**
     * Taps an option in the save snackbar.
     *
     * @param yesDoIt {@code true} for 'YES', {@code false} for 'NO THANKS'.
     * @param types expected types of save info.
     */
    public void saveForAutofill(int negativeButtonStyle, boolean yesDoIt, int... types)
            throws Exception {
        final UiObject2 saveSnackBar = assertSaveShowing(negativeButtonStyle, null, types);
        saveForAutofill(saveSnackBar, yesDoIt);
    }

    /**
     * Taps the positive button in the save snackbar.
     *
     * @param types expected types of save info.
     */
    public void saveForAutofill(int positiveButtonStyle, int... types) throws Exception {
        final UiObject2 saveSnackBar = assertSaveShowing(positiveButtonStyle, types);
        saveForAutofill(saveSnackBar, /* yesDoIt= */ true);
    }

    /**
     * Taps an option in the save snackbar.
     *
     * @param saveSnackBar Save snackbar, typically obtained through
     *            {@link #assertSaveShowing(int)}.
     * @param yesDoIt {@code true} for 'YES', {@code false} for 'NO THANKS'.
     */
    public void saveForAutofill(UiObject2 saveSnackBar, boolean yesDoIt) {
        final String id = yesDoIt ? "autofill_save_yes" : "autofill_save_no";

        final UiObject2 button = saveSnackBar.findObject(By.res("android", id));
        assertWithMessage("save button (%s)", id).that(button).isNotNull();
        button.click();
    }

    /**
     * Gets the AUTOFILL contextual menu by long pressing a text field.
     *
     * <p><b>NOTE:</b> this method should only be called in scenarios where we explicitly want to
     * test the overflow menu. For all other scenarios where we want to test manual autofill, it's
     * better to call {@code AFM.requestAutofill()} directly, because it's less error-prone and
     * faster.
     *
     * @param id resource id of the field.
     */
    public UiObject2 getAutofillMenuOption(String id) throws Exception {
        final UiObject2 field = waitForObject(By.res(mPackageName, id));
        // TODO: figure out why obj.longClick() doesn't always work
        field.click(LONG_PRESS_MS);

        List<UiObject2> menuItems = waitForObjects(
                By.res("android", RESOURCE_ID_CONTEXT_MENUITEM), mDefaultTimeout);
        final String expectedText = getAutofillContextualMenuTitle();

        final StringBuffer menuNames = new StringBuffer();

        // Check first menu for AUTOFILL
        for (UiObject2 menuItem : menuItems) {
            final String menuName = menuItem.getText();
            if (menuName.equalsIgnoreCase(expectedText)) {
                Log.v(TAG, "AUTOFILL found in first menu");
                return menuItem;
            }
            menuNames.append("'").append(menuName).append("' ");
        }

        menuNames.append(";");

        // First menu does not have AUTOFILL, check overflow
        final BySelector overflowSelector = By.res("android", RESOURCE_ID_OVERFLOW);

        // Click overflow menu button.
        final UiObject2 overflowMenu = waitForObject(overflowSelector, mDefaultTimeout);
        overflowMenu.click();

        // Wait for overflow menu to show.
        mDevice.wait(Until.gone(overflowSelector), 1000);

        menuItems = waitForObjects(
                By.res("android", RESOURCE_ID_CONTEXT_MENUITEM), mDefaultTimeout);
        for (UiObject2 menuItem : menuItems) {
            final String menuName = menuItem.getText();
            if (menuName.equalsIgnoreCase(expectedText)) {
                Log.v(TAG, "AUTOFILL found in overflow menu");
                return menuItem;
            }
            menuNames.append("'").append(menuName).append("' ");
        }
        throw new RetryableException("no '%s' on '%s'", expectedText, menuNames);
    }

    String getAutofillContextualMenuTitle() {
        return getString(RESOURCE_STRING_AUTOFILL);
    }

    /**
     * Gets a string from the Android resources.
     */
    private String getString(String id) {
        final Resources resources = mContext.getResources();
        final int stringId = resources.getIdentifier(id, "string", "android");
        try {
            return resources.getString(stringId);
        } catch (Resources.NotFoundException e) {
            throw new IllegalStateException("no internal string for '" + id + "' / res=" + stringId
                    + ": ", e);
        }
    }

    /**
     * Gets a string from the Android resources.
     */
    private String getString(String id, Object... formatArgs) {
        final Resources resources = mContext.getResources();
        final int stringId = resources.getIdentifier(id, "string", "android");
        try {
            return resources.getString(stringId, formatArgs);
        } catch (Resources.NotFoundException e) {
            throw new IllegalStateException("no internal string for '" + id + "' / res=" + stringId
                    + ": ", e);
        }
    }

    /**
     * Waits for and returns an object.
     *
     * @param selector {@link BySelector} that identifies the object.
     */
    private UiObject2 waitForObject(BySelector selector) throws Exception {
        return waitForObject(selector, mDefaultTimeout);
    }

    /**
     * Waits for and returns an object.
     *
     * @param parent where to find the object (or {@code null} to use device's root).
     * @param selector {@link BySelector} that identifies the object.
     * @param timeout timeout in ms.
     * @param dumpOnError whether the window hierarchy should be dumped if the object is not found.
     */
    private UiObject2 waitForObject(UiObject2 parent, BySelector selector, Timeout timeout,
            boolean dumpOnError) throws Exception {
        // NOTE: mDevice.wait does not work for the save snackbar, so we need a polling approach.
        try {
            return timeout.run("waitForObject(" + selector + ")", () -> {
                return parent != null
                        ? parent.findObject(selector)
                        : mDevice.findObject(selector);

            });
        } catch (RetryableException e) {
            if (dumpOnError) {
                dumpScreen("waitForObject() for " + selector + "on "
                        + (parent == null ? "mDevice" : parent) + " failed");
            }
            throw e;
        }
    }

    public UiObject2 waitForObject(@Nullable UiObject2 parent, @NonNull BySelector selector,
            @NonNull Timeout timeout)
            throws Exception {
        return waitForObject(parent, selector, timeout, DUMP_ON_ERROR);
    }

    /**
     * Waits for and returns an object.
     *
     * @param selector {@link BySelector} that identifies the object.
     * @param timeout timeout in ms
     */
    protected UiObject2 waitForObject(@NonNull BySelector selector, @NonNull Timeout timeout)
            throws Exception {
        return waitForObject(/* parent= */ null, selector, timeout);
    }

    /**
     * Waits for and returns a child from a parent {@link UiObject2}.
     */
    public UiObject2 assertChildText(UiObject2 parent, String resourceId, String expectedText)
            throws Exception {
        final UiObject2 child = waitForObject(parent, By.res(mPackageName, resourceId),
                Timeouts.UI_TIMEOUT);
        assertWithMessage("wrong text for view '%s'", resourceId).that(child.getText())
                .isEqualTo(expectedText);
        return child;
    }

    /**
     * Execute a Runnable and wait for {@link AccessibilityEvent#TYPE_WINDOWS_CHANGED} or
     * {@link AccessibilityEvent#TYPE_WINDOW_STATE_CHANGED}.
     */
    public AccessibilityEvent waitForWindowChange(Runnable runnable, long timeoutMillis) {
        try {
            return mAutoman.executeAndWaitForEvent(runnable, (AccessibilityEvent event) -> {
                switch (event.getEventType()) {
                    case AccessibilityEvent.TYPE_WINDOWS_CHANGED:
                    case AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED:
                        return true;
                    default:
                        Log.v(TAG, "waitForWindowChange(): ignoring event " + event);
                }
                return false;
            }, timeoutMillis);
        } catch (TimeoutException e) {
            throw new WindowChangeTimeoutException(e, timeoutMillis);
        }
    }

    public AccessibilityEvent waitForWindowChange(Runnable runnable) {
        return waitForWindowChange(runnable, Timeouts.WINDOW_CHANGE_TIMEOUT_MS);
    }

    /**
     * Waits for and returns a list of objects.
     *
     * @param selector {@link BySelector} that identifies the object.
     * @param timeout timeout in ms
     */
    private List<UiObject2> waitForObjects(BySelector selector, Timeout timeout) throws Exception {
        // NOTE: mDevice.wait does not work for the save snackbar, so we need a polling approach.
        try {
            return timeout.run("waitForObject(" + selector + ")", () -> {
                final List<UiObject2> uiObjects = mDevice.findObjects(selector);
                if (uiObjects != null && !uiObjects.isEmpty()) {
                    return uiObjects;
                }
                return null;

            });

        } catch (RetryableException e) {
            dumpScreen("waitForObjects() for " + selector + "failed");
            throw e;
        }
    }

    private UiObject2 findDatasetPicker(Timeout timeout) throws Exception {
        final UiObject2 picker = waitForObject(DATASET_PICKER_SELECTOR, timeout);

        final String expectedTitle = getString(RESOURCE_STRING_DATASET_PICKER_ACCESSIBILITY_TITLE);
        assertAccessibilityTitle(picker, expectedTitle);

        if (picker != null) {
            mOkToCallAssertNoDatasets = true;
        }

        return picker;
    }

    /**
     * Asserts a given object has the expected accessibility title.
     */
    private void assertAccessibilityTitle(UiObject2 object, String expectedTitle) {
        // TODO: ideally it should get the AccessibilityWindowInfo from the object, but UiAutomator
        // does not expose that.
        for (AccessibilityWindowInfo window : mAutoman.getWindows()) {
            final CharSequence title = window.getTitle();
            if (title != null && title.toString().equals(expectedTitle)) {
                return;
            }
        }
        throw new RetryableException("Title '%s' not found for %s", expectedTitle, object);
    }

    /**
     * Sets the the screen orientation.
     *
     * @param orientation typically {@link #LANDSCAPE} or {@link #PORTRAIT}.
     *
     * @throws RetryableException if value didn't change.
     */
    public void setScreenOrientation(int orientation) throws Exception {
        mAutoman.setRotation(orientation);

        UI_SCREEN_ORIENTATION_TIMEOUT.run("setScreenOrientation(" + orientation + ")", () -> {
            return getScreenOrientation() == orientation ? Boolean.TRUE : null;
        });
    }

    /**
     * Gets the value of the screen orientation.
     *
     * @return typically {@link #LANDSCAPE} or {@link #PORTRAIT}.
     */
    public int getScreenOrientation() {
        return mDevice.getDisplayRotation();
    }

    /**
     * Dumps the current view hierarchy and take a screenshot and save both locally so they can be
     * inspected later.
     */
    public void dumpScreen(@NonNull String cause) {
        try {
            final File file = Helper.createTestFile("hierarchy.xml");
            if (file == null) return;
            Log.w(TAG, "Dumping window hierarchy because " + cause + " on " + file);
            try (FileInputStream fis = new FileInputStream(file)) {
                mDevice.dumpWindowHierarchy(file);
            }
        } catch (Exception e) {
            Log.e(TAG, "error dumping screen on " + cause, e);
        } finally {
            takeScreenshotAndSave();
        }
    }

    private Rect cropScreenshotWithoutScreenDecoration(Activity activity) {
        final WindowInsets[] inset = new WindowInsets[1];
        final View[] rootView = new View[1];

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            rootView[0] = activity.getWindow().getDecorView();
            inset[0] = rootView[0].getRootWindowInsets();
        });
        final int navBarHeight = inset[0].getStableInsetBottom();
        final int statusBarHeight = inset[0].getStableInsetTop();

        return new Rect(0, statusBarHeight, rootView[0].getWidth(),
                rootView[0].getHeight() - navBarHeight - statusBarHeight);
    }

    // TODO(b/74358143): ideally we should take a screenshot limited by the boundaries of the
    // activity window, so external elements (such as the clock) are filtered out and don't cause
    // test flakiness when the contents are compared.
    public Bitmap takeScreenshot() {
        return takeScreenshotWithRect(null);
    }

    public Bitmap takeScreenshot(@NonNull Activity activity) {
        // crop the screenshot without screen decoration to prevent test flakiness.
        final Rect rect = cropScreenshotWithoutScreenDecoration(activity);
        return takeScreenshotWithRect(rect);
    }

    private Bitmap takeScreenshotWithRect(@Nullable Rect r) {
        final long before = SystemClock.elapsedRealtime();
        final Bitmap bitmap = mAutoman.takeScreenshot();
        final long delta = SystemClock.elapsedRealtime() - before;
        Log.v(TAG, "Screenshot taken in " + delta + "ms");
        if (r == null) {
            return bitmap;
        }
        try {
            return Bitmap.createBitmap(bitmap, r.left, r.top, r.right, r.bottom);
        } finally {
            if (bitmap != null) {
                bitmap.recycle();
            }
        }
    }

    /**
     * Takes a screenshot and save it in the file system for post-mortem analysis.
     */
    public void takeScreenshotAndSave() {
        File file = null;
        try {
            file = Helper.createTestFile("screenshot.png");
            if (file != null) {
                Log.i(TAG, "Taking screenshot on " + file);
                final Bitmap screenshot = takeScreenshot();
                Helper.dumpBitmap(screenshot, file);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error taking screenshot and saving on " + file, e);
        }
    }

    /**
     * Asserts the contents of a child element.
     *
     * @param parent parent object
     * @param childId (relative) resource id of the child
     * @param assertion if {@code null}, asserts the child does not exist; otherwise, asserts the
     * child with it.
     */
    public void assertChild(@NonNull UiObject2 parent, @NonNull String childId,
            @Nullable Visitor<UiObject2> assertion) {
        final UiObject2 child = parent.findObject(By.res(mPackageName, childId));
        try {
            if (assertion != null) {
                assertWithMessage("Didn't find child with id '%s'", childId).that(child)
                        .isNotNull();
                try {
                    assertion.visit(child);
                } catch (Throwable t) {
                    throw new AssertionError("Error on child '" + childId + "'", t);
                }
            } else {
                assertWithMessage("Shouldn't find child with id '%s'", childId).that(child)
                        .isNull();
            }
        } catch (RuntimeException | Error e) {
            dumpScreen("assertChild(" + childId + ") failed: " + e);
            throw e;
        }
    }

    /**
     * Waits until the window was split to show multiple activities.
     */
    public void waitForWindowSplit() throws Exception {
        try {
            assertShownById(SPLIT_WINDOW_DIVIDER_ID);
        } catch (Exception e) {
            final long timeout = Timeouts.ACTIVITY_RESURRECTION.ms();
            Log.e(TAG, "Did not find window divider " + SPLIT_WINDOW_DIVIDER_ID + "; waiting "
                    + timeout + "ms instead");
            SystemClock.sleep(timeout);
        }
    }

    /**
     * Finds the first {@link URLSpan} on the current screen.
     */
    public URLSpan findFirstUrlSpanWithText(String str) throws Exception {
        final List<AccessibilityNodeInfo> list = mAutoman.getRootInActiveWindow()
                .findAccessibilityNodeInfosByText(str);
        if (list.isEmpty()) {
            throw new AssertionError("Didn't found AccessibilityNodeInfo with " + str);
        }

        final AccessibilityNodeInfo text = list.get(0);
        final CharSequence accessibilityTextWithSpan = text.getText();
        if (!(accessibilityTextWithSpan instanceof Spanned)) {
            throw new AssertionError("\"" + text.getViewIdResourceName() + "\" was not a Spanned");
        }

        final URLSpan[] spans = ((Spanned) accessibilityTextWithSpan)
                .getSpans(0, accessibilityTextWithSpan.length(), URLSpan.class);
        return spans[0];
    }

    public boolean scrollToTextObject(String text) {
        UiScrollable scroller = new UiScrollable(new UiSelector().scrollable(true));
        try {
            // Swipe far away from the edges to avoid triggering navigation gestures
            scroller.setSwipeDeadZonePercentage(0.25);
            return scroller.scrollTextIntoView(text);
        } catch (UiObjectNotFoundException e) {
            return false;
        }
    }
}
