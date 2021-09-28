#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
"""Interface for a USB-connected Monsoon power meter
(http://msoon.com/LabEquipment/PowerMonitor/).
"""

import argparse

import acts.controllers.monsoon as monsoon_controller


def main(args):
    """Simple command-line interface for Monsoon."""
    if args.avg and args.avg < 0:
        print('--avg must be greater than 0')
        return

    mon = monsoon_controller.create([int(args.serialno[0])])[0]

    if args.voltage is not None:
        mon.set_voltage(args.voltage)

    if args.current is not None:
        mon.set_max_current(args.current)

    if args.status:
        items = sorted(mon.status.items())
        print('\n'.join(['%s: %s' % item for item in items]))

    if args.usbpassthrough:
        mon.usb(args.usbpassthrough)

    if args.startcurrent is not None:
        mon.set_max_initial_current(args.startcurrent)

    if args.samples:
        result = mon.measure_power(
            args.samples / args.hz,
            measure_after_seconds=args.offset,
            hz=args.hz,
            output_path='monsoon_output.txt')
        print(repr(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='This is a python utility tool to control monsoon power '
                    'measurement boxes.')
    parser.add_argument(
        '--status', action='store_true', help='Print power meter status.')
    parser.add_argument(
        '-avg',
        '--avg',
        type=int,
        default=0,
        help='Also report average over last n data points.')
    parser.add_argument(
        '-v', '--voltage', type=float, help='Set output voltage (0 for off)')
    parser.add_argument(
        '-c', '--current', type=float, help='Set max output current.')
    parser.add_argument(
        '-sc',
        '--startcurrent',
        type=float,
        help='Set max power-up/initial current.')
    parser.add_argument(
        '-usb',
        '--usbpassthrough',
        choices=('on', 'off', 'auto'),
        help='USB control (on, off, auto).')
    parser.add_argument(
        '-sp',
        '--samples',
        type=int,
        help='Collect and print this many samples')
    parser.add_argument(
        '-hz', '--hz', type=int, help='Sample this many times per second.')
    parser.add_argument('-d', '--device', help='Use this /dev/ttyACM... file.')
    parser.add_argument(
        '-sn',
        '--serialno',
        type=int,
        nargs=1,
        required=True,
        help='The serial number of the Monsoon to use.')
    parser.add_argument(
        '--offset',
        type=int,
        nargs='?',
        default=0,
        help='The number of samples to discard when calculating average.')
    parser.add_argument(
        '-r',
        '--ramp',
        action='store_true',
        help='Gradually increase voltage to prevent tripping Monsoon '
             'overvoltage.')
    arguments = parser.parse_args()
    main(arguments)
