# External CEC adapter

## USB - CEC Adapter from Pulse-Eight

![Picture of USB - CEC Adapter](https://www.pulse-eight.com/generated-assets/products/0000237_555.jpeg)

## Get the hardware

*   [Order from Pulse-Eight](https://www.pulse-eight.com/p/104/usb-hdmi-cec-adapter#)
*   [Pulse-Eight USB CEC adapter on Amazon](https://www.amazon.com/s/ref=nb_sb_ss_i_1_22?url=search-alias%3Daps&field-keywords=pulse+eight+usb+cec+adapter&sprefix=usb+cec+adapter+pulse+%2Caps%2C218&crid=UK4LY390M5H2)

## Get the software {#software}

1.  Connect "TV" port on the adapter to your TV
2.  Connect USB mini port to you computer
3.  Install the cec-client

    *   Linux

    ```shell
    sudo apt-get install cec-utils
    ```

    *   mac (using [MacPorts](https://guide.macports.org/#installing))

    ```shell
     sudo /opt/local/bin/port install libcec
    ```

4.  run the client

    ```shell
    $ cec-client
    No device type given. Using 'recording device'
    CEC Parser created - libCEC version 4.0.2
    no serial port given. trying autodetect:
     path:     /dev/cu.usbmodemv1
     com port: /dev/cu.usbmodemv1

    opening a connection to the CEC adapter...
    DEBUG:   [               1] Broadcast (F): osd name set to 'Broadcast'
    DEBUG:   [               3] connection opened, clearing any previous input     and waiting for active transmissions to end before starting
    DEBUG:   [              10] communication thread started
    DEBUG:   [              70] turning controlled mode on
    NOTICE:  [             255] connection opened
    DEBUG:   [             255] processor thread started
    DEBUG:   [             255] << Broadcast (F) -> TV (0): POLL
    TRAFFIC: [             255] << f0
    DEBUG:   [             255] setting the line timeout to 3
    DEBUG:   [             403] >> POLL sent
    DEBUG:   [             403] TV (0): device status changed into 'present'
    ```

## Add timestamps

Use the `ts` command to add timestamps.

```shell
$ cec-client  | ts
Nov 18 16:15:46 No device type given. Using 'recording device'
Nov 18 16:15:46 CEC Parser created - libCEC version 4.0.4
Nov 18 16:15:46 no serial port given. trying autodetect:
Nov 18 16:15:46  path:     /sys/devices/pci0000:00/0000:00:14.0/usb2/2-9
Nov 18 16:15:46  com port: /dev/ttyACM0
Nov 18 16:15:46
Nov 18 16:15:46 opening a connection to the CEC adapter...
Nov 18 16:15:46 DEBUG:   [             386] Broadcast (F): osd name set to 'Broadcast'
```

### ts is part of the moreutils package

```shell
sudo apt-get install moreutils
```

## cheat sheets

*   Show all connected devices

    ```shell
    $ echo scan | cec-client -s -d 1

    ```

## Available Commands

[tx] \{bytes\}
:   transfer bytes over the CEC line.

[txn] \{bytes\}
:   transfer bytes but don't wait for transmission ACK.

[on] \{address\}
:   power on the device with the given logical address.

[standby] \{address\}
:   put the device with the given address in standby mode.

[la] \{logical address\}
:   change the logical address of the CEC adapter.

[p] \{device\} \{port\}
:   change the HDMI port number of the CEC adapter.

[pa] \{physical address\}
:   change the physical address of the CEC adapter.

[as]
:   make the CEC adapter the active source.

[is]
:   mark the CEC adapter as inactive source.

[osd] \{addr\} \{string\}
:   set OSD message on the specified device.

[ver] \{addr\}
:   get the CEC version of the specified device.

[ven] \{addr\}
:   get the vendor ID of the specified device.

[lang] \{addr\}
:   get the menu language of the specified device.

[pow] \{addr\}
:   get the power status of the specified device.

[name] \{addr\}
:   get the OSD name of the specified device.

[poll] \{addr\}
:   poll the specified device.

[lad]
:   lists active devices on the bus

[ad] \{addr\}
:   checks whether the specified device is active.

[at] \{type\}
:   checks whether the specified device type is active.

[sp] \{addr\}
:   makes the specified physical address active.

[spl] \{addr\}
:   makes the specified logical address active.

[volup]
:   send a volume up command to the amp if present

[voldown]
:   send a volume down command to the amp if present

[mute]
:   send a mute/unmute command to the amp if present

[self]
:   show the list of addresses controlled by libCEC

[scan]
:   scan the CEC bus and display device info

## Sending Remote Control Events

You can send CEC remote events using the `tx` command above. The format is as
follows:

```
tx <source id><destination id>:<command id>:<param value>
```

where all of the above parameters should be filled in with a single HEX digit
(except `<command id>`, which requires 2 digits). Here, we want to send commands
to the Android TV, so we will place its ID in `<destination id>`. The scan
command above will give you the IDs for each device that the CEC adapter is
aware of.

In the examples below the DUT is a CEC player device with a logical address of
4. Here are some useful commands to execute remote actions:

*   Press home

    ```
    tx 04:44:09
    ```

*   Press select

    ```
    tx 04:44:00
    ```

*   Press d-pad up

    ```
    tx 04:44:01
    ```

*   Press d-pad down

    ```
    tx 04:44:02
    ```

*   Press d-pad left

    ```
    tx 04:44:03
    ```

*   Press d-pad right

    ```
    tx 04:44:04
    ```

You can check out [https://www.cec-o-matic.com/](https]://www.cec-o-matic.com/)
for more info on formatting your request and a full list of commands and their
respective parameters.
