#!/usr/bin/env python3
#
#   Copyright 2018 Google, Inc.
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

import logging
import os
import time

from queue import Empty

from acts import asserts
from acts import utils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts
import acts.utils


def is_discovered(event, ad):
    """Check an Android device exist in WifiP2pOnPeersAvailable event or not.

    Args:
        event: WifiP2pOnPeersAvailable which include all of p2p devices.
        ad: The android device
    Returns:
        True: if an Android device exist in p2p list
        False: if not exist
    """
    for device in event['data']['Peers']:
        if device['Name'] == ad.name:
            ad.deviceAddress = device['Address']
            return True
    return False


def check_disconnect(ad, timeout=p2pconsts.DEFAULT_TIMEOUT):
    """Check an Android device disconnect or not

    Args:
        ad: The android device
    """
    ad.droid.wifiP2pRequestConnectionInfo()
    # wait disconnect event
    ad.ed.pop_event(p2pconsts.DISCONNECTED_EVENT, timeout)


def p2p_disconnect(ad):
    """Invoke an Android device removeGroup to trigger p2p disconnect

    Args:
        ad: The android device
    """
    ad.log.debug("Disconnect")
    ad.droid.wifiP2pRemoveGroup()
    check_disconnect(ad)


def p2p_connection_ping_test(ad, target_ip_address):
    """Let an Android device to start ping target_ip_address

    Args:
        ad: The android device
        target_ip_address: ip address which would like to ping
    """
    ad.log.debug("Run Ping Test, %s ping %s " % (ad.serial, target_ip_address))
    asserts.assert_true(
        acts.utils.adb_shell_ping(ad,
                                  count=6,
                                  dest_ip=target_ip_address,
                                  timeout=20), "%s ping failed" % (ad.serial))


def is_go(ad):
    """Check an Android p2p role is Go or not

    Args:
        ad: The android device
    Return:
        True: An Android device is p2p  go
        False: An Android device is p2p gc
    """
    ad.log.debug("is go check")
    ad.droid.wifiP2pRequestConnectionInfo()
    ad_connect_info_event = ad.ed.pop_event(
        p2pconsts.CONNECTION_INFO_AVAILABLE_EVENT, p2pconsts.DEFAULT_TIMEOUT)
    if ad_connect_info_event['data']['isGroupOwner']:
        return True
    return False


def p2p_go_ip(ad):
    """Get GO IP address

    Args:
        ad: The android device
    Return:
        GO IP address
    """
    ad.log.debug("p2p go ip")
    ad.droid.wifiP2pRequestConnectionInfo()
    ad_connect_info_event = ad.ed.pop_event(
        p2pconsts.CONNECTION_INFO_AVAILABLE_EVENT, p2pconsts.DEFAULT_TIMEOUT)
    ad.log.debug("p2p go ip: %s" %
                 ad_connect_info_event['data']['groupOwnerHostAddress'])
    return ad_connect_info_event['data']['groupOwnerHostAddress']


def p2p_get_current_group(ad):
    """Get current group information

    Args:
        ad: The android device
    Return:
        p2p group information
    """
    ad.log.debug("get current group")
    ad.droid.wifiP2pRequestGroupInfo()
    ad_group_info_event = ad.ed.pop_event(p2pconsts.GROUP_INFO_AVAILABLE_EVENT,
                                          p2pconsts.DEFAULT_TIMEOUT)
    ad.log.debug(
        "p2p group: SSID:%s, password:%s, owner address: %s, interface: %s" %
        (ad_group_info_event['data']['NetworkName'],
         ad_group_info_event['data']['Passphrase'],
         ad_group_info_event['data']['OwnerAddress'],
         ad_group_info_event['data']['Interface']))
    return ad_group_info_event['data']


