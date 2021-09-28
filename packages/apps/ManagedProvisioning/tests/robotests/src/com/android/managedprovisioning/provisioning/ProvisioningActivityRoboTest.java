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
 * limitations under the License
 */

package com.android.managedprovisioning.provisioning;

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;

import static com.android.managedprovisioning.common.LogoUtils.saveOrganisationLogo;
import static com.android.managedprovisioning.provisioning.AbstractProvisioningActivity.CANCEL_PROVISIONING_DIALOG_OK;
import static com.android.managedprovisioning.provisioning.AbstractProvisioningActivity.ERROR_DIALOG_OK;
import static com.android.managedprovisioning.provisioning.AbstractProvisioningActivity.ERROR_DIALOG_RESET;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.Application;
import android.app.DialogFragment;
import android.app.Fragment;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;
import android.net.Uri;
import android.widget.ImageView;

import com.android.managedprovisioning.R;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.List;

/**
 * Robolectric tests for {@link ProvisioningActivity}.
 */
@RunWith(RobolectricTestRunner.class)
public class ProvisioningActivityRoboTest {

    private static final String ADMIN_PACKAGE = "com.test.admin";
    private static final ComponentName ADMIN = new ComponentName(ADMIN_PACKAGE, ".Receiver");
    private static final ProvisioningParams DEVICE_OWNER_PARAMS = new ProvisioningParams.Builder()
            .setProvisioningAction(ACTION_PROVISION_MANAGED_DEVICE)
            .setDeviceAdminComponentName(ADMIN)
            .build();
    private static final ProvisioningParams PROFILE_OWNER_PARAMS = new ProvisioningParams.Builder()
            .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
            .setDeviceAdminComponentName(ADMIN)
            .build();
    private static final Intent PROFILE_OWNER_INTENT = new Intent()
            .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, PROFILE_OWNER_PARAMS);
    private static final Intent DEVICE_OWNER_INTENT = new Intent()
            .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, DEVICE_OWNER_PARAMS);
    private static final int ERROR_MESSAGE_ID = R.string.managed_provisioning_error_text;
    private static final int DEFAULT_MAIN_COLOR = -15043608;
    private static final int CUSTOM_COLOR = Color.parseColor("#d40000");
    private static final Uri LOGO_URI = Uri.parse("http://logo");

    private ProvisioningManager mMockProvisioningManager = Mockito.mock(ProvisioningManager.class);
    private Application mContext = RuntimeEnvironment.application;

    @Test
    public void error_noFactoryReset_showsDialogue() {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT).get();

        activity.error(R.string.cant_set_up_device, ERROR_MESSAGE_ID, /* resetRequired= */ false);

        final Fragment dialog = activity.getFragmentManager().findFragmentByTag(ERROR_DIALOG_OK);
        assertThat(dialog).isNotNull();
    }

    @Test
    public void error_noFactoryReset_doesNotReset() throws Exception {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT).get();
        activity.error(R.string.cant_set_up_device, ERROR_MESSAGE_ID, /* resetRequired= */ false);

        final Fragment dialog = activity.getFragmentManager().findFragmentByTag(ERROR_DIALOG_OK);
        clickOnPositiveButton(activity, (DialogFragment) dialog);

        final List<Intent> intents = shadowOf(mContext).getBroadcastIntents();
        assertThat(intentsContainsAction(intents, Intent.ACTION_FACTORY_RESET)).isFalse();
    }

    @Test
    public void error_factoryReset_showsDialogue() {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT).get();

        activity.error(R.string.cant_set_up_device, ERROR_MESSAGE_ID, /* resetRequired= */ true);

        final Fragment dialog = activity.getFragmentManager().findFragmentByTag(ERROR_DIALOG_RESET);
        assertThat(dialog).isNotNull();
    }

    @Test
    public void error_factoryReset_resets() throws Exception {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT).get();
        activity.error(R.string.cant_set_up_device, ERROR_MESSAGE_ID, /* resetRequired= */ true);

        final Fragment dialog = activity.getFragmentManager().findFragmentByTag(ERROR_DIALOG_RESET);
        clickOnPositiveButton(activity, (DialogFragment) dialog);

        final List<Intent> intents = shadowOf(mContext).getBroadcastIntents();
        assertThat(intentsContainsAction(intents, Intent.ACTION_FACTORY_RESET)).isTrue();
    }

    @Test
    public void profileOwnerIntent_usesDefaultLogo() throws Throwable {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();

        assertUsesDefaultLogo(activity);
    }

    @Test
    public void profileOwnerIntent_setCustomLogo_usesCustomLogo() throws Throwable {
        setupCustomLogo(mContext, LOGO_URI);

        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();

        assertUsesCustomLogo(activity);
    }

    @Test
    public void deviceOwnerIntent_usesDefaultLogo() throws Throwable {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, DEVICE_OWNER_INTENT)
                        .setup().get();

        assertUsesDefaultLogo(activity);
    }

    @Test
    public void deviceOwnerIntent_setCustomLogo_usesCustomLogo() throws Throwable {
        setupCustomLogo(mContext, LOGO_URI);

        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, DEVICE_OWNER_INTENT)
                        .setup().get();

        assertUsesCustomLogo(activity);
    }

    @Test
    public void managedProfileIntent_defaultColor_colorCorrect() {
        assertColorsCorrect(
                PROFILE_OWNER_INTENT,
                DEFAULT_MAIN_COLOR,
                /* statusBarColor= */ Color.TRANSPARENT);
    }

    @Test
    public void deviceOwnerIntent_defaultColor_colorCorrect() {
        assertColorsCorrect(
                DEVICE_OWNER_INTENT,
                DEFAULT_MAIN_COLOR,
                /* statusBarColor= */ Color.TRANSPARENT);
    }

    @Test
    public void managedProfileIntent_customColor_colorCorrect() {
        assertColorsCorrect(
                createProvisioningIntent(ACTION_PROVISION_MANAGED_PROFILE, CUSTOM_COLOR),
                /* mainColor= */ CUSTOM_COLOR,
                /* statusBarColor= */ CUSTOM_COLOR);
    }

    @Test
    public void deviceOwnerIntent_customColor_colorCorrect() {
        assertColorsCorrect(
                createProvisioningIntent(ACTION_PROVISION_MANAGED_DEVICE, CUSTOM_COLOR),
                /* mainColor= */ CUSTOM_COLOR,
                /* statusBarColor= */ CUSTOM_COLOR);
    }

    @Test
    public void activity_profileOwner_backPressed_showsCancelDialog() throws Throwable {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();

        activity.onBackPressed();

        final Fragment dialog =
                activity.getFragmentManager().findFragmentByTag(CANCEL_PROVISIONING_DIALOG_OK);
        assertThat(dialog).isNotNull();
    }

    @Test
    public void activity_profileOwner_backPressed_doNotCancel_doesNotFinishActivity() {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();

        activity.onBackPressed();
        final Fragment dialog =
                activity.getFragmentManager().findFragmentByTag(CANCEL_PROVISIONING_DIALOG_OK);
        clickOnNegativeButton(activity, (DialogFragment) dialog);

        assertThat(activity.isFinishing()).isFalse();
    }

    @Test
    public void activity_profileOwner_backPressed_doNotCancel_doesNotCancelProvisioning() {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();
        activity.mProvisioningManager = mMockProvisioningManager;

        activity.onBackPressed();
        final Fragment dialog =
                activity.getFragmentManager().findFragmentByTag(CANCEL_PROVISIONING_DIALOG_OK);
        clickOnNegativeButton(activity, (DialogFragment) dialog);

        verify(mMockProvisioningManager, never()).cancelProvisioning();
    }

    @Test
    public void activity_profileOwner_backPressed_cancel_doesFinishActivity() {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();

        activity.onBackPressed();
        final Fragment dialog =
                activity.getFragmentManager().findFragmentByTag(CANCEL_PROVISIONING_DIALOG_OK);
        clickOnPositiveButton(activity, (DialogFragment) dialog);

        assertThat(activity.isFinishing()).isTrue();
    }

    @Test
    public void activity_profileOwner_backPressed_cancel_doesCancelProvisioning() {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, PROFILE_OWNER_INTENT)
                        .setup().get();
        activity.mProvisioningManager = mMockProvisioningManager;

        activity.onBackPressed();
        final Fragment dialog =
                activity.getFragmentManager().findFragmentByTag(CANCEL_PROVISIONING_DIALOG_OK);
        clickOnPositiveButton(activity, (DialogFragment) dialog);

        verify(mMockProvisioningManager).cancelProvisioning();
    }

    private Intent createProvisioningIntent(String action, int mainColor) {
        final ProvisioningParams provisioningParams = new ProvisioningParams.Builder()
                .setProvisioningAction(action)
                .setDeviceAdminComponentName(ADMIN)
                .setMainColor(mainColor)
                .build();

        final Intent intent = new Intent();
        intent.putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, provisioningParams);
        return intent;
    }

    private void assertColorsCorrect(Intent intent, int mainColor, int statusBarColor) {
        final ProvisioningActivity activity =
                Robolectric.buildActivity(ProvisioningActivity.class, intent)
                        .setup().get();

        assertDefaultLogoColorCorrect(activity, mainColor);
        assertStatusBarColorCorrect(activity, statusBarColor);
    }

    private void assertDefaultLogoColorCorrect(Activity activity, int targetColor) {
        Drawable actualLogo =
                ((ImageView) activity.findViewById(R.id.sud_layout_icon)).getDrawable();
        PorterDuffColorFilter colorFilter = (PorterDuffColorFilter) actualLogo.getColorFilter();

        assertThat(colorFilter.getColor()).isEqualTo(targetColor);
    }

    private void assertStatusBarColorCorrect(Activity activity, int targetColor) {
        final int statusBarColor = activity.getWindow().getStatusBarColor();
        assertThat(statusBarColor).isEqualTo(targetColor);
    }

    private static boolean intentsContainsAction(List<Intent> intents, String action) {
        return intents.stream().anyMatch(intent -> intent.getAction().equals(action));
    }

    private void clickOnOkButton(ProvisioningActivity activity, DialogFragment dialog) {
        // TODO(135181317): This should be replaced by
        //  activity.findViewById(android.R.id.button1).performClick();
        activity.onPositiveButtonClick(dialog);
    }

    private static void setupCustomLogo(Context context, Uri logoUri) {
        Bitmap bitmap = Bitmap.createBitmap(300, 300, Bitmap.Config.ARGB_8888);
        InputStream inputStream = bitmapToInputStream(bitmap);
        shadowOf(context.getContentResolver()).registerInputStream(logoUri, inputStream);
        saveOrganisationLogo(context, logoUri);
    }

    private static InputStream bitmapToInputStream(Bitmap bitmap) {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 0 /* ignored for PNG */, bos);
        byte[] bitmapdata = bos.toByteArray();
        return new ByteArrayInputStream(bitmapdata);
    }

    private static void assertUsesDefaultLogo(Activity activity) {
        final ImageView imageView = activity.findViewById(R.id.sud_layout_icon);
        // We default to a vector logo
        assertThat(imageView.getDrawable()).isInstanceOf(VectorDrawable.class);
    }

    private static void assertUsesCustomLogo(Activity activity) {
        final ImageView imageView = activity.findViewById(R.id.sud_layout_icon);
        // The custom logo we have set is a bitmap
        assertThat(imageView.getDrawable()).isInstanceOf(BitmapDrawable.class);
    }

    private void clickOnPositiveButton(ProvisioningActivity activity, DialogFragment dialog) {
        // TODO(135181317): This should be replaced by
        //  activity.findViewById(android.R.id.button1).performClick();

        activity.onPositiveButtonClick(dialog);
    }

    private void clickOnNegativeButton(ProvisioningActivity activity, DialogFragment dialog) {
        // TODO(135181317): This should be replaced by
        //  activity.findViewById(android.R.id.button2).performClick();
        activity.onNegativeButtonClick(dialog);
    }
}
