#!/usr/bin/python2
# Takes a list of hostnames (via file) and schedules host repair
# jobs based on the delay specified in order to throttle the jobs
# and not overwhelm the system.

import argparse
import sys

import common
import time

from autotest_lib.server import frontend

def GetParser():
    """Creates the argparse parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=str, action='store',
                        help='File with hostnames to repair')
    parser.add_argument('--delay_seconds', type=int, action='store', default=5,
                        help='Delay between scheduling repair jobs')
    return parser


def main(argv):
    parser = GetParser()
    options = parser.parse_args(argv)

    afe = frontend.AFE()

    with open(options.input) as input:
        hostnames = input.readlines()
        remaining = len(hostnames)
        delay = options.delay_seconds
        print "Scheduling %d repairs with %s delay in seconds" \
              % (remaining, delay)
        for hostname in hostnames:
            hostname = hostname.strip()
            afe.repair_hosts([hostname])
            remaining = remaining - 1
            print "%s host repair scheduled with %d remaining" \
                  % (hostname, remaining)
            time.sleep(delay)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
