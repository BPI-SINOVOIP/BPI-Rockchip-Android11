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

package com.android.car.notification;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.service.notification.StatusBarNotification;

import androidx.test.core.app.ApplicationProvider;

import com.android.car.notification.testutils.ShadowApplicationPackageManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.HashMap;
import java.util.Map;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowApplicationPackageManager.class})
public class NotificationUtilsTest {

    private Context mContext;
    private AlertEntry mAlertEntry;
    @Mock
    private StatusBarNotification mStatusBarNotification;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        when(mStatusBarNotification.getKey()).thenReturn("TEST_KEY");
        mAlertEntry = new AlertEntry(mStatusBarNotification);
    }

    @Test
    public void onIsSystemPrivilegedOrPlatformKey_isPlatformKey_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ true, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemPrivilegedOrPlatformKey(mContext, mAlertEntry))
                .isTrue();
    }

    @Test
    public void onIsSystemPrivilegedOrPlatformKey_isSystemPrivileged_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                true, /* isPrivilegedApp= */ true);

        assertThat(NotificationUtils.isSystemPrivilegedOrPlatformKey(mContext, mAlertEntry))
                .isTrue();
    }

    @Test
    public void onIsSystemPrivilegedOrPlatformKey_isSystemNotPrivileged_returnsFalse() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                true, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemPrivilegedOrPlatformKey(mContext, mAlertEntry))
                .isFalse();
    }

    @Test
    public void onIsSystemPrivilegedOrPlatformKey_isNeither_returnsFalse() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemPrivilegedOrPlatformKey(mContext, mAlertEntry))
                .isFalse();
    }

    @Test
    public void onIsSystemOrPlatformKey_isPlatformKey_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ true, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemOrPlatformKey(mContext, mAlertEntry))
                .isTrue();
    }

    @Test
    public void onIsSystemOrPlatformKey_isSystemPrivileged_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                true, /* isPrivilegedApp= */ true);

        assertThat(NotificationUtils.isSystemOrPlatformKey(mContext, mAlertEntry))
                .isTrue();
    }

    @Test
    public void onIsSystemOrPlatformKey_isSystemNotPrivileged_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                true, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemOrPlatformKey(mContext, mAlertEntry))
                .isTrue();
    }

    @Test
    public void onIsSystemOrPlatformKey_isNeither_returnsFalse() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemOrPlatformKey(mContext, mAlertEntry))
                .isFalse();
    }

    @Test
    public void onIsSystemApp_isPlatformKey_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                true, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemApp(mContext, mAlertEntry.getStatusBarNotification()))
                .isTrue();
    }

    @Test
    public void onIsSystemApp_isNotPlatformKey_returnsFalse() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSystemApp(mContext, mAlertEntry.getStatusBarNotification()))
                .isFalse();
    }

    @Test
    public void onIsSignedWithPlatformKey_isSignedWithPlatformKey_returnsTrue() {
        setApplicationInfo(/* signedWithPlatformKey= */ true, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSignedWithPlatformKey(mContext,
                mAlertEntry.getStatusBarNotification()))
                .isTrue();
    }

    @Test
    public void onIsSignedWithPlatformKey_isNotSignedWithPlatformKey_returnsFalse() {
        setApplicationInfo(/* signedWithPlatformKey= */ false, /* isSystemApp= */
                false, /* isPrivilegedApp= */ false);

        assertThat(NotificationUtils.isSignedWithPlatformKey(mContext,
                mAlertEntry.getStatusBarNotification()))
                .isFalse();
    }

    @Test
    public void onGetNotificationViewType_notificationIsARecognizedType_returnsCorrectType() {
        Map<String, CarNotificationTypeItem> typeMap = new HashMap<>();
        typeMap.put(Notification.CATEGORY_CAR_EMERGENCY, CarNotificationTypeItem.EMERGENCY);
        typeMap.put(Notification.CATEGORY_NAVIGATION, CarNotificationTypeItem.NAVIGATION);
        typeMap.put(Notification.CATEGORY_CALL, CarNotificationTypeItem.CALL);
        typeMap.put(Notification.CATEGORY_CAR_WARNING, CarNotificationTypeItem.WARNING);
        typeMap.put(Notification.CATEGORY_CAR_INFORMATION, CarNotificationTypeItem.INFORMATION);
        typeMap.put(Notification.CATEGORY_MESSAGE, CarNotificationTypeItem.MESSAGE);

        typeMap.forEach((category, typeItem) -> {
            Notification notification = new Notification();
            notification.category = category;
            when(mStatusBarNotification.getNotification()).thenReturn(notification);
            AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
            assertThat(NotificationUtils.getNotificationViewType(alertEntry)).isEqualTo(typeItem);
        });
    }

    @Test
    public void onGetNotificationViewType_notificationHasBigTextAndSummaryText_returnsInbox() {
        Bundle extras = new Bundle();
        extras.putBoolean(Notification.EXTRA_BIG_TEXT, true);
        extras.putBoolean(Notification.EXTRA_SUMMARY_TEXT, true);

        Notification notification = new Notification();
        notification.extras = extras;
        when(mStatusBarNotification.getNotification()).thenReturn(notification);
        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);

        assertThat(NotificationUtils.getNotificationViewType(alertEntry)).isEqualTo(
                CarNotificationTypeItem.INBOX);
    }

    @Test
    public void onGetNotificationViewType_unrecognizedTypeWithoutBigTextOrSummary_returnsBasic() {
        Notification notification = new Notification();
        when(mStatusBarNotification.getNotification()).thenReturn(notification);
        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);

        assertThat(NotificationUtils.getNotificationViewType(alertEntry)).isEqualTo(
                CarNotificationTypeItem.BASIC);
    }

    private void setApplicationInfo(boolean signedWithPlatformKey, boolean isSystemApp,
            boolean isPrivilegedApp) {
        ApplicationInfo applicationInfo = new ApplicationInfo();

        if (signedWithPlatformKey) {
            applicationInfo.privateFlags = applicationInfo.privateFlags
                    | ApplicationInfo.PRIVATE_FLAG_SIGNED_WITH_PLATFORM_KEY;
        }

        if (isSystemApp) {
            applicationInfo.flags = applicationInfo.flags
                    | ApplicationInfo.FLAG_SYSTEM;
        }

        if (isPrivilegedApp) {
            applicationInfo.privateFlags = applicationInfo.privateFlags
                    | ApplicationInfo.PRIVATE_FLAG_PRIVILEGED;
        }

        PackageInfo packageInfo = new PackageInfo();
        packageInfo.applicationInfo = applicationInfo;
        ShadowApplicationPackageManager.setPackageInfo(packageInfo);
    }
}
