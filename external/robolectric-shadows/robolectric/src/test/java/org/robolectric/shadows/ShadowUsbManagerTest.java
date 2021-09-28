package org.robolectric.shadows;

import static android.os.Build.VERSION_CODES.Q;
import static android.os.Build.VERSION_CODES.P;
import static android.os.Build.VERSION_CODES.M;
import static android.os.Build.VERSION_CODES.N;
import static android.os.Build.VERSION_CODES.N_MR1;
import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;
import static org.robolectric.util.ReflectionHelpers.callInstanceMethod;

import android.content.Context;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.hardware.usb.UsbPort;
import android.hardware.usb.UsbPortStatus;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

/** Unit tests for {@link ShadowUsbManager}. */
@RunWith(AndroidJUnit4.class)
public class ShadowUsbManagerTest {
  private static final String DEVICE_NAME_1 = "usb1";
  private static final String DEVICE_NAME_2 = "usb2";
  private UsbManager usbManager;
  private ShadowUsbManager shadowUsbManager;

  @Mock UsbDevice usbDevice1;
  @Mock UsbDevice usbDevice2;
  @Mock UsbAccessory usbAccessory;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    usbManager =
        (UsbManager)
            ApplicationProvider.getApplicationContext().getSystemService(Context.USB_SERVICE);
    shadowUsbManager = shadowOf(usbManager);

    when(usbDevice1.getDeviceName()).thenReturn(DEVICE_NAME_1);
    when(usbDevice2.getDeviceName()).thenReturn(DEVICE_NAME_2);
  }

  @Test
  public void getDeviceList() {
    assertThat(usbManager.getDeviceList()).isEmpty();
    shadowUsbManager.addOrUpdateUsbDevice(usbDevice1, true);
    shadowUsbManager.addOrUpdateUsbDevice(usbDevice2, true);
    assertThat(usbManager.getDeviceList().values())
        .containsExactly(usbDevice1, usbDevice2);
  }

  @Test
  public void hasPermission() {
    assertThat(usbManager.hasPermission(usbDevice1)).isFalse();

    shadowUsbManager.addOrUpdateUsbDevice(usbDevice1, false);
    shadowUsbManager.addOrUpdateUsbDevice(usbDevice2, false);

    assertThat(usbManager.hasPermission(usbDevice1)).isFalse();
    assertThat(usbManager.hasPermission(usbDevice2)).isFalse();

    shadowUsbManager.addOrUpdateUsbDevice(usbDevice1, true);

    assertThat(usbManager.hasPermission(usbDevice1)).isTrue();
    assertThat(usbManager.hasPermission(usbDevice2)).isFalse();
  }

  @Test
  @Config(minSdk = N)
  public void grantPermission_selfPackage_shouldHavePermission() {
    usbManager.grantPermission(usbDevice1);

    assertThat(usbManager.hasPermission(usbDevice1)).isTrue();
  }

  @Test
  @Config(minSdk = N_MR1)
  public void grantPermission_differentPackage_shouldHavePermission() {
    usbManager.grantPermission(usbDevice1, "foo.bar");

    assertThat(shadowUsbManager.hasPermissionForPackage(usbDevice1, "foo.bar")).isTrue();
  }

  @Test
  @Config(minSdk = N_MR1)
  public void revokePermission_shouldNotHavePermission() {
    usbManager.grantPermission(usbDevice1, "foo.bar");
    assertThat(shadowUsbManager.hasPermissionForPackage(usbDevice1, "foo.bar")).isTrue();

    shadowUsbManager.revokePermission(usbDevice1, "foo.bar");

    assertThat(shadowUsbManager.hasPermissionForPackage(usbDevice1, "foo.bar")).isFalse();
  }

  @Test
  @Config(minSdk = M, maxSdk = P)
  public void getPorts_shouldReturnAddedPortsMtoP() {
    shadowUsbManager.addPort("port1");
    shadowUsbManager.addPort("port2");
    shadowUsbManager.addPort("port3");

    UsbPort[] usbPorts = callInstanceMethod(usbManager, "getPorts");
    assertThat(usbPorts).hasLength(3);
    assertThat(Arrays.stream(usbPorts).map(UsbPort::getId).collect(Collectors.toList()))
            .containsExactly("port1", "port2", "port3");
  }

  @Test
  @Config(minSdk = Q)
  public void getPorts_shouldReturnAddedPorts() {
    shadowUsbManager.addPort("port1");
    shadowUsbManager.addPort("port2");
    shadowUsbManager.addPort("port3");

    List<UsbPort> usbPorts = usbManager.getPorts();
    assertThat(usbPorts).hasSize(3);
    assertThat(usbPorts.stream().map(UsbPort::getId).collect(Collectors.toList()))
        .containsExactly("port1", "port2", "port3");
  }

  @Test
  @Config(minSdk = Q)
  public void clearPorts_shouldRemoveAllPorts() {
    shadowUsbManager.addPort("port1");
    shadowUsbManager.clearPorts();

    List<UsbPort> usbPorts = usbManager.getPorts();
    assertThat(usbPorts).isEmpty();
  }

  @Test
  @Config(minSdk = Q)
  public void setPortRoles_sinkHost_shouldSetPortStatus() {
    shadowUsbManager.addPort("port1");

    List<UsbPort> usbPorts = usbManager.getPorts();
    usbPorts.get(0).setRoles(UsbPortStatus.POWER_ROLE_SINK, UsbPortStatus.DATA_ROLE_HOST);

    UsbPortStatus usbPortStatus = usbPorts.get(0).getStatus();
    assertThat(usbPortStatus.getCurrentPowerRole()).isEqualTo(UsbPortStatus.POWER_ROLE_SINK);
    assertThat(usbPortStatus.getCurrentDataRole()).isEqualTo(UsbPortStatus.DATA_ROLE_HOST);
  }

  @Test
  public void removeDevice() {
    assertThat(usbManager.getDeviceList()).isEmpty();
    shadowUsbManager.addOrUpdateUsbDevice(usbDevice1, false);
    shadowUsbManager.addOrUpdateUsbDevice(usbDevice2, false);

    assertThat(usbManager.getDeviceList().values())
        .containsExactly(usbDevice1, usbDevice2);

    shadowUsbManager.removeUsbDevice(usbDevice1);
    assertThat(usbManager.getDeviceList().values()).containsExactly(usbDevice2);
  }

  @Test
  public void openAccessory() {
    assertThat(usbManager.openAccessory(usbAccessory)).isNotNull();
  }

  @Test
  public void setAccessory() {
    assertThat(usbManager.getAccessoryList()).isNull();
    shadowUsbManager.setAttachedUsbAccessory(usbAccessory);
    assertThat(usbManager.getAccessoryList()).hasLength(1);
    assertThat(usbManager.getAccessoryList()[0]).isEqualTo(usbAccessory);
  }
}