#trigger p2p connect to ad2 from ad1
def p2p_connect(ad1,
                ad2,
                isReconnect,
                wpsSetup,
                p2p_connect_type=p2pconsts.P2P_CONNECT_NEGOTIATION,
                go_ad=None):
    """trigger p2p connect to ad2 from ad1

    Args:
        ad1: The android device
        ad2: The android device
        isReconnect: boolean, if persist group is exist,
                isReconnect is true, otherswise is false.
        wpsSetup: which wps connection would like to use
        p2p_connect_type: enumeration, which type this p2p connection is
        go_ad: The group owner android device which is used for the invitation connection
    """
    ad1.log.info("Create p2p connection from %s to %s via wps: %s type %d" %
                 (ad1.name, ad2.name, wpsSetup, p2p_connect_type))
    if p2p_connect_type == p2pconsts.P2P_CONNECT_INVITATION:
        if go_ad is None:
            go_ad = ad1
        find_p2p_device(ad1, ad2)
        find_p2p_group_owner(ad2, go_ad)
    elif p2p_connect_type == p2pconsts.P2P_CONNECT_JOIN:
        find_p2p_group_owner(ad1, ad2)
    else:
        find_p2p_device(ad1, ad2)
    time.sleep(p2pconsts.DEFAULT_SLEEPTIME)
    wifi_p2p_config = {
        WifiP2PEnums.WifiP2pConfig.DEVICEADDRESS_KEY: ad2.deviceAddress,
        WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY: {
            WifiP2PEnums.WpsInfo.WPS_SETUP_KEY: wpsSetup
        }
    }
    ad1.droid.wifiP2pConnect(wifi_p2p_config)
    ad1.ed.pop_event(p2pconsts.CONNECT_SUCCESS_EVENT,
                     p2pconsts.DEFAULT_TIMEOUT)
    time.sleep(p2pconsts.DEFAULT_SLEEPTIME)
    if not isReconnect:
        ad1.droid.requestP2pPeerConfigure()
        ad1_peerConfig = ad1.ed.pop_event(
            p2pconsts.ONGOING_PEER_INFO_AVAILABLE_EVENT,
            p2pconsts.DEFAULT_TIMEOUT)
        ad1.log.debug(ad1_peerConfig['data'])
        ad2.droid.requestP2pPeerConfigure()
        ad2_peerConfig = ad2.ed.pop_event(
            p2pconsts.ONGOING_PEER_INFO_AVAILABLE_EVENT,
            p2pconsts.DEFAULT_TIMEOUT)
        ad2.log.debug(ad2_peerConfig['data'])
        if wpsSetup == WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_DISPLAY:
            asserts.assert_true(
                WifiP2PEnums.WpsInfo.WPS_PIN_KEY in ad1_peerConfig['data'][
                    WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY],
                "Can't get pin value")
            ad2_peerConfig['data'][WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY][
                WifiP2PEnums.WpsInfo.WPS_PIN_KEY] = ad1_peerConfig['data'][
                    WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY][
                        WifiP2PEnums.WpsInfo.WPS_PIN_KEY]
            ad2.droid.setP2pPeerConfigure(ad2_peerConfig['data'])
            ad2.ed.pop_event(p2pconsts.ONGOING_PEER_SET_SUCCESS_EVENT,
                             p2pconsts.DEFAULT_TIMEOUT)
            ad2.droid.wifiP2pAcceptConnection()
        elif wpsSetup == WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_KEYPAD:
            asserts.assert_true(
                WifiP2PEnums.WpsInfo.WPS_PIN_KEY in ad2_peerConfig['data'][
                    WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY],
                "Can't get pin value")
            ad1_peerConfig['data'][WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY][
                WifiP2PEnums.WpsInfo.WPS_PIN_KEY] = ad2_peerConfig['data'][
                    WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY][
                        WifiP2PEnums.WpsInfo.WPS_PIN_KEY]
            ad1.droid.setP2pPeerConfigure(ad1_peerConfig['data'])
            ad1.ed.pop_event(p2pconsts.ONGOING_PEER_SET_SUCCESS_EVENT,
                             p2pconsts.DEFAULT_TIMEOUT)
            #Need to Accept first in ad1 to avoid connect time out in ad2,
            #the timeout just 1 sec in ad2
            ad1.droid.wifiP2pAcceptConnection()
            time.sleep(p2pconsts.DEFAULT_SLEEPTIME)
            ad2.droid.wifiP2pConfirmConnection()
        elif wpsSetup == WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC:
            ad2.droid.wifiP2pAcceptConnection()
            if p2p_connect_type == p2pconsts.P2P_CONNECT_INVITATION:
                time.sleep(p2pconsts.DEFAULT_SLEEPTIME)
                go_ad.droid.wifiP2pAcceptConnection()

    #wait connected event
    if p2p_connect_type == p2pconsts.P2P_CONNECT_INVITATION:
        go_ad.ed.pop_event(p2pconsts.CONNECTED_EVENT,
                           p2pconsts.DEFAULT_TIMEOUT)
    else:
        ad1.ed.pop_event(p2pconsts.CONNECTED_EVENT, p2pconsts.DEFAULT_TIMEOUT)
    ad2.ed.pop_event(p2pconsts.CONNECTED_EVENT, p2pconsts.DEFAULT_TIMEOUT)


