#!/usr/bin/env python3
#
#   Copyright 2018 - Google
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

######################################################
# Wifi P2p framework designed value
######################################################
P2P_FIND_TIMEOUT = 120
GO_IP_ADDRESS = '192.168.49.1'

######################################################
# Wifi P2p Acts flow control timer value
######################################################

DEFAULT_TIMEOUT = 30
DEFAULT_SLEEPTIME = 5
DEFAULT_FUNCTION_SWITCH_TIME = 10
DEFAULT_SERVICE_WAITING_TIME = 20
DEFAULT_GROUP_CLIENT_LOST_TIME = 60

P2P_CONNECT_NEGOTIATION = 0
P2P_CONNECT_JOIN = 1
P2P_CONNECT_INVITATION = 2
######################################################
# Wifi P2p sl4a Event String
######################################################
CONNECTED_EVENT = "WifiP2pConnected"
DISCONNECTED_EVENT = "WifiP2pDisconnected"
PEER_AVAILABLE_EVENT = "WifiP2pOnPeersAvailable"
CONNECTION_INFO_AVAILABLE_EVENT = "WifiP2pOnConnectionInfoAvailable"
ONGOING_PEER_INFO_AVAILABLE_EVENT = "WifiP2pOnOngoingPeerAvailable"
ONGOING_PEER_SET_SUCCESS_EVENT = "WifiP2psetP2pPeerConfigureOnSuccess"
CONNECT_SUCCESS_EVENT = "WifiP2pConnectOnSuccess"
CREATE_GROUP_SUCCESS_EVENT = "WifiP2pCreateGroupOnSuccess"
SET_CHANNEL_SUCCESS_EVENT = "WifiP2pSetChannelsOnSuccess"
GROUP_INFO_AVAILABLE_EVENT = "WifiP2pOnGroupInfoAvailable"

######################################################
# Wifi P2p local service event
####################################################

DNSSD_EVENT = "WifiP2pOnDnsSdServiceAvailable"
DNSSD_TXRECORD_EVENT = "WifiP2pOnDnsSdTxtRecordAvailable"
UPNP_EVENT = "WifiP2pOnUpnpServiceAvailable"

DNSSD_EVENT_INSTANCENAME_KEY = "InstanceName"
DNSSD_EVENT_REGISTRATIONTYPE_KEY = "RegistrationType"
DNSSD_TXRECORD_EVENT_FULLDOMAINNAME_KEY = "FullDomainName"
DNSSD_TXRECORD_EVENT_TXRECORDMAP_KEY = "TxtRecordMap"
UPNP_EVENT_SERVICELIST_KEY = "ServiceList"

######################################################
# Wifi P2p local service type
####################################################
P2P_LOCAL_SERVICE_UPNP = 0
P2P_LOCAL_SERVICE_IPP = 1
P2P_LOCAL_SERVICE_AFP = 2

######################################################
# Wifi P2p group capability
######################################################
P2P_GROUP_CAPAB_GROUP_OWNER = 1


######################################################
# Wifi P2p UPnP MediaRenderer local service
######################################################
class UpnpTestData():
    AVTransport = "urn:schemas-upnp-org:service:AVTransport:1"
    ConnectionManager = "urn:schemas-upnp-org:service:ConnectionManager:1"
    serviceType = "urn:schemas-upnp-org:device:MediaRenderer:1"
    uuid = "6859dede-8574-59ab-9332-123456789011"
    rootdevice = "upnp:rootdevice"


######################################################
# Wifi P2p Bonjour IPP & AFP local service
######################################################
class IppTestData():
    ippInstanceName = "MyPrinter"
    ippRegistrationType = "_ipp._tcp"
    ippDomainName = "myprinter._ipp._tcp.local."
    ipp_txtRecord = {"txtvers": "1", "pdl": "application/postscript"}


class AfpTestData():
    afpInstanceName = "Example"
    afpRegistrationType = "_afpovertcp._tcp"
    afpDomainName = "example._afpovertcp._tcp.local."
    afp_txtRecord = {}
