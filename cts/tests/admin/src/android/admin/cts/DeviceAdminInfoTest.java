/*
 * Copyright (C) 2011 The Android Open Source Project
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

package android.admin.cts;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DeviceAdminInfo;
import android.content.ComponentName;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.test.AndroidTestCase;
import android.util.Log;

public class DeviceAdminInfoTest extends AndroidTestCase {

    private static final String TAG = DeviceAdminInfoTest.class.getSimpleName();

    private PackageManager mPackageManager;
    private ComponentName mComponent;
    private ComponentName mSecondComponent;
    private ComponentName mThirdComponent;
    private boolean mDeviceAdmin;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mPackageManager = mContext.getPackageManager();
        mComponent = getReceiverComponent();
        mSecondComponent = getSecondReceiverComponent();
        mThirdComponent = getThirdReceiverComponent();
        mDeviceAdmin =
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN);
    }

    static ComponentName getReceiverComponent() {
        return new ComponentName("android.admin.app", "android.admin.app.CtsDeviceAdminReceiver");
    }

    static ComponentName getSecondReceiverComponent() {
        return new ComponentName("android.admin.app", "android.admin.app.CtsDeviceAdminReceiver2");
    }

    static ComponentName getThirdReceiverComponent() {
        return new ComponentName("android.admin.app", "android.admin.app.CtsDeviceAdminReceiver3");
    }

    static ComponentName getProfileOwnerComponent() {
        return new ComponentName("android.admin.app", "android.admin.app.CtsDeviceAdminProfileOwner");
    }

    static ComponentName getVisibleComponent() {
        return new ComponentName(
                "android.admin.app", "android.admin.app.CtsDeviceAdminReceiverVisible");
    }

    static ComponentName getInvisibleComponent() {
        return new ComponentName(
                "android.admin.app", "android.admin.app.CtsDeviceAdminReceiverInvisible");
    }

    public void testDeviceAdminInfo() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testDeviceAdminInfo");
            return;
        }
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = mPackageManager.getReceiverInfo(mComponent,
                PackageManager.GET_META_DATA);

        DeviceAdminInfo info = new DeviceAdminInfo(mContext, resolveInfo);
        assertThat(mComponent).isEqualTo(info.getComponent());
        assertThat(mComponent.getPackageName()).isEqualTo(info.getPackageName());
        assertThat(mComponent.getClassName()).isEqualTo(info.getReceiverName());

        assertThat(info.supportsTransferOwnership()).isFalse();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_FORCE_LOCK)).isTrue();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_LIMIT_PASSWORD)).isTrue();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_RESET_PASSWORD)).isTrue();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_WATCH_LOGIN)).isTrue();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_WIPE_DATA)).isTrue();

        assertThat("force-lock")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_FORCE_LOCK));
        assertThat("limit-password")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_LIMIT_PASSWORD));
        assertThat("reset-password")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_RESET_PASSWORD));
        assertThat("watch-login")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_WATCH_LOGIN));
        assertThat("wipe-data")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_WIPE_DATA));
    }

    public void testDeviceAdminInfo2() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testDeviceAdminInfo2");
            return;
        }
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = mPackageManager.getReceiverInfo(mSecondComponent,
                PackageManager.GET_META_DATA);

        DeviceAdminInfo info = new DeviceAdminInfo(mContext, resolveInfo);
        assertThat(mSecondComponent).isEqualTo(info.getComponent());
        assertThat(mSecondComponent.getPackageName()).isEqualTo(info.getPackageName());
        assertThat(mSecondComponent.getClassName()).isEqualTo(info.getReceiverName());

        assertThat(info.supportsTransferOwnership()).isFalse();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_FORCE_LOCK)).isFalse();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_LIMIT_PASSWORD)).isTrue();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_RESET_PASSWORD)).isTrue();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_WATCH_LOGIN)).isFalse();
        assertThat(info.usesPolicy(DeviceAdminInfo.USES_POLICY_WIPE_DATA)).isTrue();

        assertThat("force-lock")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_FORCE_LOCK));
        assertThat("limit-password")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_LIMIT_PASSWORD));
        assertThat("reset-password")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_RESET_PASSWORD));
        assertThat("watch-login")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_WATCH_LOGIN));
        assertThat("wipe-data")
                .isEqualTo(info.getTagForPolicy(DeviceAdminInfo.USES_POLICY_WIPE_DATA));
    }

    public void testDeviceAdminInfo3() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testDeviceAdminInfo3");
            return;
        }
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = mPackageManager.getReceiverInfo(mThirdComponent,
                PackageManager.GET_META_DATA);

        DeviceAdminInfo info = new DeviceAdminInfo(mContext, resolveInfo);
        assertThat(info.supportsTransferOwnership()).isTrue();
    }

    public void testDescribeContents_returnsAtLeastZero() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testDescribeContents_returnsAtLeastZero");
            return;
        }

        assertThat(buildDeviceAdminInfo(buildActivityInfo()).describeContents()).isAtLeast(0);
    }

    public void testGetActivityInfo_returnsActivityInfo() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testGetActivityInfo_returnsActivityInfo");
            return;
        }
        ActivityInfo activityInfo = buildActivityInfo();
        DeviceAdminInfo deviceAdminInfo = buildDeviceAdminInfo(activityInfo);

        assertThat(deviceAdminInfo.getActivityInfo()).isEqualTo(activityInfo);
    }

    public void testIsVisible_visibleComponent_returnsTrue() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testIsVisible_visibleComponent_returnsTrue");
            return;
        }
        ActivityInfo activityInfo = buildActivityInfo(getVisibleComponent());
        DeviceAdminInfo deviceAdminInfo = buildDeviceAdminInfo(activityInfo);

        assertThat(deviceAdminInfo.isVisible()).isTrue();
    }

    public void testIsVisible_invisibleComponent_returnsFalse() throws Exception {
        if (!mDeviceAdmin) {
            Log.w(TAG, "Skipping testIsVisible_invisibleComponent_returnsFalse");
            return;
        }
        ActivityInfo activityInfo = buildActivityInfo(getInvisibleComponent());
        DeviceAdminInfo deviceAdminInfo = buildDeviceAdminInfo(activityInfo);

        assertThat(deviceAdminInfo.isVisible()).isFalse();
    }

    private DeviceAdminInfo buildDeviceAdminInfo(ActivityInfo activityInfo) throws Exception {
        return new DeviceAdminInfo(mContext, buildResolveInfo(activityInfo));
    }

    private ResolveInfo buildResolveInfo(ActivityInfo activityInfo) {
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private ActivityInfo buildActivityInfo() throws Exception {
        return buildActivityInfo(mThirdComponent);
    }

    private ActivityInfo buildActivityInfo(ComponentName componentName) throws Exception {
        return mPackageManager.getReceiverInfo(componentName, PackageManager.GET_META_DATA);
    }
}