def p2p_connect_with_config(ad1, ad2, network_name, passphrase, band):
    """trigger p2p connect to ad2 from ad1 with config

    Args:
        ad1: The android device
        ad2: The android device
        network_name: the network name of the desired group.
        passphrase: the passphrase of the desired group.
        band: the operating band of the desired group.
    """
    ad1.log.info("Create p2p connection from %s to %s" % (ad1.name, ad2.name))
    find_p2p_device(ad1, ad2)
    time.sleep(p2pconsts.DEFAULT_SLEEPTIME)
    wifi_p2p_config = {
        WifiP2PEnums.WifiP2pConfig.NETWORK_NAME: network_name,
        WifiP2PEnums.WifiP2pConfig.PASSPHRASE: passphrase,
        WifiP2PEnums.WifiP2pConfig.GROUP_BAND: band,
        WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY: {
            WifiP2PEnums.WpsInfo.WPS_SETUP_KEY:
            WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC
        }
    }
    ad1.droid.wifiP2pConnect(wifi_p2p_config)
    ad1.ed.pop_event(p2pconsts.CONNECT_SUCCESS_EVENT,
                     p2pconsts.DEFAULT_TIMEOUT)
    time.sleep(p2pconsts.DEFAULT_SLEEPTIME)

    #wait connected event
    ad1.ed.pop_event(p2pconsts.CONNECTED_EVENT, p2pconsts.DEFAULT_TIMEOUT)
    ad2.ed.pop_event(p2pconsts.CONNECTED_EVENT, p2pconsts.DEFAULT_TIMEOUT)


def find_p2p_device(ad1, ad2):
    """Check an Android device ad1 can discover an Android device ad2

    Args:
        ad1: The android device
        ad2: The android device
    """
    ad1.droid.wifiP2pDiscoverPeers()
    ad2.droid.wifiP2pDiscoverPeers()
    p2p_find_result = False
    while not p2p_find_result:
        ad1_event = ad1.ed.pop_event(p2pconsts.PEER_AVAILABLE_EVENT,
                                     p2pconsts.P2P_FIND_TIMEOUT)
        ad1.log.debug(ad1_event['data'])
        p2p_find_result = is_discovered(ad1_event, ad2)
    asserts.assert_true(p2p_find_result,
                        "DUT didn't discovered peer:%s device" % (ad2.name))


def find_p2p_group_owner(ad1, ad2):
    """Check an Android device ad1 can discover an Android device ad2 which
       is a group owner

    Args:
        ad1: The android device
        ad2: The android device which is a group owner
    """
    ad2.droid.wifiP2pStopPeerDiscovery()
    time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
    ad1.droid.wifiP2pDiscoverPeers()
    p2p_find_result = False
    while not p2p_find_result:
        ad1_event = ad1.ed.pop_event(p2pconsts.PEER_AVAILABLE_EVENT,
                                     p2pconsts.P2P_FIND_TIMEOUT)
        ad1.log.debug(ad1_event['data'])
        for device in ad1_event['data']['Peers']:
            if (device['Name'] == ad2.name and int(device['GroupCapability'])
                    & p2pconsts.P2P_GROUP_CAPAB_GROUP_OWNER):
                ad2.deviceAddress = device['Address']
                p2p_find_result = True
    asserts.assert_true(
        p2p_find_result,
        "DUT didn't discovered group owner peer:%s device" % (ad2.name))


def createP2pLocalService(ad, serviceCategory):
    """Based on serviceCategory to create p2p local service
            on an Android device ad

    Args:
        ad: The android device
        serviceCategory: p2p local service type, UPNP / IPP / AFP,
    """
    testData = genTestData(serviceCategory)
    if serviceCategory == p2pconsts.P2P_LOCAL_SERVICE_UPNP:
        ad.droid.wifiP2pCreateUpnpServiceInfo(testData[0], testData[1],
                                              testData[2])
    elif (serviceCategory == p2pconsts.P2P_LOCAL_SERVICE_IPP
          or serviceCategory == p2pconsts.P2P_LOCAL_SERVICE_AFP):
        ad.droid.wifiP2pCreateBonjourServiceInfo(testData[0], testData[1],
                                                 testData[2])
    ad.droid.wifiP2pAddLocalService()


