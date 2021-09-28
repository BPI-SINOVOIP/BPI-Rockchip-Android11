package com.android.cts.profileowner;

public class BackupServicePoliciesTest extends BaseProfileOwnerTest {
  /**
   * Test: Test enabling and disabling backup service. This test should be executed after installing
   * a profile owner so that we check that backup service is not enabled by default.
   */
  public void testEnablingAndDisablingBackupService() {
    assertFalse(mDevicePolicyManager.isBackupServiceEnabled(getWho()));
    mDevicePolicyManager.setBackupServiceEnabled(getWho(), true);
    assertTrue(mDevicePolicyManager.isBackupServiceEnabled(getWho()));
    mDevicePolicyManager.setBackupServiceEnabled(getWho(), false);
    assertFalse(mDevicePolicyManager.isBackupServiceEnabled(getWho()));
  }
}
