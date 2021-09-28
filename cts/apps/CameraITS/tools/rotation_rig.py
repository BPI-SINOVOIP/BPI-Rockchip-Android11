# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import codecs
import logging
import struct
import sys
import time
import hardware as hw
import pyudev
import serial
import serial.tools.list_ports

ARDUINO_ANGLE_MAX = 180.0  # degrees
ARDUINO_ANGLES = [0]*5 + range(0, 90, 3) + [90]*5 + range(90, -1, -3)
ARDUINO_BAUDRATE = 9600
ARDUINO_CMD_LENGTH = 3
ARDUINO_CMD_TIME = 2.0 * ARDUINO_CMD_LENGTH / ARDUINO_BAUDRATE  # round trip
ARDUINO_DEFAULT_CH = '1'
ARDUINO_MOVE_TIME = 0.06 - ARDUINO_CMD_TIME  # seconds
ARDUINO_START_BYTE = 255
ARDUINO_START_NUM_TRYS = 3
ARDUINO_TEST_CMD = [b'\x01', b'\x02', b'\x03']
ARDUINO_VALID_CH = ['1', '2', '3', '4', '5', '6']
CANAKIT_BAUDRATE = 115200
CANAKIT_COM_SLEEP = 0.05
CANAKIT_DATA_DELIMITER = '\r\n'
CANAKIT_DEFAULT_CH = '1'
CANAKIT_DEVICE = 'relay'
CANAKIT_PID = 'fc73'
CANAKIT_SET_CMD = 'REL'
CANAKIT_SLEEP_TIME = 2  # seconds
CANAKIT_VALID_CMD = ['ON', 'OFF']
CANAKIT_VALID_CH = ['1', '2', '3', '4']
CANAKIT_VID = '04d8'
HS755HB_ANGLE_MAX = 202.0  # degrees
NUM_ROTATIONS = 10
SERIAL_SEND_TIMEOUT = 0.02


def get_cmd_line_args():
    """Get command line arguments.

    Args:
        None, but gets sys.argv()
    Returns:
        rotate_cntl:    str; 'arduino' or 'canakit'
        rotate_ch:      dict; arduino -> {'ch': str}
                              canakit --> {'vid': str, 'pid': str, 'ch': str}
        num_rotations:  int; number of rotations
    """
    num_rotations = NUM_ROTATIONS
    rotate_cntl = 'canakit'
    rotate_ch = {}
    for s in sys.argv[1:]:
        if s[:8] == 'rotator=':
            if len(s) > 8:
                rotator_ids = s[8:].split(':')
                if len(rotator_ids) == 1:
                    # 'rotator=default'
                    if rotator_ids[0] == 'default':
                        print ('Using default values %s:%s:%s for VID:PID:CH '
                               'of rotator' % (CANAKIT_VID, CANAKIT_PID,
                                               CANAKIT_DEFAULT_CH))
                        vid = '0x' + CANAKIT_VID
                        pid = '0x' + CANAKIT_PID
                        ch = CANAKIT_DEFAULT_CH
                        rotate_ch = {'vid': vid, 'pid': pid, 'ch': ch}
                    # 'rotator=$ch'
                    elif rotator_ids[0] in CANAKIT_VALID_CH:
                        print ('Using default values %s:%s for VID:PID '
                               'of rotator' % (CANAKIT_VID, CANAKIT_PID))
                        vid = '0x' + CANAKIT_VID
                        pid = '0x' + CANAKIT_PID
                        ch = rotator_ids[0]
                        rotate_ch = {'vid': vid, 'pid': pid, 'ch': ch}
                    # 'rotator=arduino'
                    elif rotator_ids[0] == 'arduino':
                        rotate_cntl = 'arduino'
                        rotate_ch = {'ch': ARDUINO_DEFAULT_CH}
                # 'rotator=arduino:$ch'
                elif len(rotator_ids) == 2:
                    rotate_cntl = 'arduino'
                    ch = rotator_ids[1]
                    rotate_ch = {'ch': ch}
                    if ch not in ARDUINO_VALID_CH:
                        print 'Invalid arduino ch: %s' % ch
                        print 'Valid channels:', ARDUINO_VALID_CH
                        sys.exit()
                # 'rotator=$vid:$pid:$ch'
                elif len(rotator_ids) == 3:
                    vid = '0x' + rotator_ids[0]
                    pid = '0x' + rotator_ids[1]
                    rotate_ch = {'vid': vid, 'pid': pid, 'ch': rotator_ids[2]}
                else:
                    err_string = 'Rotator ID (if entered) must be of form: '
                    err_string += 'rotator=default or rotator=CH or '
                    err_string += 'rotator=VID:PID:CH or '
                    err_string += 'rotator=arduino or rotator=arduino:CH'
                    print err_string
                    sys.exit()
            if (rotate_cntl == 'canakit' and
                        rotate_ch['ch'] not in CANAKIT_VALID_CH):
                print 'Invalid canakit ch: %s' % rotate_ch['ch']
                print 'Valid channels:', CANAKIT_VALID_CH
                sys.exit()

        if s[:14] == 'num_rotations=':
            num_rotations = int(s[14:])

    return rotate_cntl, rotate_ch, num_rotations