def requestServiceAndCheckResult(ad_serviceProvider, ad_serviceReceiver,
                                 serviceType, queryString1, queryString2):
    """Based on serviceType and query info, check service request result
            same as expect or not on an Android device ad_serviceReceiver.
            And remove p2p service request after result check.

    Args:
        ad_serviceProvider: The android device which provide p2p local service
        ad_serviceReceiver: The android device which query p2p local service
        serviceType: P2p local service type, Upnp or Bonjour
        queryString1: Query String, NonNull
        queryString2: Query String, used for Bonjour, Nullable
    """
    expectData = genExpectTestData(serviceType, queryString1, queryString2)
    find_p2p_device(ad_serviceReceiver, ad_serviceProvider)
    ad_serviceReceiver.droid.wifiP2pStopPeerDiscovery()
    ad_serviceReceiver.droid.wifiP2pClearServiceRequests()
    time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    ad_serviceReceiver.droid.wifiP2pDiscoverServices()
    serviceData = {}
    service_id = 0
    if (serviceType ==
            WifiP2PEnums.WifiP2pServiceInfo.WIFI_P2P_SERVICE_TYPE_BONJOUR):
        ad_serviceReceiver.log.info(
            "Request bonjour service in \
                %s with Query String %s and %s " %
            (ad_serviceReceiver.name, queryString1, queryString2))
        ad_serviceReceiver.log.info("expectData %s" % expectData)
        if queryString1 != None:
            service_id = ad_serviceReceiver.droid.wifiP2pAddDnssdServiceRequest(
                queryString1, queryString2)
        else:
            service_id = ad_serviceReceiver.droid.wifiP2pAddServiceRequest(
                serviceType)
            ad_serviceReceiver.log.info("request bonjour service id %s" %
                                        service_id)
        ad_serviceReceiver.droid.wifiP2pSetDnsSdResponseListeners()
        ad_serviceReceiver.droid.wifiP2pDiscoverServices()
        ad_serviceReceiver.log.info("Check Service Listener")
        time.sleep(p2pconsts.DEFAULT_SERVICE_WAITING_TIME)
        try:
            dnssd_events = ad_serviceReceiver.ed.pop_all(p2pconsts.DNSSD_EVENT)
            dnssd_txrecord_events = ad_serviceReceiver.ed.pop_all(
                p2pconsts.DNSSD_TXRECORD_EVENT)
            dns_service = WifiP2PEnums.WifiP2pDnsSdServiceResponse()
            for dnssd_event in dnssd_events:
                if dnssd_event['data'][
                        'SourceDeviceAddress'] == ad_serviceProvider.deviceAddress:
                    dns_service.InstanceName = dnssd_event['data'][
                        p2pconsts.DNSSD_EVENT_INSTANCENAME_KEY]
                    dns_service.RegistrationType = dnssd_event['data'][
                        p2pconsts.DNSSD_EVENT_REGISTRATIONTYPE_KEY]
                    dns_service.FullDomainName = ""
                    dns_service.TxtRecordMap = ""
                    serviceData[dns_service.toString()] = 1
            for dnssd_txrecord_event in dnssd_txrecord_events:
                if dnssd_txrecord_event['data'][
                        'SourceDeviceAddress'] == ad_serviceProvider.deviceAddress:
                    dns_service.InstanceName = ""
                    dns_service.RegistrationType = ""
                    dns_service.FullDomainName = dnssd_txrecord_event['data'][
                        p2pconsts.DNSSD_TXRECORD_EVENT_FULLDOMAINNAME_KEY]
                    dns_service.TxtRecordMap = dnssd_txrecord_event['data'][
                        p2pconsts.DNSSD_TXRECORD_EVENT_TXRECORDMAP_KEY]
                    serviceData[dns_service.toString()] = 1
            ad_serviceReceiver.log.info("serviceData %s" % serviceData)
            if len(serviceData) == 0:
                ad_serviceReceiver.droid.wifiP2pRemoveServiceRequest(
                    service_id)
                return -1
        except queue.Empty as error:
            ad_serviceReceiver.log.info("dnssd event is empty", )
    elif (serviceType ==
          WifiP2PEnums.WifiP2pServiceInfo.WIFI_P2P_SERVICE_TYPE_UPNP):
        ad_serviceReceiver.log.info(
            "Request upnp service in %s with Query String %s " %
            (ad_serviceReceiver.name, queryString1))
        ad_serviceReceiver.log.info("expectData %s" % expectData)
        if queryString1 != None:
            service_id = ad_serviceReceiver.droid.wifiP2pAddUpnpServiceRequest(
                queryString1)
        else:
            service_id = ad_serviceReceiver.droid.wifiP2pAddServiceRequest(
                WifiP2PEnums.WifiP2pServiceInfo.WIFI_P2P_SERVICE_TYPE_UPNP)
        ad_serviceReceiver.droid.wifiP2pSetUpnpResponseListeners()
        ad_serviceReceiver.droid.wifiP2pDiscoverServices()
        ad_serviceReceiver.log.info("Check Service Listener")
        time.sleep(p2pconsts.DEFAULT_SERVICE_WAITING_TIME)
        try:
            upnp_events = ad_serviceReceiver.ed.pop_all(p2pconsts.UPNP_EVENT)
            for upnp_event in upnp_events:
                if upnp_event['data']['Device'][
                        'Address'] == ad_serviceProvider.deviceAddress:
                    for service in upnp_event['data'][
                            p2pconsts.UPNP_EVENT_SERVICELIST_KEY]:
                        serviceData[service] = 1
            ad_serviceReceiver.log.info("serviceData %s" % serviceData)
            if len(serviceData) == 0:
                ad_serviceReceiver.droid.wifiP2pRemoveServiceRequest(
                    service_id)
                return -1
        except queue.Empty as error:
            ad_serviceReceiver.log.info("p2p upnp event is empty", )

    ad_serviceReceiver.log.info("Check ServiceList")
    asserts.assert_true(checkServiceQueryResult(serviceData, expectData),
                        "ServiceList not same as Expect")
    # After service checked, remove the service_id
    ad_serviceReceiver.droid.wifiP2pRemoveServiceRequest(service_id)
    return 0


