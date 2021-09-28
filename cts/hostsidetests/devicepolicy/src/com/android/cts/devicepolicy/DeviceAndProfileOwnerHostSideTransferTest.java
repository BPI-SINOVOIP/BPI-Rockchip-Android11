package com.android.cts.devicepolicy;

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;

import static org.junit.Assert.fail;

import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Test;

import static com.google.common.truth.Truth.assertThat;

public abstract class DeviceAndProfileOwnerHostSideTransferTest extends BaseDevicePolicyTest {

    protected static final String TRANSFER_OWNER_OUTGOING_PKG =
            "com.android.cts.transferowneroutgoing";
    protected static final String TRANSFER_OWNER_OUTGOING_APK = "CtsTransferOwnerOutgoingApp.apk";
    protected static final String TRANSFER_OWNER_OUTGOING_TEST_RECEIVER =
            TRANSFER_OWNER_OUTGOING_PKG
                    + "/com.android.cts.transferowner"
                    + ".DeviceAndProfileOwnerTransferOutgoingTest$BasicAdminReceiver";
    protected static final String TRANSFER_OWNER_INCOMING_PKG =
            "com.android.cts.transferownerincoming";
    protected static final String TRANSFER_OWNER_INCOMING_APK = "CtsTransferOwnerIncomingApp.apk";
    protected static final String INVALID_TARGET_APK = "CtsIntentReceiverApp.apk";
    protected static final String TRANSFER_PROFILE_OWNER_OUTGOING_TEST =
        "com.android.cts.transferowner.TransferProfileOwnerOutgoingTest";
    protected static final String TRANSFER_PROFILE_OWNER_INCOMING_TEST =
        "com.android.cts.transferowner.TransferProfileOwnerIncomingTest";
    private final String INCOMING_ADMIN_SERVICE_FULL_NAME =
            "com.android.cts.transferowner"
                    + ".DeviceAndProfileOwnerTransferIncomingTest$BasicAdminService";


    protected int mUserId;
    protected String mOutgoingTestClassName;
    protected String mIncomingTestClassName;

    @Test
    public void testTransferOwnership() throws Exception {
        if (!mHasFeature) {
            return;
        }

        final boolean hasManagedProfile = (mUserId != mPrimaryUserId);
        final String expectedManagementType = hasManagedProfile ? "profile-owner" : "device-owner";
        assertMetricsLogged(getDevice(), () -> {
            runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG, mOutgoingTestClassName,
                    "testTransferOwnership", mUserId);
        }, new DevicePolicyEventWrapper.Builder(EventId.TRANSFER_OWNERSHIP_VALUE)
                .setAdminPackageName(TRANSFER_OWNER_OUTGOING_PKG)
                .setStrings(TRANSFER_OWNER_INCOMING_PKG, expectedManagementType)
                .build());
    }

    @Test
    public void testTransferSameAdmin() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferSameAdmin", mUserId);
    }

    @Test
    public void testTransferInvalidTarget() throws Exception {
        if (!mHasFeature) {
            return;
        }
        installAppAsUser(INVALID_TARGET_APK, mUserId);
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferInvalidTarget", mUserId);
    }

    @Test
    public void testTransferPolicies() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferWithPoliciesOutgoing", mUserId);
        runDeviceTestsAsUser(TRANSFER_OWNER_INCOMING_PKG,
                mIncomingTestClassName,
                "testTransferPoliciesAreRetainedAfterTransfer", mUserId);
    }

    @Test
    public void testTransferOwnershipChangedBroadcast() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferOwnershipChangedBroadcast", mUserId);
    }

    @Test
    public void testTransferCompleteCallback() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferOwnership", mUserId);

        waitForBroadcastIdle();

        runDeviceTestsAsUser(TRANSFER_OWNER_INCOMING_PKG,
                mIncomingTestClassName,
                "testTransferCompleteCallbackIsCalled", mUserId);
    }

    protected void setupTestParameters(int userId, String outgoingTestClassName,
            String incomingTestClassName) {
        mUserId = userId;
        mOutgoingTestClassName = outgoingTestClassName;
        mIncomingTestClassName = incomingTestClassName;
    }

    @Test
    public void testTransferOwnershipNoMetadata() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferOwnershipNoMetadata", mUserId);
    }

    @Test
    public void testIsTransferBundlePersisted() throws DeviceNotAvailableException {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferOwnershipBundleSaved", mUserId);
        runDeviceTestsAsUser(TRANSFER_OWNER_INCOMING_PKG,
                mIncomingTestClassName,
                "testTransferOwnershipBundleLoaded", mUserId);
    }

    @Test
    public void testGetTransferOwnershipBundleOnlyCalledFromAdmin()
            throws DeviceNotAvailableException {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testGetTransferOwnershipBundleOnlyCalledFromAdmin", mUserId);
    }

    @Test
    public void testBundleEmptyAfterTransferWithNullBundle() throws DeviceNotAvailableException {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testTransferOwnershipNullBundle", mUserId);
        runDeviceTestsAsUser(TRANSFER_OWNER_INCOMING_PKG,
                mIncomingTestClassName,
                "testTransferOwnershipEmptyBundleLoaded", mUserId);
    }

    @Test
    public void testIsBundleNullNoTransfer() throws DeviceNotAvailableException {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
                mOutgoingTestClassName,
                "testIsBundleNullNoTransfer", mUserId);
    }

    protected int setupManagedProfileOnDeviceOwner(String apkName, String adminReceiverClassName)
            throws Exception {
        return setupManagedProfile(apkName, adminReceiverClassName);
    }

    protected int setupManagedProfile(String apkName, String adminReceiverClassName)
            throws Exception {
        final int userId = createManagedProfile(mPrimaryUserId);
        installAppAsUser(apkName, userId);
        if (!setProfileOwner(adminReceiverClassName, userId, false)) {
            removeAdmin(TRANSFER_OWNER_OUTGOING_TEST_RECEIVER, userId);
            getDevice().uninstallPackage(TRANSFER_OWNER_OUTGOING_PKG);
            fail("Failed to set device owner");
            return -1;
        }
        startUserAndWait(userId);
        return userId;
    }

    @Test
    public void testTargetDeviceAdminServiceBound() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
            mOutgoingTestClassName,
            "testTransferOwnership", mUserId);
        assertServiceRunning(INCOMING_ADMIN_SERVICE_FULL_NAME);
    }

    private void assertServiceRunning(String serviceName) throws DeviceNotAvailableException {
        final String result = getDevice().executeShellCommand(
                String.format("dumpsys activity services %s", serviceName));
        assertThat(result).contains("app=ProcessRecord");
    }

    protected void setSameAffiliationId(int profileUserId, String testClassName)
        throws Exception {
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
            testClassName,
            "testSetAffiliationId1", mPrimaryUserId);
        runDeviceTestsAsUser(TRANSFER_OWNER_OUTGOING_PKG,
            testClassName,
            "testSetAffiliationId1", profileUserId);
    }

    protected void assertAffiliationIdsAreIntact(int profileUserId,
        String testClassName) throws Exception {
        runDeviceTestsAsUser(TRANSFER_OWNER_INCOMING_PKG,
            testClassName,
            "testIsAffiliationId1", mPrimaryUserId);
        runDeviceTestsAsUser(TRANSFER_OWNER_INCOMING_PKG,
            testClassName,
            "testIsAffiliationId1", profileUserId);
    }
}
