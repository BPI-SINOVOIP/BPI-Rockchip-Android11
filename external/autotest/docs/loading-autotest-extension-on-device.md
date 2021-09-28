# Loading autotestPrivate extension on your device

AutotestPrivate is an extension that exposes APIs that facilitate the
interaction with a Chrome OS device during tests. This guide shows how to load
the extension on your device.

[TOC]


## Prerequisites

You need a device running a Chrome OS test image and a Chromium OS
checkout. To load a test image on your device follow [these steps] from the
Simple Chrome guide. To grab a checkout of Chromium OS follow the
[OS Developer Guide].


## Removing rootfs verification and editing command line flags
NOTE: If you've completed these steps before feel free to skip to [Loading
autotest extension on your device](#Loading-autotest-extension-on-your-device)

To get a command shell on the device:

1.  Run in your workstation:

    `ssh root@$IP_ADDR`

    Use the default password:

    `test0000`

1.  To run Chrome OS with flags, first make usr partition writeable with:

    ```
    /usr/share/vboot/bin/make_dev_ssd.sh --remove_rootfs_verification --partitions 2
    /usr/share/vboot/bin/make_dev_ssd.sh --remove_rootfs_verification --partitions 4
    ```

1.  Reboot your device for rootfs changes to take effect. Run:

    `reboot`

1.  Then append desired flags(listed below) to `/etc/chrome_dev.conf`.

## Loading autotest extension on your device

1.  Enter a Chrome OS chroot. Inside of your Chrome OS checkout directory run:

    `cros_sdk`

1.  From inside your Chrome OS chroot run:

    `test_that $IP_ADDR -b $BOARD dummy_Pass`

    This will install the autotestPrivate extension manifest to your device.

1.  Open a command prompt in your device and edit your command line flags.

    *   Add flag `--remote-debugging-port=9333`
        *   NOTE: Port 9333 is not required, any port will work.
    *   Add flag
        `--load-extension=/usr/local/autotest/common_lib/cros/autotest_private_ext`

1.  In your device's command prompt run

    `restart ui`

    for flag changes to take effect.

1.  Ssh to your device with `ssh -L 9333:localhost:9333 root@$IP_ADDR`

    This will forward your device's devtools port to a local port on your
    workstation.

1.  On your workstation, point your browser to `localhost:9333`.

1.  Click on *Telemetry ChromeOS AutoTest Component Extension*. This opens a
    connection to the extension's backgound page.

1.  Open the console tab.

If you've followed all the steps correctly, you should be able to execute code
in the console tab to interact with the extension.

For example, try running:
``` js
> chrome.autotestPrivate.loginStatus(status => console.log(status));
> {isLoggedIn: false, isOwner: false, isReadyForPassword: false, isScreenLocked: false}
```

[these steps]: https://chromium.googlesource.com/chromiumos/docs/+/master/simple_chrome_workflow.md#set-up-the-chrome-os-device
[OS Developer Guide]: https://chromium.googlesource.com/chromiumos/docs/+/master/developer_guide.md#get-the-source
