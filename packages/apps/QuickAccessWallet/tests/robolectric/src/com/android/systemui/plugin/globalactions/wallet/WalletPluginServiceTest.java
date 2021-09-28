package com.android.systemui.plugin.globalactions.wallet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.service.quickaccesswallet.QuickAccessWalletClient;

import androidx.test.core.app.ApplicationProvider;

import com.android.systemui.plugins.GlobalActionsPanelPlugin;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class WalletPluginServiceTest {

    private final Context mContext = ApplicationProvider.getApplicationContext();
    @Mock QuickAccessWalletClient mWalletClient;
    @Mock GlobalActionsPanelPlugin.Callbacks mPluginCallbacks;
    private WalletPluginService mPluginService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(true);
        mPluginService = new WalletPluginService();
        ShadowLog.stream = System.out;
    }

    @Test
    public void onCreate_serviceAvailable_enablesFeatureInSettings() {
        mPluginService.onCreate(mContext, mContext, mWalletClient);

        ContentResolver cr = mContext.getContentResolver();
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_AVAILABLE, -1)).isEqualTo(1);
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(1);
    }

    @Test
    public void onCreate_serviceUnavailable_disablesFeatureInSettings() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(false);

        mPluginService.onCreate(mContext, mContext, mWalletClient);

        ContentResolver cr = mContext.getContentResolver();
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_AVAILABLE, -1)).isEqualTo(0);
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(1);
    }

    @Test
    public void onCreate_doesNotOverridePanelEnabledSettingIfOff() {
        ContentResolver cr = mContext.getContentResolver();
        Settings.Secure.putInt(cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, 0);

        mPluginService.onCreate(mContext, mContext, mWalletClient);

        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(0);
    }

    @Test
    public void onPanelShown_wasUnavailable_nowAvailable_updatesFeatureAvailabilityInSettings() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(false);
        mPluginService.onCreate(mContext, mContext, mWalletClient);
        ContentResolver cr = mContext.getContentResolver();
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_AVAILABLE, -1)).isEqualTo(0);
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(1);
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(true);

        mPluginService.onPanelShown(mPluginCallbacks, false, mWalletClient);

        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_AVAILABLE, -1)).isEqualTo(1);
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(1);
    }

    @Test
    public void onPanelShown_wasAvailable_nowUnavailable_updatesFeatureAvailabilityInSettings() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(true);
        mPluginService.onCreate(mContext, mContext, mWalletClient);
        ContentResolver cr = mContext.getContentResolver();
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_AVAILABLE, -1)).isEqualTo(1);
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(1);
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(false);

        mPluginService.onPanelShown(mPluginCallbacks, false, mWalletClient);

        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_AVAILABLE, -1)).isEqualTo(0);
        assertThat(Settings.Secure.getInt(
                cr, Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, -1)).isEqualTo(1);
    }

    @Test
    public void onPanelShown_returnsControllerIfFeatureAvailable() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(true);
        when(mWalletClient.isWalletFeatureAvailable()).thenReturn(true);
        mPluginService.onCreate(mContext, mContext, mWalletClient);

        GlobalActionsPanelPlugin.PanelViewController viewController =
                mPluginService.onPanelShown(mPluginCallbacks, false, mWalletClient);

        assertThat(viewController).isNotNull();
    }

    @Test
    public void onPanelShown_performsGetWalletCardsRequest() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(true);
        when(mWalletClient.isWalletFeatureAvailable()).thenReturn(true);
        mPluginService.onCreate(mContext, mContext, mWalletClient);

        mPluginService.onPanelShown(mPluginCallbacks, false, mWalletClient);

        verify(mWalletClient).getWalletCards(any(), any(), any());
    }

    @Test
    public void onPanelShown_returnsNullIfFeatureUnavailable() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(true);
        when(mWalletClient.isWalletFeatureAvailable()).thenReturn(false);
        mPluginService.onCreate(mContext, mContext, mWalletClient);

        GlobalActionsPanelPlugin.PanelViewController viewController =
                mPluginService.onPanelShown(mPluginCallbacks, false, mWalletClient);

        assertThat(viewController).isNull();
    }

    @Test
    public void onPanelShown_returnsNullIfServiceUnavailable() {
        when(mWalletClient.isWalletServiceAvailable()).thenReturn(false);
        when(mWalletClient.isWalletFeatureAvailable()).thenReturn(true);
        mPluginService.onCreate(mContext, mContext, mWalletClient);

        GlobalActionsPanelPlugin.PanelViewController viewController =
                mPluginService.onPanelShown(mPluginCallbacks, false, mWalletClient);

        assertThat(viewController).isNull();
    }
}