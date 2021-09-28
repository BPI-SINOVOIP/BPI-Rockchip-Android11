# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Server side bluetooth GATT client helper class for testing"""

import base64
import json


class GATT_ClientFacade(object):
    """A wrapper for getting GATT application from GATT server"""

    def __init__(self, bluetooth_facade):
        """Initialize a GATT_ClientFacade

        @param bluetooth_facade: facade to communicate with adapter in DUT

        """
        self.bluetooth_facade = bluetooth_facade


    def browse(self, address):
        """Browse the application on GATT server

        @param address: a string of MAC address of the GATT server device

        @return: GATT_Application object

        """
        attr_map_json = json.loads(self.bluetooth_facade.\
                              get_gatt_attributes_map(address))
        application = GATT_Application()
        application.browse(attr_map_json, self.bluetooth_facade)

        return application


class GATT_Application(object):
    """A GATT client application class"""

    def __init__(self):
        """Initialize a GATT Application"""
        self.services = dict()


    def browse(self, attr_map_json, bluetooth_facade):
        """Browse the application on GATT server

        @param attr_map_json: a json object returned by
                              bluetooth_device_xmlrpc_server

        @bluetooth_facade: facade to communicate with adapter in DUT

        """
        servs_json = attr_map_json['services']
        for uuid in servs_json:
            path = servs_json[uuid]['path']
            service_obj = GATT_Service(uuid, path, bluetooth_facade)
            service_obj.read_properties()
            self.add_service(service_obj)

            chrcs_json = servs_json[uuid]['characteristics']
            for uuid in chrcs_json:
                path = chrcs_json[uuid]['path']
                chrc_obj = GATT_Characteristic(uuid, path, bluetooth_facade)
                chrc_obj.read_properties()
                service_obj.add_characteristic(chrc_obj)

                descs_json = chrcs_json[uuid]['descriptors']
                for uuid in descs_json:
                    path = descs_json[uuid]['path']
                    desc_obj = GATT_Descriptor(uuid, path, bluetooth_facade)
                    desc_obj.read_properties()
                    chrc_obj.add_descriptor(desc_obj)


    def find_by_uuid(self, uuid):
        """Find attribute under this application by specifying UUID

        @param uuid: string of UUID

        @return: Attribute object if found,
                 none otherwise
        """
        for serv_uuid, serv in self.services.items():
            found = serv.find_by_uuid(uuid)
            if found:
                return found
        return None


    def add_service(self, service):
        """Add a service into this application"""
        self.services[service.uuid] = service


    @staticmethod
    def diff(appl_a, appl_b):
        """Compare two Applications, and return their difference

        @param appl_a: the first application which is going to be compared

        @param appl_b: the second application which is going to be compared

        @return: a list of string, each describes one difference

        """
        result = []

        uuids_a = set(appl_a.services.keys())
        uuids_b = set(appl_b.services.keys())
        uuids = uuids_a.union(uuids_b)

        for uuid in uuids:
            serv_a = appl_a.services.get(uuid, None)
            serv_b = appl_b.services.get(uuid, None)

            if not serv_a or not serv_b:
                result.append("Service %s is not included in both Applications:"
                              "%s vs %s" % (uuid, bool(serv_a), bool(serv_b)))
            else:
                result.extend(GATT_Service.diff(serv_a, serv_b))
        return result