def requestServiceAndCheckResultWithRetry(ad_serviceProvider,
                                          ad_serviceReceiver,
                                          serviceType,
                                          queryString1,
                                          queryString2,
                                          retryCount=3):
    """ allow failures for requestServiceAndCheckResult. Service
        discovery might fail unexpectedly because the request packet might not be
        recevied by the service responder due to p2p state switch.

    Args:
        ad_serviceProvider: The android device which provide p2p local service
        ad_serviceReceiver: The android device which query p2p local service
        serviceType: P2p local service type, Upnp or Bonjour
        queryString1: Query String, NonNull
        queryString2: Query String, used for Bonjour, Nullable
        retryCount: maximum retry count, default is 3
    """
    ret = 0
    while retryCount > 0:
        ret = requestServiceAndCheckResult(ad_serviceProvider,
                                           ad_serviceReceiver, serviceType,
                                           queryString1, queryString2)
        if (ret == 0):
            break
        retryCount -= 1

    asserts.assert_equal(0, ret, "cannot find any services with retries.")


def checkServiceQueryResult(serviceList, expectServiceList):
    """Check serviceList same as expectServiceList or not

    Args:
        serviceList: ServiceList which get from query result
        expectServiceList: ServiceList which hardcode in genExpectTestData
    Return:
        True: serviceList  same as expectServiceList
        False:Exist discrepancy between serviceList and expectServiceList
    """
    tempServiceList = serviceList.copy()
    tempExpectServiceList = expectServiceList.copy()
    for service in serviceList.keys():
        if service in expectServiceList:
            del tempServiceList[service]
            del tempExpectServiceList[service]
    return len(tempExpectServiceList) == 0 and len(tempServiceList) == 0


def genTestData(serviceCategory):
    """Based on serviceCategory to generator Test Data

    Args:
        serviceCategory: P2p local service type, Upnp or Bonjour
    Return:
        TestData
    """
    testData = []
    if serviceCategory == p2pconsts.P2P_LOCAL_SERVICE_UPNP:
        testData.append(p2pconsts.UpnpTestData.uuid)
        testData.append(p2pconsts.UpnpTestData.serviceType)
        testData.append([
            p2pconsts.UpnpTestData.AVTransport,
            p2pconsts.UpnpTestData.ConnectionManager
        ])
    elif serviceCategory == p2pconsts.P2P_LOCAL_SERVICE_IPP:
        testData.append(p2pconsts.IppTestData.ippInstanceName)
        testData.append(p2pconsts.IppTestData.ippRegistrationType)
        testData.append(p2pconsts.IppTestData.ipp_txtRecord)
    elif serviceCategory == p2pconsts.P2P_LOCAL_SERVICE_AFP:
        testData.append(p2pconsts.AfpTestData.afpInstanceName)
        testData.append(p2pconsts.AfpTestData.afpRegistrationType)
        testData.append(p2pconsts.AfpTestData.afp_txtRecord)

    return testData


