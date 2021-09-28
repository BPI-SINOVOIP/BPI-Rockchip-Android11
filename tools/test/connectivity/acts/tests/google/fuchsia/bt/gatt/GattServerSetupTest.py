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
This test exercises basic setup of various GATT server configurations.

Setup:
This test only requires one fuchsia device as the purpose is to test
different configurations of valid GATT services.
"""

from acts import signals
from acts.base_test import BaseTestClass
from acts.test_decorators import test_tracker_info

import gatt_server_databases as database


class GattServerSetupTest(BaseTestClass):
    err_message = "Setting up database failed with: {}"

    def setup_class(self):
        super().setup_class()
        self.fuchsia_dut = self.fuchsia_devices[0]

    def setup_database(self, database):
        setup_result = self.fuchsia_dut.gatts_lib.publishServer(database)
        if setup_result.get("error") is None:
            signals.TestPass(setup_result.get("result"))
        else:
            raise signals.TestFailure(
                self.err_message.format(setup_result.get("error")))

    def test_teardown(self):
        self.fuchsia_dut.gatts_lib.closeServer()

    @test_tracker_info(uuid='25f3463b-b6bd-408b-9924-f18ed3b9bbe2')
    def test_single_primary_service(self):
        """Test GATT Server Setup: Single Primary Service

        Test a single primary service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.SINGLE_PRIMARY_SERVICE)

    @test_tracker_info(uuid='084d2c91-ca8c-4572-8e9b-053495acd894')
    def test_single_secondary_service(self):
        """Test GATT Server Setup: Single Secondary Service

        Test a single secondary service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.SINGLE_SECONDARY_SERVICE)

    @test_tracker_info(uuid='9ff0a91d-3b17-4e11-b5d6-b91997c79a04')
    def test_primary_and_secondary_service(self):
        """Test GATT Server Setup: Primary and secondary service

        Test primary and secondary service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.PRIMARY_AND_SECONDARY_SERVICES)

    @test_tracker_info(uuid='4f57e827-5511-4d5d-9591-c247d84d6d6c')
    def test_duplicate_services(self):
        """Test GATT Server Setup: Duplicate service uuids

        Test duplicate service uuids as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.DUPLICATE_SERVICES)

    ### Begin SIG defined services ###

    @test_tracker_info(uuid='6b0993f5-18ff-4a20-be3d-d76529eb1126')
    def test_alert_notification_service(self):
        """Test GATT Server Setup: Alert Notification Service

        Test Alert Notification Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """       
        self.setup_database(database.ALERT_NOTIFICATION_SERVICE)

    @test_tracker_info(uuid='c42e8bc9-1ba7-4d4e-b67e-9c19cc11472c')
    def test_automation_io_service(self):
        """Test GATT Server Setup: Automation IO

        Test Automation IO as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.AUTOMATION_IO_SERVICE)

    @test_tracker_info(uuid='68936f97-4a4d-4bfb-9826-558846a0a53a')
    def test_battery_service(self):
        """Test GATT Server Setup: Battery Service

        Test Battery Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.BATTERY_SERVICE)

    @test_tracker_info(uuid='678864a1-edcf-4335-85b4-1494d00c62be')
    def test_blood_pressure_service(self):
        """Test GATT Server Setup: Blood Pressure

        Test Blood Pressure as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.BLOOD_PRESSURE_SERVICE)

    @test_tracker_info(uuid='1077d373-7d9b-4a43-acde-7d20bb0fa4e4')
    def test_body_composition_service(self):
        """Test GATT Server Setup: Body Composition

        Test Body Composition as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.BODY_COMPOSITION_SERVICE)

    @test_tracker_info(uuid='67a82719-7dcb-4d11-b9a4-a95c05f6dda6')
    def test_bond_management_service(self):
        """Test GATT Server Setup: Bond Management Service

        Test Bond Management Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.BOND_MANAGEMENT_SERVICE)

    @test_tracker_info(uuid='c0edd532-00f1-4afc-85f2-114477d81a0a')
    def test_continuous_glucose_monitoring_service(self):
        """Test GATT Server Setup: Continuous Glucose Monitoring

        Test Continuous Glucose Monitoring as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.CONTINUOUS_GLUCOSE_MONITORING_SERVICE)

    @test_tracker_info(uuid='23b3faeb-1423-4687-81bc-ff9d71bc1c1c')
    def test_current_time_service(self):
        """Test GATT Server Setup: Current Time Service

        Test Current Time Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.CURRENT_TIME_SERVICE)

    @test_tracker_info(uuid='7cc592d8-69e6-459e-b4ae-fe0b1be77491')
    def test_cycling_power_service(self):
        """Test GATT Server Setup: Cycling Power

        Test Cycling Power as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.CYCLING_POWER_SERVICE)

    @test_tracker_info(uuid='3134ee93-7e19-4bd5-ab09-9bcb4a8f6b7d')
    def test_cycling_speed_and_cadence_service(self):
        """Test GATT Server Setup: Cycling Speed and Cadence

        Test Cycling Speed and Cadence as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.CYCLING_SPEED_AND_CADENCE_SERVICE)

    @test_tracker_info(uuid='c090443e-c4cf-42de-ae79-a60dae50cad4')
    def test_device_information_service(self):
        """Test GATT Server Setup: Device Information

        Test Device Information as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.DEVICE_INFORMATION_SERVICE)

    @test_tracker_info(uuid='bd696344-34a4-4665-857f-fafbd2d3c849')
    def test_environmental_sensing_service(self):
        """Test GATT Server Setup: Environmental Sensing

        Test Environmental Sensing as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.ENVIRONMENTAL_SENSING_SERVICE)

    @test_tracker_info(uuid='d567c23b-276e-43c9-9862-94d0d36887c2')
    def test_fitness_machine_service(self):
        """Test GATT Server Setup: Fitness Machine

        Test Fitness Machine as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.FITNESS_MACHINE_SERVICE)

    @test_tracker_info(uuid='47084eb2-b99f-4f8d-b943-04148b8a71ca')
    def test_glucose_service(self):
        """Test GATT Server Setup: Glucose

        Test Glucose as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.GLUCOSE_SERVICE)

    @test_tracker_info(uuid='d64ce217-13f1-4cdd-965a-03d06349ac53')
    def test_health_thermometer_service(self):
        """Test GATT Server Setup: Health Thermometer

        Test Health Thermometer as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.HEALTH_THERMOMETER_SERVICE)

    @test_tracker_info(uuid='2510d536-0464-4e25-a2c1-f2acc989e7ef')
    def test_heart_rate_service(self):
        """Test GATT Server Setup: Heart Rate

        Test Heart Rate as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.HEART_RATE_SERVICE)

    @test_tracker_info(uuid='cc3ca7f2-d783-426d-a62e-86a56cafec96')
    def test_http_proxy_service(self):
        """Test GATT Server Setup: HTTP Proxy

        Test HTTP Proxy as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.HTTP_PROXY_SERVICE)

    @test_tracker_info(uuid='4e265f85-12aa-43e3-9560-b9f2fb015116')
    def test_human_interface_device_service(self):
        """Test GATT Server Setup: Human Interface Device

    Test Human Interface Device as a GATT server input.

    Steps:
    1. Setup input database

    Expected Result:
    Verify that there are no errors after setting up the input database.

    Returns:
      signals.TestPass if no errors
      signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.HUMAN_INTERFACE_DEVICE_SERVICE)

    @test_tracker_info(uuid='3e092fb8-fe5f-42a9-ac1d-9951e4ddebe5')
    def test_immediate_alert_service(self):
        """Test GATT Server Setup: Immediate Alert

        Test Immediate Alert as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.IMMEDIATE_ALERT_SERVICE)

    @test_tracker_info(uuid='c6eb782a-4999-460d-9ebe-d4a050002242')
    def test_indoor_positioning_service(self):
        """Test GATT Server Setup: Indoor Positioning

        Test Indoor Positioning as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.INDOOR_POSITIONING_SERVICE)

    @test_tracker_info(uuid='96ac4b99-2232-4efd-8039-d62e8f98f0ef')
    def test_insulin_delivery_service(self):
        """Test GATT Server Setup: Insulin Delivery

        Test Insulin Delivery as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.INSULIN_DELIVERY_SERVICE)

    @test_tracker_info(uuid='28a0f54e-906d-4c9a-800f-238ce1a3829a')
    def test_internet_protocol_support_service(self):
        """Test GATT Server Setup: Internet Protocol Support Service

        Test Internet Protocol Support Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.INTERNET_PROTOCOL_SUPPORT_SERVICE)

    @test_tracker_info(uuid='3d0452ce-5cfc-4312-97c3-e7060e2efce6')
    def test_link_loss_service(self):
        """Test GATT Server Setup: Link Loss

        Test Link Loss as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.LINK_LOSS_SERVICE)

    @test_tracker_info(uuid='79a375e9-1f6c-498e-a7f8-63a2e2fdb03b')
    def test_location_and_navigation_service(self):
        """Test GATT Server Setup: Location and Navigation

        Test Location and Navigation as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.LOCATION_AND_NAVIGATION_SERVICE)

    @test_tracker_info(uuid='4517a844-e6b3-4f28-91f1-989d7affd190')
    def test_mesh_provisioning_service(self):
        """Test GATT Server Setup: Mesh Provisioning Service

        Test Mesh Provisioning Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.MESH_PROVISIONING_SERVICE)

    @test_tracker_info(uuid='960fdf95-4e55-4c88-86ce-a6d31dd094fa')
    def test_mesh_proxy_service(self):
        """Test GATT Server Setup: Mesh Proxy Service

        Test Mesh Proxy Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.MESH_PROXY_SERVICE)

    @test_tracker_info(uuid='c97f933a-2200-46ae-a161-9c133fcc2c67')
    def test_next_dst_change_service(self):
        """Test GATT Server Setup: Next DST Change Service

        Test Next DST Change Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.NEXT_DST_CHANGE_SERVICE)

    @test_tracker_info(uuid='90b2caa1-f0fa-4afa-8771-924234deae17')
    def test_object_transfer_service(self):
        """Test GATT Server Setup: Object Transfer Service

        Test Object Transfer Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.OBJECT_TRANSFER_SERVICE)

    @test_tracker_info(uuid='28ac2e34-1d0d-4c29-95f3-92e845dc65a2')
    def test_phone_alert_status_service(self):
        """Test GATT Server Setup: Phone Alert Status Service

        Test Phone Alert Status Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.PHONE_ALERT_STATUS_SERVICE)

    @test_tracker_info(uuid='fb19e36f-0a25-48af-8787-dae2b213bf5b')
    def test_pulse_oximeter_service(self):
        """Test GATT Server Setup: Pulse Oximeter Service

        Test Pulse Oximeter Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.PULSE_OXIMETER_SERVICE)

    @test_tracker_info(uuid='5e59820a-db03-4acd-96a4-1e3fc0bf7f53')
    def test_reconnection_configuration_service(self):
        """Test GATT Server Setup: Reconnection Configuration

        Test Reconnection Configuration as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.RECONNECTION_CONFIGURATION_SERVICE)

    @test_tracker_info(uuid='7b2525c9-178a-44db-aa3b-f3cf5ceed132')
    def test_reference_time_update_service(self):
        """Test GATT Server Setup: Reference Time Update Service

        Test Reference Time Update Service as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.REFERENCE_TIME_UPDATE_SERVICE)

    @test_tracker_info(uuid='71d657db-4519-4158-b6d2-a744f2cc2c22')
    def test_running_speed_and_cadence_service(self):
        """Test GATT Server Setup: Running Speed and Cadence

        Test Running Speed and Cadence as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.RUNNING_SPEED_AND_CADENCE_SERVICE)

    @test_tracker_info(uuid='4d3bd69a-978a-4483-9a2d-f1aff308a845')
    def test_scan_parameters_service(self):
        """Test GATT Server Setup: Scan Parameters

        Test Scan Parameters as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.SCAN_PARAMETERS_SERVICE)

    @test_tracker_info(uuid='2c6f95a1-a353-4147-abb6-714bc3aacbb4')
    def test_transport_discovery_service(self):
        """Test GATT Server Setup: Transport Discovery

        Test Transport Discovery as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.TRANSPORT_DISCOVERY_SERVICE)

    @test_tracker_info(uuid='3e36f2ba-5590-4201-897e-7321c8e44f4c')
    def test_tx_power_service(self):
        """Test GATT Server Setup: Tx Power

        Test Tx Power as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.TX_POWER_SERVICE)

    @test_tracker_info(uuid='c62d70f8-e474-4c88-9960-f2ce2200852e')
    def test_user_data_service(self):
        """Test GATT Server Setup: User Data

        Test User Data as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.USER_DATA_SERVICE)

    @test_tracker_info(uuid='18951e43-f587-4395-93e3-b440dc4b3285')
    def test_weight_scale_service(self):
        """Test GATT Server Setup: Weight Scale

        Test Weight Scale as a GATT server input.

        Steps:
        1. Setup input database

        Expected Result:
        Verify that there are no errors after setting up the input database.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during setup

        TAGS: GATT
        Priority: 1
        """
        self.setup_database(database.WEIGHT_SCALE_SERVICE)

    ### End SIG defined services ###