class GATT_Service(object):
    """GATT client service class"""
    PROPERTIES = ['UUID', 'Primary', 'Device', 'Includes']


    def __init__(self, uuid, object_path, bluetooth_facade):
        """Initialize a GATT service object

        @param uuid: string of UUID

        @param object_path: object path of this service

        @param bluetooth_facade: facade to communicate with adapter in DUT

        """
        self.uuid = uuid
        self.object_path = object_path
        self.bluetooth_facade = bluetooth_facade
        self.properties = dict()
        self.characteristics = dict()


    def add_characteristic(self, chrc_obj):
        """Add a characteristic attribute into service

        @param chrc_obj: a characteristic object

        """
        self.characteristics[chrc_obj.uuid] = chrc_obj


    def read_properties(self):
        """Read all properties in this service"""
        for prop_name in self.PROPERTIES:
            self.properties[prop_name] = self.read_property(prop_name)
        return self.properties


    def read_property(self, property_name):
        """Read a property in this service

        @param property_name: string of the name of the property

        @return: the value of the property

        """
        return self.bluetooth_facade.get_gatt_service_property(
                                        self.object_path, property_name)

    def find_by_uuid(self, uuid):
        """Find attribute under this service by specifying UUID

        @param uuid: string of UUID

        @return: Attribute object if found,
                 none otherwise

        """
        if self.uuid == uuid:
            return self

        for chrc_uuid, chrc in self.characteristics.items():
            found = chrc.find_by_uuid(uuid)
            if found:
                return found
        return None


    @staticmethod
    def diff(serv_a, serv_b):
        """Compare two Services, and return their difference

        @param serv_a: the first service which is going to be compared

        @param serv_b: the second service which is going to be compared

        @return: a list of string, each describes one difference

        """
        result = []

        for prop_name in GATT_Service.PROPERTIES:
            if serv_a.properties[prop_name] != serv_b.properties[prop_name]:
                result.append("Service %s is different in %s: %s vs %s" %
                              (serv_a.uuid, prop_name,
                              serv_a.properties[prop_name],
                              serv_b.properties[prop_name]))

        uuids_a = set(serv_a.characteristics.keys())
        uuids_b = set(serv_b.characteristics.keys())
        uuids = uuids_a.union(uuids_b)

        for uuid in uuids:
            chrc_a = serv_a.characteristics.get(uuid, None)
            chrc_b = serv_b.characteristics.get(uuid, None)

            if not chrc_a or not chrc_b:
                result.append("Characteristic %s is not included in both "
                              "Services: %s vs %s" % (uuid, bool(chrc_a),
                                                    bool(chrc_b)))
            else:
                result.extend(GATT_Characteristic.diff(chrc_a, chrc_b))
        return result


class GATT_Characteristic(object):
    """GATT client characteristic class"""

    PROPERTIES = ['UUID', 'Service', 'Value', 'Notifying', 'Flags']


    def __init__(self, uuid, object_path, bluetooth_facade):
        """Initialize a GATT characteristic object

        @param uuid: string of UUID

        @param object_path: object path of this characteristic

        @param bluetooth_facade: facade to communicate with adapter in DUT

        """
        self.uuid = uuid
        self.object_path = object_path
        self.bluetooth_facade = bluetooth_facade
        self.properties = dict()
        self.descriptors = dict()


    def add_descriptor(self, desc_obj):
        """Add a characteristic attribute into service

        @param desc_obj: a descriptor object

        """
        self.descriptors[desc_obj.uuid] = desc_obj


    def read_properties(self):
        """Read all properties in this characteristic"""
        for prop_name in self.PROPERTIES:
            self.properties[prop_name] = self.read_property(prop_name)
        return self.properties


    def read_property(self, property_name):
        """Read a property in this characteristic

        @param property_name: string of the name of the property

        @return: the value of the property

        """
        return self.bluetooth_facade.get_gatt_characteristic_property(
                                        self.object_path, property_name)


    def find_by_uuid(self, uuid):
        """Find attribute under this characteristic by specifying UUID

        @param uuid: string of UUID

        @return: Attribute object if found,
                 none otherwise

        """
        if self.uuid == uuid:
            return self

        for desc_uuid, desc in self.descriptors.items():
            if desc_uuid == uuid:
                return desc
        return None


    def read_value(self):
        """Perform ReadValue in DUT and store it in property 'Value'

        @return: bytearray of the value

        """
        value = self.bluetooth_facade.gatt_characteristic_read_value(
                                                self.uuid, self.object_path)
        self.properties['Value'] = bytearray(base64.standard_b64decode(value))
        return self.properties['Value']


    @staticmethod
    def diff(chrc_a, chrc_b):
        """Compare two Characteristics, and return their difference

        @param serv_a: the first service which is going to be compared

        @param serv_b: the second service which is going to be compared

        @return: a list of string, each describes one difference

        """
        result = []

        for prop_name in GATT_Characteristic.PROPERTIES:
            if chrc_a.properties[prop_name] != chrc_b.properties[prop_name]:
                result.append("Characteristic %s is different in %s: %s vs %s"
                              % (chrc_a.uuid, prop_name,
                              chrc_a.properties[prop_name],
                              chrc_b.properties[prop_name]))

        uuids_a = set(chrc_a.descriptors.keys())
        uuids_b = set(chrc_b.descriptors.keys())
        uuids = uuids_a.union(uuids_b)

        for uuid in uuids:
            desc_a = chrc_a.descriptors.get(uuid, None)
            desc_b = chrc_b.descriptors.get(uuid, None)

            if not desc_a or not desc_b:
                result.append("Descriptor %s is not included in both"
                              "Characteristic: %s vs %s" % (uuid, bool(desc_a),
                                                          bool(desc_b)))
            else:
                result.extend(GATT_Descriptor.diff(desc_a, desc_b))
        return result