def genExpectTestData(serviceType, queryString1, queryString2):
    """Based on serviceCategory to generator expect serviceList

    Args:
        serviceType: P2p local service type, Upnp or Bonjour
        queryString1: Query String, NonNull
        queryString2: Query String, used for Bonjour, Nullable
    Return:
        expectServiceList
    """
    expectServiceList = {}
    if (serviceType ==
            WifiP2PEnums.WifiP2pServiceInfo.WIFI_P2P_SERVICE_TYPE_BONJOUR):
        ipp_service = WifiP2PEnums.WifiP2pDnsSdServiceResponse()
        afp_service = WifiP2PEnums.WifiP2pDnsSdServiceResponse()
        if queryString1 == p2pconsts.IppTestData.ippRegistrationType:
            if queryString2 == p2pconsts.IppTestData.ippInstanceName:
                ipp_service.InstanceName = ""
                ipp_service.RegistrationType = ""
                ipp_service.FullDomainName = p2pconsts.IppTestData.ippDomainName
                ipp_service.TxtRecordMap = p2pconsts.IppTestData.ipp_txtRecord
                expectServiceList[ipp_service.toString()] = 1
                return expectServiceList
            ipp_service.InstanceName = p2pconsts.IppTestData.ippInstanceName
            ipp_service.RegistrationType = (
                p2pconsts.IppTestData.ippRegistrationType + ".local.")
            ipp_service.FullDomainName = ""
            ipp_service.TxtRecordMap = ""
            expectServiceList[ipp_service.toString()] = 1
            return expectServiceList
        elif queryString1 == p2pconsts.AfpTestData.afpRegistrationType:
            if queryString2 == p2pconsts.AfpTestData.afpInstanceName:
                afp_service.InstanceName = ""
                afp_service.RegistrationType = ""
                afp_service.FullDomainName = p2pconsts.AfpTestData.afpDomainName
                afp_service.TxtRecordMap = p2pconsts.AfpTestData.afp_txtRecord
                expectServiceList[afp_service.toString()] = 1
                return expectServiceList
        ipp_service.InstanceName = p2pconsts.IppTestData.ippInstanceName
        ipp_service.RegistrationType = (
            p2pconsts.IppTestData.ippRegistrationType + ".local.")
        ipp_service.FullDomainName = ""
        ipp_service.TxtRecordMap = ""
        expectServiceList[ipp_service.toString()] = 1

        ipp_service.InstanceName = ""
        ipp_service.RegistrationType = ""
        ipp_service.FullDomainName = p2pconsts.IppTestData.ippDomainName
        ipp_service.TxtRecordMap = p2pconsts.IppTestData.ipp_txtRecord
        expectServiceList[ipp_service.toString()] = 1

        afp_service.InstanceName = p2pconsts.AfpTestData.afpInstanceName
        afp_service.RegistrationType = (
            p2pconsts.AfpTestData.afpRegistrationType + ".local.")
        afp_service.FullDomainName = ""
        afp_service.TxtRecordMap = ""
        expectServiceList[afp_service.toString()] = 1

        afp_service.InstanceName = ""
        afp_service.RegistrationType = ""
        afp_service.FullDomainName = p2pconsts.AfpTestData.afpDomainName
        afp_service.TxtRecordMap = p2pconsts.AfpTestData.afp_txtRecord
        expectServiceList[afp_service.toString()] = 1

        return expectServiceList
    elif serviceType == WifiP2PEnums.WifiP2pServiceInfo.WIFI_P2P_SERVICE_TYPE_UPNP:
        upnp_service = "uuid:" + p2pconsts.UpnpTestData.uuid + "::" + (
            p2pconsts.UpnpTestData.rootdevice)
        expectServiceList[upnp_service] = 1
        if queryString1 != "upnp:rootdevice":
            upnp_service = "uuid:" + p2pconsts.UpnpTestData.uuid + (
                "::" + p2pconsts.UpnpTestData.AVTransport)
            expectServiceList[upnp_service] = 1
            upnp_service = "uuid:" + p2pconsts.UpnpTestData.uuid + (
                "::" + p2pconsts.UpnpTestData.ConnectionManager)
            expectServiceList[upnp_service] = 1
            upnp_service = "uuid:" + p2pconsts.UpnpTestData.uuid + (
                "::" + p2pconsts.UpnpTestData.serviceType)
            expectServiceList[upnp_service] = 1
            upnp_service = "uuid:" + p2pconsts.UpnpTestData.uuid
            expectServiceList[upnp_service] = 1

    return expectServiceList


