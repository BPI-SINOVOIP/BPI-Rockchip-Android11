package com.android.car.messenger;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import android.app.AppOpsManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothMapClient;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowBluetoothAdapter;

import java.util.Arrays;
import java.util.HashSet;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowBluetoothAdapter.class})
public class MessengerDelegateTest {

    private static final String BLUETOOTH_ADDRESS_ONE = "FA:F8:14:CA:32:39";
    private static final String BLUETOOTH_ADDRESS_TWO = "FA:F8:33:44:32:39";

    @Mock
    private BluetoothDevice mMockBluetoothDeviceOne;
    @Mock
    private BluetoothDevice mMockBluetoothDeviceTwo;
    @Mock
    AppOpsManager mMockAppOpsManager;

    private Context mContext = RuntimeEnvironment.application;
    private MessageNotificationDelegate mMessengerDelegate;
    private ShadowBluetoothAdapter mShadowBluetoothAdapter;
    private Intent mMessageOneIntent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Add AppOps permissions required to write to Telephony.SMS database.
        when(mMockAppOpsManager.checkOpNoThrow(anyInt(), anyInt(), anyString())).thenReturn(
                AppOpsManager.MODE_DEFAULT);
        Shadows.shadowOf(RuntimeEnvironment.application)
                .setSystemService(Context.APP_OPS_SERVICE, mMockAppOpsManager);

        when(mMockBluetoothDeviceOne.getAddress()).thenReturn(BLUETOOTH_ADDRESS_ONE);
        when(mMockBluetoothDeviceTwo.getAddress()).thenReturn(BLUETOOTH_ADDRESS_TWO);
        mShadowBluetoothAdapter = Shadow.extract(BluetoothAdapter.getDefaultAdapter());

        createMockMessages();
        mMessengerDelegate = new MessageNotificationDelegate(mContext);
        mMessengerDelegate.onDeviceConnected(mMockBluetoothDeviceOne);
    }

    @Test
    public void testDeviceConnections() {
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).containsKey(
                BLUETOOTH_ADDRESS_ONE);
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).hasSize(1);

        mMessengerDelegate.onDeviceConnected(mMockBluetoothDeviceTwo);
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).containsKey(
                BLUETOOTH_ADDRESS_TWO);
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).hasSize(2);

        mMessengerDelegate.onDeviceConnected(mMockBluetoothDeviceOne);
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).hasSize(2);
    }

    @Test
    public void testDeviceConnection_hasCorrectTimestamp() {
        long timestamp = System.currentTimeMillis();
        mMessengerDelegate.onDeviceConnected(mMockBluetoothDeviceTwo);

        long deviceConnectionTimestamp =
                mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp.get(BLUETOOTH_ADDRESS_TWO);

        // Sometimes there is slight flakiness in the timestamps.
        assertThat(deviceConnectionTimestamp-timestamp).isLessThan(5L);
    }

    @Test
    public void testOnDeviceDisconnected_notConnectedDevice() {
        mMessengerDelegate.onDeviceDisconnected(mMockBluetoothDeviceTwo);

        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).containsKey(
                BLUETOOTH_ADDRESS_ONE);
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).hasSize(1);
    }

    @Test
    public void testOnDeviceDisconnected_connectedDevice() {
        mMessengerDelegate.onDeviceConnected(mMockBluetoothDeviceTwo);

        mMessengerDelegate.onDeviceDisconnected(mMockBluetoothDeviceOne);

        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).containsKey(
                BLUETOOTH_ADDRESS_TWO);
        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).hasSize(1);
    }

    @Test
    public void testOnDeviceDisconnected_connectedDevice_withMessages() {
        // Disconnect a connected device, and ensure its messages are removed.
        mMessengerDelegate.onMessageReceived(mMessageOneIntent);
        mMessengerDelegate.onDeviceDisconnected(mMockBluetoothDeviceOne);

        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).isEmpty();
        assertThat(mMessengerDelegate.mGeneratedGroupConversationTitles).isEmpty();
        assertThat(mMessengerDelegate.mSenderToLargeIconBitmap).isEmpty();
        assertThat(mMessengerDelegate.mUriToSenderNameMap).isEmpty();
    }

    @Test
    public void testOnDeviceDisconnected_notConnectedDevice_withMessagesFromConnectedDevice() {
        // Disconnect a not connected device, and ensure device one's messages are still saved.
        mMessengerDelegate.onMessageReceived(mMessageOneIntent);
        mMessengerDelegate.onDeviceDisconnected(mMockBluetoothDeviceTwo);

        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).hasSize(1);
        assertThat(mMessengerDelegate.mUriToSenderNameMap).hasSize(1);
    }

    @Test
    public void testConnectedDevices_areNotAddedFromBTAdapterBondedDevices() {
        mShadowBluetoothAdapter.setBondedDevices(
                new HashSet<>(Arrays.asList(mMockBluetoothDeviceTwo)));
        mMessengerDelegate = new MessageNotificationDelegate(mContext);

        assertThat(mMessengerDelegate.mBtDeviceAddressToConnectionTimestamp).isEmpty();
    }

    private Intent createMessageIntent(BluetoothDevice device, String handle, String senderUri,
            String senderName, String messageText, Long timestamp, boolean isReadOnPhone) {
        Intent intent = new Intent();
        intent.setAction(BluetoothMapClient.ACTION_MESSAGE_RECEIVED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothMapClient.EXTRA_MESSAGE_HANDLE, handle);
        intent.putExtra(BluetoothMapClient.EXTRA_SENDER_CONTACT_URI, senderUri);
        intent.putExtra(BluetoothMapClient.EXTRA_SENDER_CONTACT_NAME, senderName);
        intent.putExtra(BluetoothMapClient.EXTRA_MESSAGE_READ_STATUS, isReadOnPhone);
        intent.putExtra(android.content.Intent.EXTRA_TEXT, messageText);
        if (timestamp != null) {
            intent.putExtra(BluetoothMapClient.EXTRA_MESSAGE_TIMESTAMP, timestamp);
        }
        return intent;
    }

    private void createMockMessages() {
        mMessageOneIntent= createMessageIntent(mMockBluetoothDeviceOne, "mockHandle",
                "510-111-2222", "testSender",
                "Hello", /* timestamp= */ null, /* isReadOnPhone */ false);
    }
}