class GATT_Descriptor(object):
    """GATT client descriptor class"""

    PROPERTIES = ['UUID', 'Characteristic', 'Value', 'Flags']

    def __init__(self, uuid, object_path, bluetooth_facade):
        """Initialize a GATT descriptor object

        @param uuid: string of UUID

        @param object_path: object path of this descriptor

        @param bluetooth_facade: facade to communicate with adapter in DUT

        """
        self.uuid = uuid
        self.object_path = object_path
        self.bluetooth_facade = bluetooth_facade
        self.properties = dict()


    def read_properties(self):
        """Read all properties in this characteristic"""
        for prop_name in self.PROPERTIES:
            self.properties[prop_name] = self.read_property(prop_name)
        return self.properties


    def read_property(self, property_name):
        """Read a property in this characteristic

        @param property_name: string of the name of the property

        @return: the value of the property

        """
        return self.bluetooth_facade.get_gatt_descriptor_property(
                                        self.object_path, property_name)


    def read_value(self):
        """Perform ReadValue in DUT and store it in property 'Value'

        @return: bytearray of the value

        """
        value = self.bluetooth_facade.gatt_descriptor_read_value(
                                                self.uuid, self.object_path)
        self.properties['Value'] = bytearray(base64.standard_b64decode(value))

        return self.properties['Value']


    @staticmethod
    def diff(desc_a, desc_b):
        """Compare two Descriptors, and return their difference

        @param serv_a: the first service which is going to be compared

        @param serv_b: the second service which is going to be compared

        @return: a list of string, each describes one difference

        """
        result = []

        for prop_name in desc_a.properties.keys():
            if desc_a.properties[prop_name] != desc_b.properties[prop_name]:
                result.append("Descriptor %s is different in %s: %s vs %s" %
                              (desc_a.uuid, prop_name,
                              desc_a.properties[prop_name],
                              desc_b.properties[prop_name]))

        return result


def UUID_Short2Full(uuid):
    """Transform 2 bytes uuid string to 16 bytes

    @param uuid: 2 bytes shortened UUID string in hex

    @return: full uuid string
    """
    uuid_template = '0000%s-0000-1000-8000-00805f9b34fb'
    return uuid_template % uuid