def p2p_create_group(ad):
    """Create a group as Group Owner

    Args:
        ad: The android device
    """
    ad.droid.wifiP2pCreateGroup()
    ad.ed.pop_event(p2pconsts.CREATE_GROUP_SUCCESS_EVENT,
                    p2pconsts.DEFAULT_TIMEOUT)
    time.sleep(p2pconsts.DEFAULT_SLEEPTIME)


def p2p_create_group_with_config(ad, network_name, passphrase, band):
    """Create a group as Group Owner

    Args:
        ad: The android device
    """
    wifi_p2p_config = {
        WifiP2PEnums.WifiP2pConfig.NETWORK_NAME: network_name,
        WifiP2PEnums.WifiP2pConfig.PASSPHRASE: passphrase,
        WifiP2PEnums.WifiP2pConfig.GROUP_BAND: band,
        WifiP2PEnums.WifiP2pConfig.WPSINFO_KEY: {
            WifiP2PEnums.WpsInfo.WPS_SETUP_KEY:
            WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC
        }
    }
    ad.droid.wifiP2pCreateGroupWithConfig(wifi_p2p_config)
    ad.ed.pop_event(p2pconsts.CREATE_GROUP_SUCCESS_EVENT,
                    p2pconsts.DEFAULT_TIMEOUT)
    time.sleep(p2pconsts.DEFAULT_SLEEPTIME)


def wifi_p2p_set_channels_for_current_group(ad, listening_chan,
                                            operating_chan):
    """Sets the listening channel and operating channel of the current group
       created with initialize.

    Args:
        ad: The android device
        listening_chan: Integer, the listening channel
        operating_chan: Integer, the operating channel
    """
    ad.droid.wifiP2pSetChannelsForCurrentGroup(listening_chan, operating_chan)
    ad.ed.pop_event(p2pconsts.SET_CHANNEL_SUCCESS_EVENT,
                    p2pconsts.DEFAULT_TIMEOUT)


class WifiP2PEnums():
    class WifiP2pConfig():
        DEVICEADDRESS_KEY = "deviceAddress"
        WPSINFO_KEY = "wpsInfo"
        GO_INTENT_KEY = "groupOwnerIntent"
        NETID_KEY = "netId"
        NETWORK_NAME = "networkName"
        PASSPHRASE = "passphrase"
        GROUP_BAND = "groupOwnerBand"

    class WpsInfo():
        WPS_SETUP_KEY = "setup"
        BSSID_KEY = "BSSID"
        WPS_PIN_KEY = "pin"
        #TODO: remove it from wifi_test_utils.py
        WIFI_WPS_INFO_PBC = 0
        WIFI_WPS_INFO_DISPLAY = 1
        WIFI_WPS_INFO_KEYPAD = 2
        WIFI_WPS_INFO_LABEL = 3
        WIFI_WPS_INFO_INVALID = 4

    class WifiP2pServiceInfo():
        #TODO: remove it from wifi_test_utils.py
        # Macros for wifi p2p.
        WIFI_P2P_SERVICE_TYPE_ALL = 0
        WIFI_P2P_SERVICE_TYPE_BONJOUR = 1
        WIFI_P2P_SERVICE_TYPE_UPNP = 2
        WIFI_P2P_SERVICE_TYPE_VENDOR_SPECIFIC = 255

    class WifiP2pDnsSdServiceResponse():
        def __init__(self):
            pass

        InstanceName = ""
        RegistrationType = ""
        FullDomainName = ""
        TxtRecordMap = {}

        def toString(self):
            return self.InstanceName + self.RegistrationType + (
                self.FullDomainName + str(self.TxtRecordMap))
