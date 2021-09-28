#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

"""
GATT server dictionaries which will be setup in various tests.
"""

from acts.test_utils.bt.bt_constants import gatt_characteristic
from acts.test_utils.bt.bt_constants import gatt_descriptor
from acts.test_utils.bt.bt_constants import gatt_service_types
from acts.test_utils.bt.bt_constants import gatt_char_types
from acts.test_utils.bt.bt_constants import gatt_characteristic_value_format
from acts.test_utils.bt.bt_constants import gatt_char_desc_uuids


SINGLE_PRIMARY_SERVICE = {
    'services': [{
        'uuid': '00001802-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
    }]
}

SINGLE_SECONDARY_SERVICE = {
    'services': [{
        'uuid': '00001802-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['secondary'],
    }]
}

PRIMARY_AND_SECONDARY_SERVICES = {
    'services': [{
        'uuid': '00001802-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
    }, {
        'uuid': '00001803-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['secondary'],
    }]
}

DUPLICATE_SERVICES = {
    'services': [{
        'uuid': '00001802-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
    }, {
        'uuid': '00001802-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
    }]
}

### Begin SIG defined services ###

# TODO: Reconcile all the proper security parameters of each service.
# Some are correct, others are not.

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.alert_notification.xml
ALERT_NOTIFICATION_SERVICE = {
    'services': [{
        'uuid': '00001811-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a47-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a46-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a48-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        }, {
            'uuid': '00002a45-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a44-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.automation_io.xml
AUTOMATION_IO_SERVICE = {
    'services': [{
        'uuid': '00001815-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a56-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }, {
                'uuid': '00002904-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_ext_props']
            }, {
                'uuid': '0000290a-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290e-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '00002909-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'],
            }]
        }, {
            'uuid': '00002a58-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'] |
            gatt_characteristic['write_type_signed'] |
            gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }, {
                'uuid': '00002904-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_ext_props']
            }, {
                'uuid': '0000290a-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290e-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '00002909-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'],
            }, {
                'uuid': '00002906-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'],
            }]
        }, {
            'uuid': '00002a5a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.battery_service.xml
BATTERY_SERVICE = {
    'services': [{
        'uuid': '0000180f-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a19-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }, {
                'uuid': '00002904-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.blood_pressure.xml
BLOOD_PRESSURE_SERVICE = {
    'services': [{
        'uuid': '00001810-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a35-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a36-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }],
        }, {
            'uuid': '00002a49-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.body_composition.xml
BODY_COMPOSITION_SERVICE = {
    'services': [{
        'uuid': '0000181b-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a9b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a9c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }],
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.bond_management.xml
BOND_MANAGEMENT_SERVICE = {
    'services': [{
        'uuid': '0000181e-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002aac-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test'
        }, {
            'uuid': '00002aa4-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }

        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.continuous_glucose_monitoring.xml
CONTINUOUS_GLUCOSE_MONITORING_SERVICE = {
    'services': [{
        'uuid': '0000180f-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002aa7-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002aa7-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002aa8-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002aa9-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002aaa-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002aab-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a52-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002aac-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.current_time.xml
CURRENT_TIME_SERVICE = {
    'services': [{
        'uuid': '00001805-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a2b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a0f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }],
        }, {
            'uuid': '00002a14-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.cycling_power.xml
CYCLING_POWER_SERVICE = {
    'services': [{
        'uuid': '00001818-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a63-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'] |
            gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg'],
            }, {
                'uuid': gatt_char_desc_uuids['server_char_cfg'],
            }]
        }, {
            'uuid': '00002a65-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a5d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a64-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a66-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.cycling_speed_and_cadence.xml
CYCLING_SPEED_AND_CADENCE_SERVICE = {
    'services': [{
        'uuid': '00001816-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a5b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg'],
            }]
        }, {
            'uuid': '00002a5c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a5d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a55-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.device_information.xml
DEVICE_INFORMATION_SERVICE = {
    'services': [{
        'uuid': '00001816-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a29-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a24-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a25-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a27-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a26-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a28-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a23-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a2a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a50-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.environmental_sensing.xml
ENVIRONMENTAL_SENSING_SERVICE = {
    'services': [{
        'uuid': '0000181a-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a7d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'] |
            gatt_characteristic['property_extended_props'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a73-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        }, {
            'uuid': '00002a72-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        }, {
            'uuid': '00002a7b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a6c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a74-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a7a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a6f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a77-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a75-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a78-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a6d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a6e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a71-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a76-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a79-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002aa3-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002a2c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002aa0-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },
        {
            'uuid': '00002aa1-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'] |
            gatt_characteristic['property_extended_props'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': '0000290c-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290d-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': '0000290b-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_user_desc'],
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            },
            ]
        },

        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.fitness_machine.xml
FITNESS_MACHINE_SERVICE = {
    'services': [{
        'uuid': '00001826-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002acc-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test'
        }, {
            'uuid': '00002acd-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ace-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002acf-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ad0-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ad1-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ad2-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ad3-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ad4-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test'
        }, {
            'uuid': '00002ad5-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test'
        }, {
            'uuid': '00002ad6-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002ad8-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test'
        }, {
            'uuid': '00002ad7-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test'
        }, {
            'uuid': '00002ad9-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002ada-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }


        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.glucose.xml
GLUCOSE_SERVICE = {
    'services': [{
        'uuid': '00001808-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a18-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg'],
            }]
        }, {
            'uuid': '00002a34-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a51-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a52-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.health_thermometer.xml
HEALTH_THERMOMETER_SERVICE = {
    'services': [{
        'uuid': '00001809-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a1c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg'],
            }]
        }, {
            'uuid': '00002a1d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a1e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a21-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'] |
            gatt_characteristic['property_write'] |
            gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }, {
                'uuid': gatt_char_desc_uuids['char_valid_range'],
                'permissions': gatt_descriptor['permission_read'],
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.heart_rate.xml
HEART_RATE_SERVICE = {
    'services': [{
        'uuid': '0000180d-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a37-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a38-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
        }, {
            'uuid': '00002a39-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.http_proxy.xml
HTTP_PROXY_SERVICE = {
    'services': [{
        'uuid': '00001823-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002ab6-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['property_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002ab7-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
        }, {
            'uuid': '00002ab9-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 8,
        }, {
            'uuid': '00002aba-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 8,
        }, {
            'uuid': '00002ab8-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 2,
        }, {
            'uuid': '00002abb-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        },
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.human_interface_device.xml
HUMAN_INTERFACE_DEVICE_SERVICE = {
    'services': [{
        'uuid': '00001812-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a4e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write_no_response'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a4d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }, {
                'uuid': '00002908-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'] |
                gatt_descriptor['permission_write'],
            }]
        }, {
            'uuid': '00002a4b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': '00002907-0000-1000-8000-00805f9b34fb',
                'permissions': gatt_descriptor['permission_read'],
            }]
        }, {
            'uuid': '00002a22-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        },{
            'uuid': '00002a32-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_read'] |
            gatt_characteristic['property_write_no_response'],
            'permissions': gatt_characteristic['permission_write'] |
            gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        }, {
            'uuid': '00002a33-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a4a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        }, {
            'uuid': '00002a4c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.immediate_alert.xml
IMMEDIATE_ALERT_SERVICE = {
    'services': [{
        'uuid': '0000180d-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a06-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.indoor_positioning.xml
INDOOR_POSITIONING_SERVICE = {
    'services': [{
        'uuid': '00001821-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a06-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a38-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002aad-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002aae-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002aaf-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002ab0-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002ab1-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002ab2-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002ab3-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002ab4-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }, {
            'uuid': '00002ab5-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'] |
            gatt_characteristic['property_read'] | gatt_characteristic['property_broadcast'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write_signed_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['server_char_cfg']
            }]
        }
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.insulin_delivery.xml
INSULIN_DELIVERY_SERVICE = {
    'services': [{
        'uuid': '0000183a-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002b20-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b21-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b22-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b23-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1
        }, {
            'uuid': '00002b24-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b25-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b26-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b27-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b28-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.internet_protocol_support.xml
INTERNET_PROTOCOL_SUPPORT_SERVICE = {
    'services': [{
        'uuid': '00001820-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.link_loss.xml
LINK_LOSS_SERVICE = {
    'services': [{
        'uuid': '00001803-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a06-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'] |
            gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.location_and_navigation.xml
LOCATION_AND_NAVIGATION_SERVICE = {
    'services': [{
        'uuid': '00001819-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a6a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a67-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'body',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a69-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
        }, {
            'uuid': '00002a6b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_write'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a68-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        },
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.mesh_provisioning.xml
MESH_PROVISIONING_SERVICE = {
    'services': [{
        'uuid': '00001827-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002adb-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002adc-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        },
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.mesh_proxy.xml
MESH_PROXY_SERVICE = {
    'services': [{
        'uuid': '00001828-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002add-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002ade-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        },
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.next_dst_change.xml
NEXT_DST_CHANGE_SERVICE = {
    'services': [{
        'uuid': '00001807-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a11-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint32'],
            'value': 1549903904,
        }
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.object_transfer.xml
OBJECT_TRANSFER_SERVICE = {
    'services': [{
        'uuid': '00001825-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002abd-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002abe-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002abf-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac0-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac1-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac2-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac3-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac4-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac5-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac6-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac7-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_read'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'] |
            gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }, {
            'uuid': '00002ac8-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 0,
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.phone_alert_status.xml
PHONE_ALERT_STATUS_SERVICE = {
    'services': [{
        'uuid': '0000180e-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a3f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a41-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a40-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
        },
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.pulse_oximeter.xml
PULSE_OXIMETER_SERVICE = {
    'services': [{
        'uuid': '00001822-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a5e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a5f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a60-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
        }, {
            'uuid': '00002a52-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.reconnection_configuration.xml
RECONNECTION_CONFIGURATION_SERVICE = {
    'services': [{
        'uuid': '00001829-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002b1d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002b1e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'] |
            gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002b1f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.reference_time_update.xml
REFERENCE_TIME_UPDATE_SERVICE = {
    'services': [{
        'uuid': '00001806-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a16-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
        }, {
            'uuid': '00002a17-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.running_speed_and_cadence.xml
RUNNING_SPEED_AND_CADENCE_SERVICE = {
    'services': [{
        'uuid': '00001814-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a53-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['string'],
            'value': 'test',
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a54-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
        }, {
            'uuid': '00002a5d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
        }, {
            'uuid': '00002a55-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write'] |
            gatt_characteristic['property_indicate'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.scan_parameters.xml
SCAN_PARAMETERS_SERVICE = {
    'services': [{
        'uuid': '00001813-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a4f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_write_no_response'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
        }, {
            'uuid': '00002a31-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_notify'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.transport_discovery.xml
TRANSPORT_DISCOVERY_SERVICE = {
    'services': [{
        'uuid': '00001824-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002abc-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'] |
            gatt_characteristic['property_write'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }
        ]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.tx_power.xml
TX_POWER_SERVICE = {
    'services': [{
        'uuid': '00001804-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a07-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['uint8'],
            'value': -24,
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.user_data.xml
USER_DATA_SERVICE = {
    'services': [{
        'uuid': '0000181c-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a8a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a90-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a87-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a80-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a85-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a8c-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a98-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a8e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a96-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a92-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a91-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a7f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a83-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a93-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a86-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a97-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a8f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a88-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a89-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a7e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a84-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a81-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a82-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a8b-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a94-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a95-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a99-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'] |
            gatt_characteristic['property_notify'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002a9a-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }, {
            'uuid': '00002a9f-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }, {
            'uuid': '00002aa2-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'] |
            gatt_characteristic['property_write'],
            'permissions': gatt_characteristic['permission_read_encrypted_mitm'] |
            gatt_characteristic['permission_write_encrypted_mitm'],
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 10,
        }]
    }]
}

# https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.weight_scale.xml
WEIGHT_SCALE_SERVICE = {
    'services': [{
        'uuid': '0000181d-0000-1000-8000-00805f9b34fb',
        'type': gatt_service_types['primary'],
        'characteristics': [{
            'uuid': '00002a9e-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_read'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 1
        }, {
            'uuid': '00002a9d-0000-1000-8000-00805f9b34fb',
            'properties': gatt_characteristic['property_indicate'],
            'permissions': 0x0,
            'value_type': gatt_characteristic_value_format['sint8'],
            'value': 100,
            'descriptors': [{
                'uuid': gatt_char_desc_uuids['client_char_cfg']
            }]
        }
        ]
    }]
}


### End SIG defined services ###