class GATT_HIDApplication(GATT_Application):
    """Default HID Application on Raspberry Pi GATT server
    """

    BatteryServiceUUID = UUID_Short2Full('180f')
    BatteryLevelUUID = UUID_Short2Full('2a19')
    CliChrcConfigUUID = UUID_Short2Full('2902')
    GenericAttributeProfileUUID = UUID_Short2Full('1801')
    ServiceChangedUUID = UUID_Short2Full('2a05')
    DeviceInfoUUID = UUID_Short2Full('180a')
    ManufacturerNameStrUUID = UUID_Short2Full('2a29')
    PnPIDUUID = UUID_Short2Full('2a50')
    GenericAccessProfileUUID = UUID_Short2Full('1800')
    DeviceNameUUID = UUID_Short2Full('2a00')
    AppearanceUUID = UUID_Short2Full('2a01')


    def __init__(self):
        """
        """
        GATT_Application.__init__(self)
        BatteryService = GATT_Service(self.BatteryServiceUUID, None, None)
        BatteryService.properties = {
            'UUID': BatteryService.uuid,
            'Primary': True,
            'Device': None,
            'Includes': None
        }
        self.add_service(BatteryService)

        BatteryLevel = GATT_Characteristic(self.BatteryLevelUUID, None, None)
        BatteryLevel.properties = {
            'UUID': BatteryLevel.uuid,
            'Service': None,
            'Value': [],
            'Notifying': False,
            'Flags': ['read', 'notify']
        }
        BatteryService.add_characteristic(BatteryLevel)

        CliChrcConfig = GATT_Descriptor(self.CliChrcConfigUUID, None, None)
        CliChrcConfig.properties = {
            'UUID': CliChrcConfig.uuid,
            'Characteristic': None,
            'Value': [],
            'Flags': None
        }

        BatteryLevel.add_descriptor(CliChrcConfig)

        GenericAttributeProfile = GATT_Service(self.GenericAttributeProfileUUID,
                                               None, None)
        GenericAttributeProfile.properties = {
            'UUID': GenericAttributeProfile.uuid,
            'Primary': True,
            'Device': None,
            'Includes': None
        }
        self.add_service(GenericAttributeProfile)

        ServiceChanged = GATT_Characteristic(self.ServiceChangedUUID, None,
                                             None)
        ServiceChanged.properties = {
            'UUID': ServiceChanged.uuid,
            'Service': None,
            'Value': [],
            'Notifying': False,
            'Flags': ['indicate']
        }
        GenericAttributeProfile.add_characteristic(ServiceChanged)

        CliChrcConfig = GATT_Descriptor(self.CliChrcConfigUUID, None, None)
        CliChrcConfig.properties = {
            'UUID': CliChrcConfig.uuid,
            'Characteristic': None,
            'Value': [],
            'Flags': None
        }
        ServiceChanged.add_descriptor(CliChrcConfig)

        DeviceInfo = GATT_Service(self.DeviceInfoUUID, None, None)
        DeviceInfo.properties = {
            'UUID': DeviceInfo.uuid,
            'Primary': True,
            'Device': None,
            'Includes': None
        }
        self.add_service(DeviceInfo)

        ManufacturerNameStr = GATT_Characteristic(self.ManufacturerNameStrUUID,
                                                  None, None)
        ManufacturerNameStr.properties = {
            'UUID': ManufacturerNameStr.uuid,
            'Service': None,
            'Value': [],
            'Notifying': None,
            'Flags': ['read']
        }
        DeviceInfo.add_characteristic(ManufacturerNameStr)

        PnPID = GATT_Characteristic(self.PnPIDUUID, None, None)
        PnPID.properties = {
            'UUID': PnPID.uuid,
            'Service': None,
            'Value': [],
            'Notifying': None,
            'Flags': ['read']
        }
        DeviceInfo.add_characteristic(PnPID)

        GenericAccessProfile = GATT_Service(self.GenericAccessProfileUUID,
                                            None, None)
        GenericAccessProfile.properties = {
            'UUID': GenericAccessProfile.uuid,
            'Primary': True,
            'Device': None,
            'Includes': None
        }
        self.add_service(GenericAccessProfile)

        DeviceName = GATT_Characteristic(self.DeviceNameUUID, None, None)
        DeviceName.properties = {
            'UUID': DeviceName.uuid,
            'Service': None,
            'Value': [],
            'Notifying': None,
            'Flags': ['read']
        }
        GenericAccessProfile.add_characteristic(DeviceName)

        Appearance = GATT_Characteristic(self.AppearanceUUID, None, None)
        Appearance.properties = {
            'UUID': Appearance.uuid,
            'Service': None,
            'Value': [],
            'Notifying': None,
            'Flags': ['read']
        }
        GenericAccessProfile.add_characteristic(Appearance)
