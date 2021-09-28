#!/usr/bin/python

import argparse
import time

from scapy import all as scapy


def send(dstmac, interval, count, lifetime, iface, rtt):
    """Generate IPv6 Router Advertisement and send to destination.

    Args:
      1. dstmac: string HWAddr of the destination ipv6 node.
      2. interval: int Time to sleep between consecutive packets.
      3. count: int Number of packets to be sent.
      4. lifetime: Router lifetime value for the original RA.
      5. iface: string Router's WiFi interface to send packets over.
      6. rtt: retrans timer in the RA packet

    """
    while count:
        ra = (scapy.Ether(dst=dstmac) /
              scapy.IPv6() /
              scapy.ICMPv6ND_RA(routerlifetime=lifetime, retranstimer=rtt))
        scapy.sendp(ra, iface=iface)
        count = count - 1
        time.sleep(interval)
        lifetime = lifetime - interval


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-m', '--mac-address', action='store', default=None,
                         help='HWAddr to send the packet to.')
    parser.add_argument('-i', '--t-interval', action='store', default=None,
                         type=int, help='Time to sleep between consecutive')
    parser.add_argument('-c', '--pkt-count', action='store', default=None,
                        type=int, help='NUmber of packets to send.')
    parser.add_argument('-l', '--life-time', action='store', default=None,
                        type=int, help='Lifetime in seconds for the first RA')
    parser.add_argument('-in', '--wifi-interface', action='store', default=None,
                        help='The wifi interface to send packets over.')
    parser.add_argument('-rtt', '--retrans-timer', action='store', default=None,
                        type=int, help='Retrans timer')
    args = parser.parse_args()
    send(args.mac_address, args.t_interval, args.pkt_count, args.life_time,
         args.wifi_interface, args.retrans_timer)