def serial_port_def(name):
    """Determine the serial port and open.

    Args:
        name:   str; device to locate (ie. 'Arduino')
    Returns:
        serial port object
    """
    devices = pyudev.Context()
    for device in devices.list_devices(subsystem='tty', ID_BUS='usb'):
        if name in device['ID_VENDOR']:
            arduino_port = device['DEVNAME']
            break

    return serial.Serial(arduino_port, ARDUINO_BAUDRATE, timeout=1)


def arduino_read_cmd(port):
    """Read back Arduino command from serial port."""
    cmd = []
    for _ in range(ARDUINO_CMD_LENGTH):
        cmd.append(port.read())
    return cmd


def arduino_send_cmd(port, cmd):
    """Send command to serial port."""
    for i in range(ARDUINO_CMD_LENGTH):
        port.write(cmd[i])


def arduino_loopback_cmd(port, cmd):
    """Send command to serial port."""
    arduino_send_cmd(port, cmd)
    time.sleep(ARDUINO_CMD_TIME)
    return arduino_read_cmd(port)


def establish_serial_comm(port):
    """Establish connection with serial port."""
    print 'Establishing communication with %s' % port.name
    trys = 1
    hex_test = convert_to_hex(ARDUINO_TEST_CMD)
    logging.info(' test tx: %s %s %s', hex_test[0], hex_test[1], hex_test[2])
    while trys <= ARDUINO_START_NUM_TRYS:
        cmd_read = arduino_loopback_cmd(port, ARDUINO_TEST_CMD)
        hex_read = convert_to_hex(cmd_read)
        logging.info(' test rx: %s %s %s',
                     hex_read[0], hex_read[1], hex_read[2])
        if cmd_read != ARDUINO_TEST_CMD:
            trys += 1
        else:
            logging.info(' Arduino comm established after %d try(s)', trys)
            break


def convert_to_hex(cmd):
    # compatible with both python 2 and python 3
    return [('%0.2x' % int(codecs.encode(x, 'hex_codec'), 16) if x else '--')
            for x in cmd]


def arduino_rotate_servo_to_angle(ch, angle, serial_port, delay=0):
    """Rotate servo to the specified angle.

    Args:
        ch:             str; servo to rotate in ARDUINO_VALID_CH
        angle:          int; servo angle to move to
        serial_port:    object; serial port
        delay:          int; time in seconds
    """

    err_msg = 'Angle must be between 0 and %d.' % (ARDUINO_ANGLE_MAX)
    if angle < 0:
        print err_msg
        angle = 0
    elif angle > ARDUINO_ANGLE_MAX:
        print err_msg
        angle = ARDUINO_ANGLE_MAX
    cmd = [struct.pack('B', i) for i in [ARDUINO_START_BYTE, int(ch), angle]]
    arduino_send_cmd(serial_port, cmd)
    time.sleep(delay)


