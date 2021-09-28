#
#   Copyright 2018 - The Android Open Source Project
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

from acts import asserts
from acts.test_utils.net import connectivity_const as cconst
from queue import Empty

def _listen_for_keepalive_event(ad, key, msg, ka_event):
    """Listen for keepalive event and return status

    Args:
        ad: DUT object
        key: keepalive key
        msg: Error message
        event: Keepalive event type
    """
    ad.droid.socketKeepaliveStartListeningForEvent(key, ka_event)
    try:
        event = ad.ed.pop_event("SocketKeepaliveCallback")
        status = event["data"]["socketKeepaliveEvent"] == ka_event
    except Empty:
        asserts.fail(msg)
    finally:
        ad.droid.socketKeepaliveStopListeningForEvent(key, ka_event)
    if ka_event != "Started":
        ad.droid.removeSocketKeepaliveReceiverKey(key)
    if status:
        ad.log.info("'%s' keepalive event successful" % ka_event)
    return status

def start_natt_socket_keepalive(ad, udp_encap, src_ip, dst_ip, interval = 10):
    """Start NATT SocketKeepalive on DUT

    Args:
        ad: DUT object
        udp_encap: udp_encap socket key
        src_ip: IP addr of the client
        dst_ip: IP addr of the keepalive server
        interval: keepalive time interval
    """
    ad.log.info("Starting Natt Socket Keepalive")
    key = ad.droid.startNattSocketKeepalive(udp_encap, src_ip, dst_ip, interval)
    msg = "Failed to receive confirmation of starting natt socket keeaplive"
    status = _listen_for_keepalive_event(ad, key, msg, "Started")
    return key if status else None

def start_tcp_socket_keepalive(ad, socket, time_interval = 10):
    """Start TCP socket keepalive on DUT

    Args:
        ad: DUT object
        socket: TCP socket key
        time_interval: Keepalive time interval
    """
    ad.log.info("Starting TCP Socket Keepalive")
    key = ad.droid.startTcpSocketKeepalive(socket, time_interval)
    msg = "Failed to receive confirmation of starting tcp socket keeaplive"
    status = _listen_for_keepalive_event(ad, key, msg, "Started")
    return key if status else None

def socket_keepalive_error(ad, key):
    """Verify Error callback

    Args:
        ad: DUT object
        key: Keepalive key
    """
    ad.log.info("Verify Error callback on keepalive: %s" % key)
    msg = "Failed to receive confirmation of Error callback"
    return _listen_for_keepalive_event(ad, key, msg, "Error")

def socket_keepalive_data_received(ad, key):
    """Verify OnDataReceived callback

    Args:
        ad: DUT object
        key: Keepalive key
    """
    ad.log.info("Verify OnDataReceived callback on keepalive: %s" % key)
    msg = "Failed to receive confirmation of OnDataReceived callback"
    return _listen_for_keepalive_event(ad, key, msg, "OnDataReceived")

def stop_socket_keepalive(ad, key):
    """Stop SocketKeepalive on DUT

    Args:
        ad: DUT object
        key: Keepalive key
    """
    ad.log.info("Stopping Socket keepalive: %s" % key)
    ad.droid.stopSocketKeepalive(key)
    msg = "Failed to receive confirmation of stopping socket keepalive"
    return _listen_for_keepalive_event(ad, key, msg, "Stopped")

def set_private_dns(ad, dns_mode, hostname=None):
    """ Set private DNS mode on dut """
    if dns_mode == cconst.PRIVATE_DNS_MODE_OFF:
        ad.droid.setPrivateDnsMode(False)
    else:
        ad.droid.setPrivateDnsMode(True, hostname)

    mode = ad.droid.getPrivateDnsMode()
    host = ad.droid.getPrivateDnsSpecifier()
    ad.log.info("DNS mode is %s and DNS server is %s" % (mode, host))
    asserts.assert_true(dns_mode == mode and host == hostname,
                        "Failed to set DNS mode to %s and DNS to %s" % \
                        (dns_mode, hostname))
