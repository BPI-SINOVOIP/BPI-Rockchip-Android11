# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ATMEL_USB_VENDOR_ID = "03eb"
SERVO_USB_KBD_DEV_ID = ATMEL_USB_VENDOR_ID + ":2042"


def is_servo_usb_keyboard_present(host):
    """
    Check if DUT can see the servo USB keyboard.

    Run lsusb and look for USB devices with SERVO_USB_KBD_DEV_ID.

    @param host: An Autotest host object

    @return Boolean, True if the USB device is found. False otherwise.
    """
    return host.run(
        "lsusb -d " + SERVO_USB_KBD_DEV_ID, ignore_status=True).exit_status == 0


def is_servo_usb_wake_capable(host):
    """
    Check if servo USB keyboard can wake the DUT from S3/S0ix.

    Run lsusb -vv -d SERVO_USB_KBD_DEV_ID and check if the keyboard has wake
    capability.

    @param host: An Autotest host object

    @return Boolean, True if the USB device is found and has wake capability.
        False otherwise.

    """
    # If the DUT cannot see the USB device return False.
    if not is_servo_usb_keyboard_present(host):
        return False
    result = host.run(
        "lsusb -vv -d " + SERVO_USB_KBD_DEV_ID,
        ignore_status=True).stdout.strip()
    # lsusb should print "Remote Wakeup" if the device has remote wake
    # capability.
    return "Remote Wakeup" in result
