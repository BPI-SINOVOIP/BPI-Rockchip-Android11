package android.car.usb.handler;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.UserHandle;
import android.os.UserManager;

import java.util.ArrayList;
import java.util.HashMap;

/** Queues work to the BootUsbService job to scan for connected devices. */
public class BootUsbScanner extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        // Only start the service for the non-system user, or for the system user if it is running
        // as a "real" user. This ensures the service is started only once and is started even on a
        // foreground user switch.
        if (context.getUserId() == UserHandle.USER_SYSTEM
                && !UserManager.isHeadlessSystemUserMode()) {
            return;
        }
        // we defer this processing to BootUsbService so that we are very quick to process
        // LOCKED_BOOT_COMPLETED
        UsbManager usbManager = context.getSystemService(UsbManager.class);
        HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
        if (deviceList.size() > 0) {
            Intent bootUsbServiceIntent = new Intent(context, BootUsbService.class);
            bootUsbServiceIntent.putParcelableArrayListExtra(
                    BootUsbService.USB_DEVICE_LIST_KEY, new ArrayList<>(deviceList.values()));

            context.startForegroundService(bootUsbServiceIntent);
        }
    }
}
