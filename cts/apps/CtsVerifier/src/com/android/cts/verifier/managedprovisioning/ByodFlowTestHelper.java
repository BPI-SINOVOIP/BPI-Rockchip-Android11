package com.android.cts.verifier.managedprovisioning;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

public class ByodFlowTestHelper {
    private Context mContext;
    private PackageManager mPackageManager;

    public ByodFlowTestHelper(Context context) {
        this.mContext = context;
        this.mPackageManager = mContext.getPackageManager();
    }

    public void setup() {
        setComponentsEnabledState(PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
    }

    /** Reports result to ByodFlowTestActivity if it is impossible via normal setResult. */
    public void sendResultToPrimary(Intent result) {
        final Intent intent = new Intent(ByodFlowTestActivity.ACTION_TEST_RESULT);
        intent.putExtra(ByodFlowTestActivity.EXTRA_RESULT, result);
        startActivityInPrimary(intent);
    }

    public void startActivityInPrimary(Intent intent) {
        // Disable app components in the current profile, so only the counterpart in the other
        // profile can respond (via cross-profile intent filter)
        mContext.getPackageManager().setComponentEnabledSetting(
                new ComponentName(mContext, ByodFlowTestActivity.class),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP);
        mContext.startActivity(intent);
    }

    /**
     * Clean up things. This has to be working even it is called multiple times.
     */
    public void tearDown() {
        Utils.requestDeleteManagedProfile(mContext);
        setComponentsEnabledState(PackageManager.COMPONENT_ENABLED_STATE_DEFAULT);
    }

    /**
     * Disable or enable app components in the current profile. When they are disabled only the
     * counterpart in the other profile can respond (via cross-profile intent filter).
     *
     * @param enabledState {@link PackageManager#COMPONENT_ENABLED_STATE_DISABLED} or
     *                     {@link PackageManager#COMPONENT_ENABLED_STATE_DEFAULT}
     */
    private void setComponentsEnabledState(final int enabledState) {
        final String[] components = {
                ByodHelperActivity.class.getName(),
                WorkStatusTestActivity.class.getName(),
                PermissionLockdownTestActivity.ACTIVITY_ALIAS,
                AuthenticationBoundKeyTestActivity.class.getName(),
                VpnTestActivity.class.getName(),
                AlwaysOnVpnSettingsTestActivity.class.getName(),
                CrossProfilePermissionControlActivity.class.getName(),
                IntermediateRecentActivity.class.getName(),
                CommandReceiverActivity.class.getName(),
                SetSupportMessageActivity.class.getName(),
                KeyChainTestActivity.class.getName(),
                WorkProfileWidgetActivity.class.getName()
        };
        for (String component : components) {
            mPackageManager.setComponentEnabledSetting(new ComponentName(mContext, component),
                    enabledState, PackageManager.DONT_KILL_APP);
        }
    }
}