def arduino_rotate_servo(ch, serial_port):
    """Rotate servo between 0 --> 90 --> 0.

    Args:
        ch:             str; servo to rotate
        serial_port:    object; serial port
    """
    for angle in ARDUINO_ANGLES:
        angle_norm = int(round(angle*ARDUINO_ANGLE_MAX/HS755HB_ANGLE_MAX, 0))
        arduino_rotate_servo_to_angle(
                ch, angle_norm, serial_port, ARDUINO_MOVE_TIME)


def canakit_cmd_send(vid, pid, cmd_str):
    """Wrapper for sending serial command.

    Args:
        vid:     str; vendor ID
        pid:     str; product ID
        cmd_str: str; value to send to device.
    """
    hw_list = hw.Device(CANAKIT_DEVICE, vid, pid, '1', '0')
    relay_port = hw_list.get_tty_path('relay')
    relay_ser = serial.Serial(relay_port, CANAKIT_BAUDRATE,
                              timeout=SERIAL_SEND_TIMEOUT,
                              parity=serial.PARITY_EVEN,
                              stopbits=serial.STOPBITS_ONE,
                              bytesize=serial.EIGHTBITS)
    try:
        relay_ser.write(CANAKIT_DATA_DELIMITER)
        time.sleep(CANAKIT_COM_SLEEP)  # This is critical for relay.
        relay_ser.write(cmd_str)
        relay_ser.close()
    except ValueError:
        print 'Port %s:%s is not open' % (vid, pid)
        sys.exit()


def set_relay_channel_state(vid, pid, ch, relay_state):
    """Set relay channel and state.

    Args:
        vid:          str; vendor ID
        pid:          str; product ID
        ch:           str; channel number of relay to set. '1', '2', '3', or '4'
        relay_state:  str; either 'ON' or 'OFF'
    Returns:
        None
    """
    if ch in CANAKIT_VALID_CH and relay_state in CANAKIT_VALID_CMD:
        canakit_cmd_send(
                vid, pid, CANAKIT_SET_CMD + ch + '.' + relay_state + '\r\n')
    else:
        print 'Invalid channel or command, no command sent.'


def main():
    """Main function.

    expected rotator string is vid:pid:ch or arduino:ch.
    Canakit vid:pid can be found through lsusb on the host.
    ch is hard wired and must be determined from the controller.
    """
    # set up logging for debug info
    logging.basicConfig(level=logging.INFO)

    # get cmd line args
    rotate_cntl, rotate_ch, num_rotations = get_cmd_line_args()
    ch = rotate_ch['ch']
    print 'Controller: %s, ch: %s' % (rotate_cntl, ch)

    # initialize port and cmd strings
    if rotate_cntl == 'arduino':
        # initialize Arduino port
        serial_port = serial_port_def('Arduino')

        # send test cmd to Arduino until cmd returns properly
        establish_serial_comm(serial_port)

        # initialize servo at origin
        print 'Moving servo to origin'
        arduino_rotate_servo_to_angle(ch, 0, serial_port, 1)
    else:
        vid = rotate_ch['vid']
        pid = rotate_ch['pid']

    # rotate phone
    print 'Rotating phone %dx' % num_rotations
    for _ in xrange(num_rotations):
        if rotate_cntl == 'arduino':
            arduino_rotate_servo(ch, serial_port)
        else:
            set_relay_channel_state(vid, pid, ch, 'ON')
            time.sleep(CANAKIT_SLEEP_TIME)
            set_relay_channel_state(vid, pid, ch, 'OFF')
            time.sleep(CANAKIT_SLEEP_TIME)
    print 'Finished rotations'


if __name__ == '__main__':
    main()
