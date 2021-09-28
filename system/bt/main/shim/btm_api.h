/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "stack/include/btm_api_types.h"
#include "stack/include/btm_ble_api_types.h"

namespace bluetooth {
namespace shim {

/*******************************************************************************
 *
 * Function         BTM_StartInquiry
 *
 * Description      This function is called to start an inquiry.
 *
 * Parameters:      p_inqparms - pointer to the inquiry information
 *                      mode - GENERAL or LIMITED inquiry
 *                      duration - length in 1.28 sec intervals (If '0', the
 *                                 inquiry is CANCELLED)
 *                      max_resps - maximum amount of devices to search for
 *                                  before ending the inquiry
 *                      filter_cond_type - BTM_CLR_INQUIRY_FILTER,
 *                                         BTM_FILTER_COND_DEVICE_CLASS, or
 *                                         BTM_FILTER_COND_BD_ADDR
 *                      filter_cond - value for the filter (based on
 *                                                          filter_cond_type)
 *
 *                  p_results_cb  - Pointer to the callback routine which gets
 *                                called upon receipt of an inquiry result. If
 *                                this field is NULL, the application is not
 *                                notified.
 *
 *                  p_cmpl_cb   - Pointer to the callback routine which gets
 *                                called upon completion.  If this field is
 *                                NULL, the application is not notified when
 *                                completed.
 * Returns          tBTM_STATUS
 *                  BTM_CMD_STARTED if successfully initiated
 *                  BTM_BUSY if already in progress
 *                  BTM_ILLEGAL_VALUE if parameter(s) are out of range
 *                  BTM_NO_RESOURCES if could not allocate resources to start
 *                                   the command
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_StartInquiry(tBTM_INQ_PARMS* p_inqparms,
                             tBTM_INQ_RESULTS_CB* p_results_cb,
                             tBTM_CMPL_CB* p_cmpl_cb);

/*******************************************************************************
 *
 * Function         BTM_SetDiscoverability
 *
 * Description      This function is called to set the device into or out of
 *                  discoverable mode. Discoverable mode means inquiry
 *                  scans are enabled.  If a value of '0' is entered for window
 *                  or interval, the default values are used.
 *
 * Returns          BTM_SUCCESS if successful
 *                  BTM_BUSY if a setting of the filter is already in progress
 *                  BTM_NO_RESOURCES if couldn't get a memory pool buffer
 *                  BTM_ILLEGAL_VALUE if a bad parameter was detected
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetDiscoverability(uint16_t inq_mode, uint16_t window,
                                   uint16_t interval);

/*******************************************************************************
 *
 * Function         BTM_SetInquiryScanType
 *
 * Description      This function is called to set the iquiry scan-type to
 *                  standard or interlaced.
 *
 * Input Params:    BTM_SCAN_TYPE_STANDARD or BTM_SCAN_TYPE_INTERLACED
 *
 * Returns          BTM_SUCCESS if successful
 *                  BTM_MODE_UNSUPPORTED if not a 1.2 device
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetInquiryScanType(uint16_t scan_type);

/*******************************************************************************
 *
 * Function         BTM_BleObserve
 *
 * Description      This procedure keep the device listening for advertising
 *                  events from a broadcast device.
 *
 * Parameters       start: start or stop observe.
 *
 * Returns          void
 *
 ******************************************************************************/
tBTM_STATUS BTM_BleObserve(bool start, uint8_t duration,
                           tBTM_INQ_RESULTS_CB* p_results_cb,
                           tBTM_CMPL_CB* p_cmpl_cb);

/*******************************************************************************
 *
 * Function         BTM_SetPageScanType
 *
 * Description      This function is called to set the page scan-type to
 *                  standard or interlaced.
 *
 * Input Params:    BTM_SCAN_TYPE_STANDARD or BTM_SCAN_TYPE_INTERLACED
 *
 * Returns          BTM_SUCCESS if successful
 *                  BTM_MODE_UNSUPPORTED if not a 1.2 device
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetPageScanType(uint16_t scan_type);

/*******************************************************************************
 *
 * Function         BTM_SetInquiryMode
 *
 * Description      This function is called to set standard, with RSSI
 *                  mode or extended of the inquiry for local device.
 *
 * Input Params:    BTM_INQ_RESULT_STANDARD, BTM_INQ_RESULT_WITH_RSSI or
 *                  BTM_INQ_RESULT_EXTENDED
 *
 * Returns          BTM_SUCCESS if successful
 *                  BTM_NO_RESOURCES if couldn't get a memory pool buffer
 *                  BTM_ILLEGAL_VALUE if a bad parameter was detected
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetInquiryMode(uint8_t mode);

/*******************************************************************************
 *
 * Function         BTM_ReadDiscoverability
 *
 * Description      This function is called to read the current discoverability
 *                  mode of the device.
 *
 * Output Params:   p_window - current inquiry scan duration
 *                  p_interval - current inquiry scan interval
 *
 * Returns          BTM_NON_DISCOVERABLE, BTM_LIMITED_DISCOVERABLE, or
 *                  BTM_GENERAL_DISCOVERABLE
 *
 ******************************************************************************/
uint16_t BTM_ReadDiscoverability(uint16_t* p_window, uint16_t* p_interval);

/*******************************************************************************
 *
 * Function         BTM_CancelPeriodicInquiry
 *
 * Description      This function cancels a periodic inquiry
 *
 * Returns
 *                  BTM_NO_RESOURCES if could not allocate a message buffer
 *                  BTM_SUCCESS - if cancelling the periodic inquiry
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_CancelPeriodicInquiry(void);

/*******************************************************************************
 *
 * Function         BTM_SetConnectability
 *
 * Description      This function is called to set the device into or out of
 *                  connectable mode. Discoverable mode means page scans are
 *                  enabled.
 *
 * Returns          BTM_SUCCESS if successful
 *                  BTM_ILLEGAL_VALUE if a bad parameter is detected
 *                  BTM_NO_RESOURCES if could not allocate a message buffer
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetConnectability(uint16_t page_mode, uint16_t window,
                                  uint16_t interval);

/*******************************************************************************
 *
 * Function         BTM_ReadConnectability
 *
 * Description      This function is called to read the current discoverability
 *                  mode of the device.
 * Output Params    p_window - current page scan duration
 *                  p_interval - current time between page scans
 *
 * Returns          BTM_NON_CONNECTABLE or BTM_CONNECTABLE
 *
 ******************************************************************************/
uint16_t BTM_ReadConnectability(uint16_t* p_window, uint16_t* p_interval);

/*******************************************************************************
 *
 * Function         BTM_IsInquiryActive
 *
 * Description      Return a bit mask of the current inquiry state
 *
 * Returns          BTM_INQUIRY_INACTIVE if inactive (0)
 *                  BTM_LIMITED_INQUIRY_ACTIVE if a limted inquiry is active
 *                  BTM_GENERAL_INQUIRY_ACTIVE if a general inquiry is active
 *                  BTM_PERIODIC_INQUIRY_ACTIVE if a periodic inquiry is active
 *
 ******************************************************************************/
uint16_t BTM_IsInquiryActive(void);

/*******************************************************************************
 *
 * Function         BTM_CancelInquiry
 *
 * Description      This function cancels an inquiry if active
 *
 * Returns          BTM_SUCCESS if successful
 *                  BTM_NO_RESOURCES if could not allocate a message buffer
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_CancelInquiry(void);

/*******************************************************************************
 *
 * Function         BTM_ReadRemoteDeviceName
 *
 * Description      This function initiates a remote device HCI command to the
 *                  controller and calls the callback when the process has
 *                  completed.
 *
 * Input Params:    remote_bda      - device address of name to retrieve
 *                  p_cb            - callback function called when
 *                                    BTM_CMD_STARTED is returned.
 *                                    A pointer to tBTM_REMOTE_DEV_NAME is
 *                                    passed to the callback.
 *
 * Returns
 *                  BTM_CMD_STARTED is returned if the request was successfully
 *                                  sent to HCI.
 *                  BTM_BUSY if already in progress
 *                  BTM_UNKNOWN_ADDR if device address is bad
 *                  BTM_NO_RESOURCES if resources could not be allocated to
 *                                   start the command
 *                  BTM_WRONG_MODE if the device is not up.
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadRemoteDeviceName(const RawAddress& remote_bda,
                                     tBTM_CMPL_CB* p_cb,
                                     tBT_TRANSPORT transport);

/*******************************************************************************
 *
 * Function         BTM_CancelRemoteDeviceName
 *
 * Description      This function initiates the cancel request for the specified
 *                  remote device.
 *
 * Input Params:    None
 *
 * Returns
 *                  BTM_CMD_STARTED is returned if the request was successfully
 *                                  sent to HCI.
 *                  BTM_NO_RESOURCES if resources could not be allocated to
 *                                   start the command
 *                  BTM_WRONG_MODE if there is no active remote name request.
 *
 ******************************************************************************/
tBTM_STATUS BTM_CancelRemoteDeviceName(void);

/*******************************************************************************
 *
 * Function         BTM_InqDbRead
 *
 * Description      This function looks through the inquiry database for a match
 *                  based on Bluetooth Device Address. This is the application's
 *                  interface to get the inquiry details of a specific BD
 *                  address.
 *
 * Returns          pointer to entry, or NULL if not found
 *
 ******************************************************************************/
tBTM_INQ_INFO* BTM_InqDbRead(const RawAddress& p_bda);

/*******************************************************************************
 *
 * Function         BTM_InqDbFirst
 *
 * Description      This function looks through the inquiry database for the
 *                  first used entry, and returns that. This is used in
 *                  conjunction with BTM_InqDbNext by applications as a way to
 *                  walk through the inquiry database.
 *
 * Returns          pointer to first in-use entry, or NULL if DB is empty
 *
 ******************************************************************************/
tBTM_INQ_INFO* BTM_InqDbFirst(void);

/*******************************************************************************
 *
 * Function         BTM_InqDbNext
 *
 * Description      This function looks through the inquiry database for the
 *                  next used entry, and returns that.  If the input parameter
 *                  is NULL, the first entry is returned.
 *
 * Returns          pointer to next in-use entry, or NULL if no more found.
 *
 ******************************************************************************/
tBTM_INQ_INFO* BTM_InqDbNext(tBTM_INQ_INFO* p_cur);

/*******************************************************************************
 *
 * Function         BTM_ClearInqDb
 *
 * Description      This function is called to clear out a device or all devices
 *                  from the inquiry database.
 *
 * Parameter        p_bda - (input) BD_ADDR ->  Address of device to clear
 *                                              (NULL clears all entries)
 *
 * Returns          BTM_BUSY if an inquiry, get remote name, or event filter
 *                          is active, otherwise BTM_SUCCESS
 *
 ******************************************************************************/
tBTM_STATUS BTM_ClearInqDb(const RawAddress* p_bda);

/*******************************************************************************
 *
 * Function         BTM_WriteEIR
 *
 * Description      This function is called to write EIR data to controller.
 *
 * Parameters       p_buff - allocated HCI command buffer including extended
 *                           inquriry response
 *
 * Returns          BTM_SUCCESS  - if successful
 *                  BTM_MODE_UNSUPPORTED - if local device cannot support it
 *
 ******************************************************************************/
tBTM_STATUS BTM_WriteEIR(BT_HDR* p_buff);

/*******************************************************************************
 *
 * Function         BTM_HasEirService
 *
 * Description      This function is called to know if UUID in bit map of UUID.
 *
 * Parameters       p_eir_uuid - bit map of UUID list
 *                  uuid16 - UUID 16-bit
 *
 * Returns          true - if found
 *                  false - if not found
 *
 ******************************************************************************/
bool BTM_HasEirService(const uint32_t* p_eir_uuid, uint16_t uuid16);

/*******************************************************************************
 *
 * Function         BTM_HasInquiryEirService
 *
 * Description      Return if a UUID is in the bit map of a UUID list.
 *
 * Parameters       p_results - inquiry results
 *                  uuid16 - UUID 16-bit
 *
 * Returns          BTM_EIR_FOUND - if found
 *                  BTM_EIR_NOT_FOUND - if not found and it is a complete list
 *                  BTM_EIR_UNKNOWN - if not found and it is not complete list
 *
 ******************************************************************************/
tBTM_EIR_SEARCH_RESULT BTM_HasInquiryEirService(tBTM_INQ_RESULTS* p_results,
                                                uint16_t uuid16);

/*******************************************************************************
 *
 * Function         BTM_AddEirService
 *
 * Description      This function is called to add a service in the bit map UUID
 *                  list.
 *
 * Parameters       p_eir_uuid - bit mask of UUID list for EIR
 *                  uuid16 - UUID 16-bit
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_AddEirService(uint32_t* p_eir_uuid, uint16_t uuid16);

/*******************************************************************************
 *
 * Function         BTM_RemoveEirService
 *
 * Description      This function is called to remove a service from the bit map
 *                  UUID list.
 *
 * Parameters       p_eir_uuid - bit mask of UUID list for EIR
 *                  uuid16 - UUID 16-bit
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_RemoveEirService(uint32_t* p_eir_uuid, uint16_t uuid16);

/*******************************************************************************
 *
 * Function         BTM_GetEirSupportedServices
 *
 * Description      This function is called to get UUID list from bit map UUID
 *                  list.
 *
 * Parameters       p_eir_uuid - bit mask of UUID list for EIR
 *                  p - reference of current pointer of EIR
 *                  max_num_uuid16 - max number of UUID can be written in EIR
 *                  num_uuid16 - number of UUID have been written in EIR
 *
 * Returns          BTM_EIR_MORE_16BITS_UUID_TYPE, if it has more than max
 *                  BTM_EIR_COMPLETE_16BITS_UUID_TYPE, otherwise
 *
 ******************************************************************************/
uint8_t BTM_GetEirSupportedServices(uint32_t* p_eir_uuid, uint8_t** p,
                                    uint8_t max_num_uuid16,
                                    uint8_t* p_num_uuid16);

/*******************************************************************************
 *
 * Function         BTM_GetEirUuidList
 *
 * Description      This function parses EIR and returns UUID list.
 *
 * Parameters       p_eir - EIR
 *                  eirl_len - EIR len
 *                  uuid_size - Uuid::kNumBytes16, Uuid::kNumBytes32,
 *                              Uuid::kNumBytes128
 *                  p_num_uuid - return number of UUID in found list
 *                  p_uuid_list - return UUID 16-bit list
 *                  max_num_uuid - maximum number of UUID to be returned
 *
 * Returns          0 - if not found
 *                  BTM_EIR_COMPLETE_16BITS_UUID_TYPE
 *                  BTM_EIR_MORE_16BITS_UUID_TYPE
 *                  BTM_EIR_COMPLETE_32BITS_UUID_TYPE
 *                  BTM_EIR_MORE_32BITS_UUID_TYPE
 *                  BTM_EIR_COMPLETE_128BITS_UUID_TYPE
 *                  BTM_EIR_MORE_128BITS_UUID_TYPE
 *
 ******************************************************************************/
uint8_t BTM_GetEirUuidList(uint8_t* p_eir, size_t eir_len, uint8_t uuid_size,
                           uint8_t* p_num_uuid, uint8_t* p_uuid_list,
                           uint8_t max_num_uuid);

/*******************************************************************************
 *
 * Function         BTM_SecAddBleDevice
 *
 * Description      Add/modify device.  This function will be normally called
 *                  during host startup to restore all required information
 *                  for a LE device stored in the NVRAM.
 *
 * Parameters:      bd_addr          - BD address of the peer
 *                  bd_name          - Name of the peer device. NULL if unknown.
 *                  dev_type         - Remote device's device type.
 *                  addr_type        - LE device address type.
 *
 * Returns          true if added OK, else false
 *
 ******************************************************************************/
bool BTM_SecAddBleDevice(const RawAddress& bd_addr, BD_NAME bd_name,
                         tBT_DEVICE_TYPE dev_type, tBLE_ADDR_TYPE addr_type);

/*******************************************************************************
 *
 * Function         BTM_SecAddBleKey
 *
 * Description      Add/modify LE device information.  This function will be
 *                  normally called during host startup to restore all required
 *                  information stored in the NVRAM.
 *
 * Parameters:      bd_addr          - BD address of the peer
 *                  p_le_key         - LE key values.
 *                  key_type         - LE SMP key type.
 *
 * Returns          true if added OK, else false
 *
 ******************************************************************************/
bool BTM_SecAddBleKey(const RawAddress& bd_addr, tBTM_LE_KEY_VALUE* p_le_key,
                      tBTM_LE_KEY_TYPE key_type);

/*******************************************************************************
 *
 * Function         BTM_BleLoadLocalKeys
 *
 * Description      Local local identity key, encryption root or sign counter.
 *
 * Parameters:      key_type: type of key, can be BTM_BLE_KEY_TYPE_ID,
 *                            BTM_BLE_KEY_TYPE_ER
 *                            or BTM_BLE_KEY_TYPE_COUNTER.
 *                  p_key: pointer to the key.
 *
 * Returns          non2.
 *
 ******************************************************************************/
void BTM_BleLoadLocalKeys(uint8_t key_type, tBTM_BLE_LOCAL_KEYS* p_key);

/** Returns local device encryption root (ER) */
const Octet16& BTM_GetDeviceEncRoot();

/** Returns local device identity root (IR) */
const Octet16& BTM_GetDeviceIDRoot();

/** Return local device DHK. */
const Octet16& BTM_GetDeviceDHK();

/*******************************************************************************
 *
 * Function         BTM_ReadConnectionAddr
 *
 * Description      Read the local device random address.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_ReadConnectionAddr(const RawAddress& remote_bda,
                            RawAddress& local_conn_addr,
                            tBLE_ADDR_TYPE* p_addr_type);

/*******************************************************************************
 *
 * Function         BTM_IsBleConnection
 *
 * Description      This function is called to check if the connection handle
 *                  for an LE link
 *
 * Returns          true if connection is LE link, otherwise false.
 *
 ******************************************************************************/
bool BTM_IsBleConnection(uint16_t conn_handle);

/*******************************************************************************
 *
 * Function         BTM_ReadRemoteConnectionAddr
 *
 * Description      Read the remote device address currently used.
 *
 * Returns          void
 *
 ******************************************************************************/
bool BTM_ReadRemoteConnectionAddr(const RawAddress& pseudo_addr,
                                  RawAddress& conn_addr,
                                  tBLE_ADDR_TYPE* p_addr_type);

/*******************************************************************************
 *
 * Function         BTM_SecurityGrant
 *
 * Description      This function is called to grant security process.
 *
 * Parameters       bd_addr - peer device bd address.
 *                  res     - result of the operation BTM_SUCCESS if success.
 *                            Otherwise, BTM_REPEATED_ATTEMPTS is too many
 *                            attempts.
 *
 * Returns          None
 *
 ******************************************************************************/
void BTM_SecurityGrant(const RawAddress& bd_addr, uint8_t res);

/*******************************************************************************
 *
 * Function         BTM_BlePasskeyReply
 *
 * Description      This function is called after Security Manager submitted
 *                  passkey request to the application.
 *
 * Parameters:      bd_addr - Address of the device for which passkey was
 *                            requested
 *                  res     - result of the operation SMP_SUCCESS if success
 *                  passkey - numeric value in the range of
 *                               BTM_MIN_PASSKEY_VAL(0) -
 *                               BTM_MAX_PASSKEY_VAL(999999(0xF423F)).
 *
 ******************************************************************************/
void BTM_BlePasskeyReply(const RawAddress& bd_addr, uint8_t res,
                         uint32_t passkey);

/*******************************************************************************
 *
 * Function         BTM_BleConfirmReply
 *
 * Description      This function is called after Security Manager submitted
 *                  numeric comparison request to the application.
 *
 * Parameters:      bd_addr      - Address of the device with which numeric
 *                                 comparison was requested
 *                  res          - comparison result BTM_SUCCESS if success
 *
 ******************************************************************************/
void BTM_BleConfirmReply(const RawAddress& bd_addr, uint8_t res);

/*******************************************************************************
 *
 * Function         BTM_LeOobDataReply
 *
 * Description      This function is called to provide the OOB data for
 *                  SMP in response to BTM_LE_OOB_REQ_EVT
 *
 * Parameters:      bd_addr     - Address of the peer device
 *                  res         - result of the operation SMP_SUCCESS if success
 *                  p_data      - simple pairing Randomizer  C.
 *
 ******************************************************************************/
void BTM_BleOobDataReply(const RawAddress& bd_addr, uint8_t res, uint8_t len,
                         uint8_t* p_data);

/*******************************************************************************
 *
 * Function         BTM_BleSecureConnectionOobDataReply
 *
 * Description      This function is called to provide the OOB data for
 *                  SMP in response to BTM_LE_OOB_REQ_EVT when secure connection
 *                  data is available
 *
 * Parameters:      bd_addr     - Address of the peer device
 *                  p_c         - pointer to Confirmation
 *                  p_r         - pointer to Randomizer.
 *
 ******************************************************************************/
void BTM_BleSecureConnectionOobDataReply(const RawAddress& bd_addr,
                                         uint8_t* p_c, uint8_t* p_r);

/******************************************************************************
 *
 * Function         BTM_BleSetConnScanParams
 *
 * Description      Set scan parameters used in BLE connection request
 *
 * Parameters:      scan_interval    - scan interval
 *                  scan_window      - scan window
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleSetConnScanParams(uint32_t scan_interval, uint32_t scan_window);

/********************************************************
 *
 * Function         BTM_BleSetPrefConnParams
 *
 * Description      Set a peripheral's preferred connection parameters. When
 *                  any of the value does not want to be updated while others
 *                  do, use BTM_BLE_CONN_PARAM_UNDEF for the ones want to
 *                  leave untouched.
 *
 * Parameters:      bd_addr          - BD address of the peripheral
 *                  min_conn_int     - minimum preferred connection interval
 *                  max_conn_int     - maximum preferred connection interval
 *                  slave_latency    - preferred slave latency
 *                  supervision_tout - preferred supervision timeout
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleSetPrefConnParams(const RawAddress& bd_addr, uint16_t min_conn_int,
                              uint16_t max_conn_int, uint16_t slave_latency,
                              uint16_t supervision_tout);

/*******************************************************************************
 *
 * Function         BTM_ReadDevInfo
 *
 * Description      This function is called to read the device/address type
 *                  of BD address.
 *
 * Parameter        remote_bda: remote device address
 *                  p_dev_type: output parameter to read the device type.
 *                  p_addr_type: output parameter to read the address type.
 *
 ******************************************************************************/
void BTM_ReadDevInfo(const RawAddress& remote_bda, tBT_DEVICE_TYPE* p_dev_type,
                     tBLE_ADDR_TYPE* p_addr_type);

/*******************************************************************************
 *
 * Function         BTM_ReadConnectedTransportAddress
 *
 * Description      This function is called to read the paired device/address
 *                  type of other device paired corresponding to the BD_address
 *
 * Parameter        remote_bda: remote device address, carry out the transport
 *                              address
 *                  transport: active transport
 *
 * Return           true if an active link is identified; false otherwise
 *
 ******************************************************************************/
bool BTM_ReadConnectedTransportAddress(RawAddress* remote_bda,
                                       tBT_TRANSPORT transport);

/*******************************************************************************
 *
 * Function         BTM_BleReceiverTest
 *
 * Description      This function is called to start the LE Receiver test
 *
 * Parameter       rx_freq - Frequency Range
 *               p_cmd_cmpl_cback - Command Complete callback
 *
 ******************************************************************************/
void BTM_BleReceiverTest(uint8_t rx_freq, tBTM_CMPL_CB* p_cmd_cmpl_cback);

/*******************************************************************************
 *
 * Function         BTM_BleTransmitterTest
 *
 * Description      This function is called to start the LE Transmitter test
 *
 * Parameter       tx_freq - Frequency Range
 *                       test_data_len - Length in bytes of payload data in each
 *                                       packet
 *                       packet_payload - Pattern to use in the payload
 *                       p_cmd_cmpl_cback - Command Complete callback
 *
 ******************************************************************************/
void BTM_BleTransmitterTest(uint8_t tx_freq, uint8_t test_data_len,
                            uint8_t packet_payload,
                            tBTM_CMPL_CB* p_cmd_cmpl_cback);

/*******************************************************************************
 *
 * Function         BTM_BleTestEnd
 *
 * Description     This function is called to stop the in-progress TX or RX test
 *
 * Parameter       p_cmd_cmpl_cback - Command complete callback
 *
 ******************************************************************************/
void BTM_BleTestEnd(tBTM_CMPL_CB* p_cmd_cmpl_cback);

/*******************************************************************************
 *
 * Function         BTM_UseLeLink
 *
 * Description      Select the underlying physical link to use.
 *
 * Returns          true to use LE, false use BR/EDR.
 *
 ******************************************************************************/
bool BTM_UseLeLink(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_SetBleDataLength
 *
 * Description      Set the maximum BLE transmission packet size
 *
 * Returns          BTM_SUCCESS if success; otherwise failed.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetBleDataLength(const RawAddress& bd_addr,
                                 uint16_t tx_pdu_length);

/*******************************************************************************
 *
 * Function         BTM_BleReadPhy
 *
 * Description      To read the current PHYs for specified LE connection
 *
 *
 * Returns          BTM_SUCCESS if success; otherwise failed.
 *
 ******************************************************************************/
void BTM_BleReadPhy(
    const RawAddress& bd_addr,
    base::Callback<void(uint8_t tx_phy, uint8_t rx_phy, uint8_t status)> cb);

/*******************************************************************************
 *
 * Function         BTM_BleSetDefaultPhy
 *
 * Description      To set preferred PHY for ensuing LE connections
 *
 *
 * Returns          BTM_SUCCESS if success; otherwise failed.
 *
 ******************************************************************************/
tBTM_STATUS BTM_BleSetDefaultPhy(uint8_t all_phys, uint8_t tx_phys,
                                 uint8_t rx_phys);

/*******************************************************************************
 *
 * Function         BTM_BleSetPhy
 *
 * Description      To set PHY preferences for specified LE connection
 *
 *
 * Returns          BTM_SUCCESS if success; otherwise failed.
 *                  BTM_MODE_UNSUPPORTED if local controller doesn't support LE
 *                  2M or LE Coded PHY,
 *                  BTM_ILLEGAL_VALUE if specified remote doesn't support LE 2M
 *                  or LE Coded PHY,
 *                  BTM_WRONG_MODE if Device in wrong mode for request.
 *
 ******************************************************************************/
void BTM_BleSetPhy(const RawAddress& bd_addr, uint8_t tx_phys, uint8_t rx_phys,
                   uint16_t phy_options);

/*******************************************************************************
 *
 * Function         BTM_BleDataSignature
 *
 * Description      This function is called to sign the data using AES128 CMAC
 *                  algorith.
 *
 * Parameter        bd_addr: target device the data to be signed for.
 *                  p_text: singing data
 *                  len: length of the signing data
 *                  signature: output parameter where data signature is going to
 *                             be stored.
 *
 * Returns          true if signing sucessul, otherwise false.
 *
 ******************************************************************************/
bool BTM_BleDataSignature(const RawAddress& bd_addr, uint8_t* p_text,
                          uint16_t len, BLE_SIGNATURE signature);

/*******************************************************************************
 *
 * Function         BTM_BleVerifySignature
 *
 * Description      This function is called to verify the data signature
 *
 * Parameter        bd_addr: target device the data to be signed for.
 *                  p_orig:  original data before signature.
 *                  len: length of the signing data
 *                  counter: counter used when doing data signing
 *                  p_comp: signature to be compared against.

 * Returns          true if signature verified correctly; otherwise false.
 *
 ******************************************************************************/
bool BTM_BleVerifySignature(const RawAddress& bd_addr, uint8_t* p_orig,
                            uint16_t len, uint32_t counter, uint8_t* p_comp);

/*******************************************************************************
 *
 * Function         BTM_GetLeSecurityState
 *
 * Description      This function is called to get security mode 1 flags and
 *                  encryption key size for LE peer.
 *
 * Returns          bool    true if LE device is found, false otherwise.
 *
 ******************************************************************************/
bool BTM_GetLeSecurityState(const RawAddress& bd_addr,
                            uint8_t* p_le_dev_sec_flags,
                            uint8_t* p_le_key_size);

/*******************************************************************************
 *
 * Function         BTM_BleSecurityProcedureIsRunning
 *
 * Description      This function indicates if LE security procedure is
 *                  currently running with the peer.
 *
 * Returns          bool true if security procedure is running, false otherwise.
 *
 ******************************************************************************/
bool BTM_BleSecurityProcedureIsRunning(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_BleGetSupportedKeySize
 *
 * Description      This function gets the maximum encryption key size in bytes
 *                  the local device can suport.
 *                  record.
 *
 * Returns          the key size or 0 if the size can't be retrieved.
 *
 ******************************************************************************/
uint8_t BTM_BleGetSupportedKeySize(const RawAddress& bd_addr);

void BTM_LE_PF_local_name(tBTM_BLE_SCAN_COND_OP action,
                          tBTM_BLE_PF_FILT_INDEX filt_index,
                          std::vector<uint8_t> name, tBTM_BLE_PF_CFG_CBACK cb);

void BTM_LE_PF_srvc_data(tBTM_BLE_SCAN_COND_OP action,
                         tBTM_BLE_PF_FILT_INDEX filt_index);

void BTM_LE_PF_manu_data(tBTM_BLE_SCAN_COND_OP action,
                         tBTM_BLE_PF_FILT_INDEX filt_index, uint16_t company_id,
                         uint16_t company_id_mask, std::vector<uint8_t> data,
                         std::vector<uint8_t> data_mask,
                         tBTM_BLE_PF_CFG_CBACK cb);

void BTM_LE_PF_srvc_data_pattern(tBTM_BLE_SCAN_COND_OP action,
                                 tBTM_BLE_PF_FILT_INDEX filt_index,
                                 std::vector<uint8_t> data,
                                 std::vector<uint8_t> data_mask,
                                 tBTM_BLE_PF_CFG_CBACK cb);

void BTM_LE_PF_addr_filter(tBTM_BLE_SCAN_COND_OP action,
                           tBTM_BLE_PF_FILT_INDEX filt_index, tBLE_BD_ADDR addr,
                           tBTM_BLE_PF_CFG_CBACK cb);

void BTM_LE_PF_uuid_filter(tBTM_BLE_SCAN_COND_OP action,
                           tBTM_BLE_PF_FILT_INDEX filt_index,
                           tBTM_BLE_PF_COND_TYPE filter_type,
                           const bluetooth::Uuid& uuid,
                           tBTM_BLE_PF_LOGIC_TYPE cond_logic,
                           const bluetooth::Uuid& uuid_mask,
                           tBTM_BLE_PF_CFG_CBACK cb);

void BTM_LE_PF_set(tBTM_BLE_PF_FILT_INDEX filt_index,
                   std::vector<ApcfCommand> commands, tBTM_BLE_PF_CFG_CBACK cb);
void BTM_LE_PF_clear(tBTM_BLE_PF_FILT_INDEX filt_index,
                     tBTM_BLE_PF_CFG_CBACK cb);

/*******************************************************************************
 *
 * Function         BTM_BleAdvFilterParamSetup
 *
 * Description      This function is called to setup the adv data payload filter
 *                  condition.
 *
 ******************************************************************************/
void BTM_BleAdvFilterParamSetup(
    int action, tBTM_BLE_PF_FILT_INDEX filt_index,
    std::unique_ptr<btgatt_filt_param_setup_t> p_filt_params,
    tBTM_BLE_PF_PARAM_CB cb);

/*******************************************************************************
 *
 * Function         BTM_BleUpdateAdvFilterPolicy
 *
 * Description      This function update the filter policy of advertiser.
 *
 * Parameter        adv_policy: advertising filter policy
 *
 * Return           void
 ******************************************************************************/
void BTM_BleUpdateAdvFilterPolicy(tBTM_BLE_AFP adv_policy);

/*******************************************************************************
 *
 * Function         BTM_BleEnableDisableFilterFeature
 *
 * Description      Enable or disable the APCF feature
 *
 * Parameters       enable - true - enables APCF, false - disables APCF
 *
 ******************************************************************************/
void BTM_BleEnableDisableFilterFeature(uint8_t enable,
                                       tBTM_BLE_PF_STATUS_CBACK p_stat_cback);

/*******************************************************************************
 *
 * Function          BTM_BleMaxMultiAdvInstanceCount
 *
 * Description      Returns the maximum number of multi adv instances supported
 *                  by the controller.
 *
 * Returns          Max multi adv instance count
 *
 ******************************************************************************/
uint8_t BTM_BleMaxMultiAdvInstanceCount();

/*******************************************************************************
 *
 * Function         BTM_DeviceReset
 *
 * Description      This function is called to reset the controller.  The
 *                  Callback function if provided is called when startup of the
 *                  device has completed.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_DeviceReset(tBTM_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_IsDeviceUp
 *
 * Description      This function is called to check if the device is up.
 *
 * Returns          true if device is up, else false
 *
 ******************************************************************************/
bool BTM_IsDeviceUp(void);

/*******************************************************************************
 *
 * Function         BTM_SetLocalDeviceName
 *
 * Description      This function is called to set the local device name.
 *
 * Returns          BTM_CMD_STARTED if successful, otherwise an error
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetLocalDeviceName(char* p_name);

/*******************************************************************************
 *
 * Function         BTM_SetDeviceClass
 *
 * Description      This function is called to set the local device class
 *
 * Returns          BTM_SUCCESS if successful, otherwise an error
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetDeviceClass(DEV_CLASS dev_class);

/*******************************************************************************
 *
 * Function         BTM_ReadLocalDeviceName
 *
 * Description      This function is called to read the local device name.
 *
 * Returns          status of the operation
 *                  If success, BTM_SUCCESS is returned and p_name points stored
 *                              local device name
 *                  If BTM doesn't store local device name, BTM_NO_RESOURCES is
 *                              is returned and p_name is set to NULL
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadLocalDeviceName(char** p_name);

/*******************************************************************************
 *
 * Function         BTM_ReadLocalDeviceNameFromController
 *
 * Description      Get local device name from controller. Do not use cached
 *                  name (used to get chip-id prior to btm reset complete).
 *
 * Returns          BTM_CMD_STARTED if successful, otherwise an error
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadLocalDeviceNameFromController(
    tBTM_CMPL_CB* p_rln_cmpl_cback);

/*******************************************************************************
 *
 * Function         BTM_ReadDeviceClass
 *
 * Description      This function is called to read the local device class
 *
 * Returns          pointer to the device class
 *
 ******************************************************************************/
uint8_t* BTM_ReadDeviceClass(void);

/*******************************************************************************
 *
 * Function         BTM_ReadLocalFeatures
 *
 * Description      This function is called to read the local features
 *
 * Returns          pointer to the local features string
 *
 ******************************************************************************/
uint8_t* BTM_ReadLocalFeatures(void);

/*******************************************************************************
 *
 * Function         BTM_RegisterForDeviceStatusNotif
 *
 * Description      This function is called to register for device status
 *                  change notifications.
 *
 * Returns          pointer to previous caller's callback function or NULL if
 *                  first registration.
 *
 ******************************************************************************/
tBTM_DEV_STATUS_CB* BTM_RegisterForDeviceStatusNotif(tBTM_DEV_STATUS_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_RegisterForVSEvents
 *
 * Description      This function is called to register/deregister for vendor
 *                  specific HCI events.
 *
 *                  If is_register=true, then the function will be registered;
 *                  otherwise the function will be deregistered.
 *
 * Returns          BTM_SUCCESS if successful,
 *                  BTM_BUSY if maximum number of callbacks have already been
 *                           registered.
 *
 ******************************************************************************/
tBTM_STATUS BTM_RegisterForVSEvents(tBTM_VS_EVT_CB* p_cb, bool is_register);

/*******************************************************************************
 *
 * Function         BTM_VendorSpecificCommand
 *
 * Description      Send a vendor specific HCI command to the controller.
 *
 ******************************************************************************/
void BTM_VendorSpecificCommand(uint16_t opcode, uint8_t param_len,
                               uint8_t* p_param_buf, tBTM_VSC_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_AllocateSCN
 *
 * Description      Look through the Server Channel Numbers for a free one to be
 *                  used with an RFCOMM connection.
 *
 * Returns          Allocated SCN number or 0 if none.
 *
 ******************************************************************************/
uint8_t BTM_AllocateSCN(void);

/*******************************************************************************
 *
 * Function         BTM_TryAllocateSCN
 *
 * Description      Try to allocate a fixed server channel
 *
 * Returns          Returns true if server channel was available
 *
 ******************************************************************************/
bool BTM_TryAllocateSCN(uint8_t scn);

/*******************************************************************************
 *
 * Function         BTM_FreeSCN
 *
 * Description      Free the specified SCN.
 *
 * Returns          true if successful, false if SCN is not in use or invalid
 *
 ******************************************************************************/
bool BTM_FreeSCN(uint8_t scn);

/*******************************************************************************
 *
 * Function         BTM_SetTraceLevel
 *
 * Description      This function sets the trace level for BTM.  If called with
 *                  a value of 0xFF, it simply returns the current trace level.
 *
 * Returns          The new or current trace level
 *
 ******************************************************************************/
uint8_t BTM_SetTraceLevel(uint8_t new_level);

/*******************************************************************************
 *
 * Function         BTM_WritePageTimeout
 *
 * Description      Send HCI Wite Page Timeout.
 *
 ******************************************************************************/
void BTM_WritePageTimeout(uint16_t timeout);

/*******************************************************************************
 *
 * Function         BTM_WriteVoiceSettings
 *
 * Description      Send HCI Write Voice Settings command.
 *                  See hcidefs.h for settings bitmask values.
 *
 ******************************************************************************/
void BTM_WriteVoiceSettings(uint16_t settings);

/*******************************************************************************
 *
 * Function         BTM_EnableTestMode
 *
 * Description      Send HCI the enable device under test command.
 *
 *                  Note: Controller can only be taken out of this mode by
 *                      resetting the controller.
 *
 * Returns
 *      BTM_SUCCESS         Command sent.
 *      BTM_NO_RESOURCES    If out of resources to send the command.
 *
 *
 ******************************************************************************/
tBTM_STATUS BTM_EnableTestMode(void);

/*******************************************************************************
 *
 * Function         BTM_ReadRemoteVersion
 *
 * Description      This function is called to read a remote device's version
 *
 * Returns          BTM_SUCCESS if successful, otherwise an error
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadRemoteVersion(const RawAddress& addr, uint8_t* lmp_version,
                                  uint16_t* manufacturer,
                                  uint16_t* lmp_sub_version);

/*******************************************************************************
 *
 * Function         BTM_ReadRemoteFeatures
 *
 * Description      This function is called to read a remote device's
 *                  supported features mask (features mask located at page 0)
 *
 *                  Note: The size of device features mask page is
 *                  BTM_FEATURE_BYTES_PER_PAGE bytes.
 *
 * Returns          pointer to the remote supported features mask
 *
 ******************************************************************************/
uint8_t* BTM_ReadRemoteFeatures(const RawAddress& addr);

/*****************************************************************************
 *  ACL CHANNEL MANAGEMENT FUNCTIONS
 ****************************************************************************/
/*******************************************************************************
 *
 * Function         BTM_SetLinkPolicy
 *
 * Description      Create and send HCI "Write Policy Set" command
 *
 * Returns          BTM_CMD_STARTED if successfully initiated, otherwise error
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetLinkPolicy(const RawAddress& remote_bda, uint16_t* settings);

/*******************************************************************************
 *
 * Function         BTM_SetDefaultLinkPolicy
 *
 * Description      Set the default value for HCI "Write Policy Set" command
 *                  to use when an ACL link is created.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_SetDefaultLinkPolicy(uint16_t settings);

/*******************************************************************************
 *
 * Function         BTM_SetDefaultLinkSuperTout
 *
 * Description      Set the default value for HCI "Write Link Supervision
 *                  Timeout" command to use when an ACL link is created.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_SetDefaultLinkSuperTout(uint16_t timeout);

/*******************************************************************************
 *
 * Function         BTM_SetLinkSuperTout
 *
 * Description      Create and send HCI "Write Link Supervision Timeout" command
 *
 * Returns          BTM_CMD_STARTED if successfully initiated, otherwise error
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetLinkSuperTout(const RawAddress& remote_bda,
                                 uint16_t timeout);
/*******************************************************************************
 *
 * Function         BTM_GetLinkSuperTout
 *
 * Description      Read the link supervision timeout value of the connection
 *
 * Returns          status of the operation
 *
 ******************************************************************************/
tBTM_STATUS BTM_GetLinkSuperTout(const RawAddress& remote_bda,
                                 uint16_t* p_timeout);

/*******************************************************************************
 *
 * Function         BTM_IsAclConnectionUp
 *
 * Description      This function is called to check if an ACL connection exists
 *                  to a specific remote BD Address.
 *
 * Returns          true if connection is up, else false.
 *
 ******************************************************************************/
bool BTM_IsAclConnectionUp(const RawAddress& remote_bda,
                           tBT_TRANSPORT transport);

/*******************************************************************************
 *
 * Function         BTM_GetRole
 *
 * Description      This function is called to get the role of the local device
 *                  for the ACL connection with the specified remote device
 *
 * Returns          BTM_SUCCESS if connection exists.
 *                  BTM_UNKNOWN_ADDR if no active link with bd addr specified
 *
 ******************************************************************************/
tBTM_STATUS BTM_GetRole(const RawAddress& remote_bd_addr, uint8_t* p_role);

/*******************************************************************************
 *
 * Function         BTM_SwitchRole
 *
 * Description      This function is called to switch role between master and
 *                  slave.  If role is already set it will do nothing.  If the
 *                  command was initiated, the callback function is called upon
 *                  completion.
 *
 * Returns          BTM_SUCCESS if already in specified role.
 *                  BTM_CMD_STARTED if command issued to controller.
 *                  BTM_NO_RESOURCES if memory couldn't be allocated to issue
 *                                   the command
 *                  BTM_UNKNOWN_ADDR if no active link with bd addr specified
 *                  BTM_MODE_UNSUPPORTED if the local device does not support
 *                                       role switching
 *
 ******************************************************************************/
tBTM_STATUS BTM_SwitchRole(const RawAddress& remote_bd_addr, uint8_t new_role,
                           tBTM_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_ReadRSSI
 *
 * Description      This function is called to read the link policy settings.
 *                  The address of link policy results are returned in the
 *                  callback. (tBTM_RSSI_RESULT)
 *
 * Returns          BTM_CMD_STARTED if command issued to controller.
 *                  BTM_NO_RESOURCES if memory couldn't be allocated to issue
 *                                   the command
 *                  BTM_UNKNOWN_ADDR if no active link with bd addr specified
 *                  BTM_BUSY if command is already in progress
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadRSSI(const RawAddress& remote_bda, tBTM_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_ReadFailedContactCounter
 *
 * Description      This function is called to read the failed contact counter.
 *                  The result is returned in the callback.
 *                  (tBTM_FAILED_CONTACT_COUNTER_RESULT)
 *
 * Returns          BTM_CMD_STARTED if command issued to controller.
 *                  BTM_NO_RESOURCES if memory couldn't be allocated to issue
 *                                   the command
 *                  BTM_UNKNOWN_ADDR if no active link with bd addr specified
 *                  BTM_BUSY if command is already in progress
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadFailedContactCounter(const RawAddress& remote_bda,
                                         tBTM_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_ReadAutomaticFlushTimeout
 *
 * Description      This function is called to read the automatic flush timeout.
 *                  The result is returned in the callback.
 *                  (tBTM_AUTOMATIC_FLUSH_TIMEOUT_RESULT)
 *
 * Returns          BTM_CMD_STARTED if command issued to controller.
 *                  BTM_NO_RESOURCES if memory couldn't be allocated to issue
 *                                   the command
 *                  BTM_UNKNOWN_ADDR if no active link with bd addr specified
 *                  BTM_BUSY if command is already in progress
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadAutomaticFlushTimeout(const RawAddress& remote_bda,
                                          tBTM_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_ReadTxPower
 *
 * Description      This function is called to read the current connection
 *                  TX power of the connection. The TX power level results
 *                  are returned in the callback.
 *                  (tBTM_RSSI_RESULT)
 *
 * Returns          BTM_CMD_STARTED if command issued to controller.
 *                  BTM_NO_RESOURCES if memory couldn't be allocated to issue
 *                                   the command
 *                  BTM_UNKNOWN_ADDR if no active link with bd addr specified
 *                  BTM_BUSY if command is already in progress
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadTxPower(const RawAddress& remote_bda,
                            tBT_TRANSPORT transport, tBTM_CMPL_CB* p_cb);

/*******************************************************************************
 *
 * Function         BTM_RegBusyLevelNotif
 *
 * Description      This function is called to register a callback to receive
 *                  busy level change events.
 *
 * Returns          BTM_SUCCESS if successfully registered, otherwise error
 *
 ******************************************************************************/
tBTM_STATUS BTM_RegBusyLevelNotif(tBTM_BL_CHANGE_CB* p_cb, uint8_t* p_level,
                                  tBTM_BL_EVENT_MASK evt_mask);

/*******************************************************************************
 *
 * Function         BTM_GetNumAclLinks
 *
 * Description      This function is called to count the number of
 *                  ACL links that are active.
 *
 * Returns          uint16_t Number of active ACL links
 *
 ******************************************************************************/
uint16_t BTM_GetNumAclLinks(void);

/*****************************************************************************
 *  (e)SCO CHANNEL MANAGEMENT FUNCTIONS
 ****************************************************************************/
/*******************************************************************************
 *
 * Function         BTM_CreateSco
 *
 * Description      This function is called to create an SCO connection. If the
 *                  "is_orig" flag is true, the connection will be originated,
 *                  otherwise BTM will wait for the other side to connect.
 *
 * Returns          BTM_UNKNOWN_ADDR if the ACL connection is not up
 *                  BTM_BUSY         if another SCO being set up to
 *                                   the same BD address
 *                  BTM_NO_RESOURCES if the max SCO limit has been reached
 *                  BTM_CMD_STARTED  if the connection establishment is started.
 *                                   In this case, "*p_sco_inx" is filled in
 *                                   with the sco index used for the connection.
 *
 ******************************************************************************/
tBTM_STATUS BTM_CreateSco(const RawAddress* remote_bda, bool is_orig,
                          uint16_t pkt_types, uint16_t* p_sco_inx,
                          tBTM_SCO_CB* p_conn_cb, tBTM_SCO_CB* p_disc_cb);

/*******************************************************************************
 *
 * Function         BTM_RemoveSco
 *
 * Description      This function is called to remove a specific SCO connection.
 *
 * Returns          BTM_CMD_STARTED if successfully initiated, otherwise error
 *
 ******************************************************************************/
tBTM_STATUS BTM_RemoveSco(uint16_t sco_inx);

/*******************************************************************************
 *
 * Function         BTM_ReadScoBdAddr
 *
 * Description      This function is read the remote BD Address for a specific
 *                  SCO connection,
 *
 * Returns          pointer to BD address or NULL if not known
 *
 ******************************************************************************/
const RawAddress* BTM_ReadScoBdAddr(uint16_t sco_inx);

/*******************************************************************************
 *
 * Function         BTM_SetEScoMode
 *
 * Description      This function sets up the negotiated parameters for SCO or
 *                  eSCO, and sets as the default mode used for calls to
 *                  BTM_CreateSco.  It can be called only when there are no
 *                  active (e)SCO links.
 *
 * Returns          BTM_SUCCESS if the successful.
 *                  BTM_BUSY if there are one or more active (e)SCO links.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetEScoMode(enh_esco_params_t* p_parms);

/*******************************************************************************
 *
 * Function         BTM_RegForEScoEvts
 *
 * Description      This function registers a SCO event callback with the
 *                  specified instance.  It should be used to received
 *                  connection indication events and change of link parameter
 *                  events.
 *
 * Returns          BTM_SUCCESS if the successful.
 *                  BTM_ILLEGAL_VALUE if there is an illegal sco_inx
 *
 ******************************************************************************/
tBTM_STATUS BTM_RegForEScoEvts(uint16_t sco_inx, tBTM_ESCO_CBACK* p_esco_cback);

/*******************************************************************************
 *
 * Function         BTM_ChangeEScoLinkParms
 *
 * Description      This function requests renegotiation of the parameters on
 *                  the current eSCO Link.  If any of the changes are accepted
 *                  by the controllers, the BTM_ESCO_CHG_EVT event is sent in
 *                  the tBTM_ESCO_CBACK function with the current settings of
 *                  the link. The callback is registered through the call to
 *                  BTM_SetEScoMode.
 *
 *
 * Returns          BTM_CMD_STARTED if command is successfully initiated.
 *                  BTM_ILLEGAL_VALUE if no connection for specified sco_inx.
 *                  BTM_NO_RESOURCES - not enough resources to initiate command.
 *                  BTM_MODE_UNSUPPORTED if local controller does not support
 *                      1.2 specification.
 *
 ******************************************************************************/
tBTM_STATUS BTM_ChangeEScoLinkParms(uint16_t sco_inx,
                                    tBTM_CHG_ESCO_PARAMS* p_parms);

/*******************************************************************************
 *
 * Function         BTM_EScoConnRsp
 *
 * Description      This function is called upon receipt of an (e)SCO connection
 *                  request event (BTM_ESCO_CONN_REQ_EVT) to accept or reject
 *                  the request. Parameters used to negotiate eSCO links.
 *                  If p_parms is NULL, then values set through BTM_SetEScoMode
 *                  are used.
 *                  If the link type of the incoming request is SCO, then only
 *                  the tx_bw, max_latency, content format, and packet_types are
 *                  valid.  The hci_status parameter should be
 *                  ([0x0] to accept, [0x0d..0x0f] to reject)
 *
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_EScoConnRsp(uint16_t sco_inx, uint8_t hci_status,
                     enh_esco_params_t* p_parms);

/*******************************************************************************
 *
 * Function         BTM_GetNumScoLinks
 *
 * Description      This function returns the number of active SCO links.
 *
 * Returns          uint8_t
 *
 ******************************************************************************/
uint8_t BTM_GetNumScoLinks(void);

/*****************************************************************************
 *  SECURITY MANAGEMENT FUNCTIONS
 ****************************************************************************/
/*******************************************************************************
 *
 * Function         BTM_SecRegister
 *
 * Description      Application manager calls this function to register for
 *                  security services.  There can be one and only one
 *                  application saving link keys.  BTM allows only first
 *                  registration.
 *
 * Returns          true if registered OK, else false
 *
 ******************************************************************************/
bool BTM_SecRegister(const tBTM_APPL_INFO* p_cb_info);

/*******************************************************************************
 *
 * Function         BTM_SecAddRmtNameNotifyCallback
 *
 * Description      Profiles can register to be notified when name of the
 *                  remote device is resolved (up to
 *                  BTM_SEC_MAX_RMT_NAME_CALLBACKS).
 *
 * Returns          true if registered OK, else false
 *
 ******************************************************************************/
bool BTM_SecAddRmtNameNotifyCallback(tBTM_RMT_NAME_CALLBACK* p_callback);

/*******************************************************************************
 *
 * Function         BTM_SecDeleteRmtNameNotifyCallback
 *
 * Description      A profile can deregister notification when a new Link Key
 *                  is generated per connection.
 *
 * Returns          true if OK, else false
 *
 ******************************************************************************/
bool BTM_SecDeleteRmtNameNotifyCallback(tBTM_RMT_NAME_CALLBACK* p_callback);

/*******************************************************************************
 *
 * Function         BTM_GetSecurityFlags
 *
 * Description      Get security flags for the device
 *
 * Returns          bool    true or false is device found
 *
 ******************************************************************************/
bool BTM_GetSecurityFlags(const RawAddress& bd_addr, uint8_t* p_sec_flags);

/*******************************************************************************
 *
 * Function         BTM_GetSecurityFlagsByTransport
 *
 * Description      Get security flags for the device on a particular transport
 *
 * Parameters      bd_addr: BD address of remote device
 *                  p_sec_flags : Out parameter to be filled with security
 *                                flags for the connection
 *                  transport :  Physical transport of the connection
 *                               (BR/EDR or LE)
 *
 * Returns          bool    true or false is device found
 *
 ******************************************************************************/
bool BTM_GetSecurityFlagsByTransport(const RawAddress& bd_addr,
                                     uint8_t* p_sec_flags,
                                     tBT_TRANSPORT transport);

/*******************************************************************************
 *
 * Function         BTM_ReadTrustedMask
 *
 * Description      Get trusted mask for the device
 *
 * Returns          NULL, if the device record is not found.
 *                  otherwise, the trusted mask
 *
 ******************************************************************************/
uint32_t* BTM_ReadTrustedMask(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_SetPinType
 *
 * Description      Set PIN type for the device.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_SetPinType(uint8_t pin_type, PIN_CODE pin_code, uint8_t pin_code_len);

/*******************************************************************************
 *
 * Function         BTM_SetPairableMode
 *
 * Description      Enable or disable pairing
 *
 * Parameters       allow_pairing - (true or false) whether or not the device
 *                      allows pairing.
 *                  connect_only_paired - (true or false) whether or not to
 *                      only allow paired devices to connect.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_SetPairableMode(bool allow_pairing, bool connect_only_paired);

/*******************************************************************************
 *
 * Function         BTM_SetSecurityLevel
 *
 * Description      Register service security level with Security Manager.  Each
 *                  service must register its requirements regardless of the
 *                  security level that is used.  This API is called once for
 *                  originators and again for acceptors of connections.
 *
 * Returns          true if registered OK, else false
 *
 ******************************************************************************/
bool BTM_SetSecurityLevel(bool is_originator, const char* p_name,
                          uint8_t service_id, uint16_t sec_level, uint16_t psm,
                          uint32_t mx_proto_id, uint32_t mx_chan_id);

/*******************************************************************************
 *
 * Function         BTM_SetOutService
 *
 * Description      This function is called to set the service for
 *                  outgoing connection.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_SetOutService(const RawAddress& bd_addr, uint8_t service_id,
                       uint32_t mx_chan_id);

/*******************************************************************************
 *
 * Function         BTM_SecClrService
 *
 * Description      Removes specified service record(s) from the security
 *                  database. All service records with the specified name are
 *                  removed. Typically used only by devices with limited RAM
 *                  so that it can reuse an old security service record.
 *
 * Returns          Number of records that were freed.
 *
 ******************************************************************************/
uint8_t BTM_SecClrService(uint8_t service_id);

/*******************************************************************************
 *
 * Function         BTM_SecAddDevice
 *
 * Description      Add/modify device.  This function will be normally called
 *                  during host startup to restore all required information
 *                  stored in the NVRAM.
 *                  dev_class, bd_name, link_key, and features are NULL if
 *                  unknown
 *
 * Returns          true if added OK, else false
 *
 ******************************************************************************/
bool BTM_SecAddDevice(const RawAddress& bd_addr, DEV_CLASS dev_class,
                      BD_NAME bd_name, uint8_t* features,
                      uint32_t trusted_mask[], LinkKey* link_key,
                      uint8_t key_type, tBTM_IO_CAP io_cap, uint8_t pin_length);

/** Free resources associated with the device associated with |bd_addr| address.
 *
 * *** WARNING ***
 * tBTM_SEC_DEV_REC associated with bd_addr becomes invalid after this function
 * is called, also any of it's fields. i.e. if you use p_dev_rec->bd_addr, it is
 * no longer valid!
 * *** WARNING ***
 *
 * Returns true if removed OK, false if not found or ACL link is active.
 */
bool BTM_SecDeleteDevice(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_SecClearSecurityFlags
 *
 * Description      Reset the security flags (mark as not-paired) for a given
 *                  remove device.
 *
 ******************************************************************************/
void BTM_SecClearSecurityFlags(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_SecGetDeviceLinkKeyType
 *
 * Description      This function is called to obtain link key type for the
 *                  device.
 *                  it returns BTM_SUCCESS if link key is available, or
 *                  BTM_UNKNOWN_ADDR if Security Manager does not know about
 *                  the device or device record does not contain link key info
 *
 * Returns          BTM_LKEY_TYPE_IGNORE if link key is unknown, link type
 *                  otherwise.
 *
 ******************************************************************************/
tBTM_LINK_KEY_TYPE BTM_SecGetDeviceLinkKeyType(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_PINCodeReply
 *
 * Description      This function is called after Security Manager submitted
 *                  PIN code request to the UI.
 *
 * Parameters:      bd_addr      - Address of the device for which PIN was
 *                                 requested
 *                  res          - result of the operation BTM_SUCCESS if
 *                                 success
 *                  pin_len      - length in bytes of the PIN Code
 *                  p_pin        - pointer to array with the PIN Code
 *                  trusted_mask - bitwise OR of trusted services
 *                                 (array of uint32_t)
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_PINCodeReply(const RawAddress& bd_addr, uint8_t res, uint8_t pin_len,
                      uint8_t* p_pin, uint32_t trusted_mask[]);

/*******************************************************************************
 *
 * Function         BTM_SecBond
 *
 * Description      Perform bonding by designated transport
 *
 * Parameters:      bd_addr      - Address of the device to bond
 *                  addr_type    - address type for LE transport
 *                  pin_len      - length in bytes of the PIN Code
 *                  p_pin        - pointer to array with the PIN Code
 *                  trusted_mask - bitwise OR of trusted services
 *                                 (array of uint32_t)
 *                  transport :  Physical transport to use for bonding
 *                               (BR/EDR or LE)
 *
 * Returns          BTM_CMD_STARTED if successfully initiated, otherwise error
 *
 ******************************************************************************/
tBTM_STATUS BTM_SecBond(const RawAddress& bd_addr, tBLE_ADDR_TYPE addr_type,
                        tBT_TRANSPORT transport, uint8_t pin_len,
                        uint8_t* p_pin, uint32_t trusted_mask[]);

/*******************************************************************************
 *
 * Function         BTM_SecBondCancel
 *
 * Description      This function is called to cancel ongoing bonding process
 *                  with peer device.
 *
 * Returns          BTM_CMD_STARTED if successfully initiated, otherwise error
 *
 ******************************************************************************/
tBTM_STATUS BTM_SecBondCancel(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_SetEncryption
 *
 * Description      This function is called to ensure that connection is
 *                  encrypted.  Should be called only on an open connection.
 *                  Typically only needed for connections that first want to
 *                  bring up unencrypted links, then later encrypt them.
 *
 * Parameters:      bd_addr       - Address of the peer device
 *                  transport     - Link transport
 *                  p_callback    - Pointer to callback function called if
 *                                  this function returns PENDING after required
 *                                  procedures are completed.  Can be set to
 *                                  NULL if status is not desired.
 *                  p_ref_data    - pointer to any data the caller wishes to
 *                                  receive in the callback function upon
 *                                  completion.
 *                                  can be set to NULL if not used.
 *                  sec_act       - LE security action, unused for BR/EDR
 *
 * Returns          BTM_SUCCESS   - already encrypted
 *                  BTM_PENDING   - command will be returned in the callback
 *                  BTM_WRONG_MODE- connection not up.
 *                  BTM_BUSY      - security procedures are currently active
 *                  BTM_MODE_UNSUPPORTED - if security manager not linked in.
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetEncryption(const RawAddress& bd_addr,
                              tBT_TRANSPORT transport,
                              tBTM_SEC_CBACK* p_callback, void* p_ref_data,
                              tBTM_BLE_SEC_ACT sec_act);

/*******************************************************************************
 *
 * Function         BTM_ConfirmReqReply
 *
 * Description      This function is called to confirm the numeric value for
 *                  Simple Pairing in response to BTM_SP_CFM_REQ_EVT
 *
 * Parameters:      res           - result of the operation BTM_SUCCESS if
 *                                  success
 *                  bd_addr       - Address of the peer device
 *
 ******************************************************************************/
void BTM_ConfirmReqReply(tBTM_STATUS res, const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_PasskeyReqReply
 *
 * Description      This function is called to provide the passkey for
 *                  Simple Pairing in response to BTM_SP_KEY_REQ_EVT
 *
 * Parameters:      res           - result of the operation BTM_SUCCESS if
 *                                  success
 *                  bd_addr       - Address of the peer device
 *                  passkey       - numeric value in the range of
 *                                  0 - 999999(0xF423F).
 *
 ******************************************************************************/
void BTM_PasskeyReqReply(tBTM_STATUS res, const RawAddress& bd_addr,
                         uint32_t passkey);

/*******************************************************************************
 *
 * Function         BTM_SendKeypressNotif
 *
 * Description      This function is used during the passkey entry model
 *                  by a device with KeyboardOnly IO capabilities
 *                  (very likely to be a HID Device).
 *                  It is called by a HID Device to inform the remote device
 *                  when a key has been entered or erased.
 *
 * Parameters:      bd_addr - Address of the peer device
 *                  type - notification type
 *
 ******************************************************************************/
void BTM_SendKeypressNotif(const RawAddress& bd_addr, tBTM_SP_KEY_TYPE type);

/*******************************************************************************
 *
 * Function         BTM_IoCapRsp
 *
 * Description      This function is called in response to BTM_SP_IO_REQ_EVT
 *                  When the event data io_req.oob_data is set to
 *                  BTM_OOB_UNKNOWN by the tBTM_SP_CALLBACK implementation, this
 *                  function is called to provide the actual response
 *
 * Parameters:      bd_addr - Address of the peer device
 *                  io_cap  - The IO capability of local device.
 *                  oob     - BTM_OOB_NONE or BTM_OOB_PRESENT.
 *                  auth_req- MITM protection required or not.
 *
 ******************************************************************************/
void BTM_IoCapRsp(const RawAddress& bd_addr, tBTM_IO_CAP io_cap,
                  tBTM_OOB_DATA oob, tBTM_AUTH_REQ auth_req);

/*******************************************************************************
 *
 * Function         BTM_ReadLocalOobData
 *
 * Description      This function is called to read the local OOB data from
 *                  LM
 *
 ******************************************************************************/
void BTM_ReadLocalOobData(void);

/*******************************************************************************
 *
 * Function         BTM_RemoteOobDataReply
 *
 * Description      This function is called to provide the remote OOB data for
 *                  Simple Pairing in response to BTM_SP_RMT_OOB_EVT
 *
 * Parameters:      bd_addr     - Address of the peer device
 *                  c           - simple pairing Hash C.
 *                  r           - simple pairing Randomizer  C.
 *
 ******************************************************************************/
void BTM_RemoteOobDataReply(tBTM_STATUS res, const RawAddress& bd_addr,
                            const Octet16& c, const Octet16& r);

/*******************************************************************************
 *
 * Function         BTM_BothEndsSupportSecureConnections
 *
 * Description      This function is called to check if both the local device
 *                  and the peer device specified by bd_addr support BR/EDR
 *                  Secure Connections.
 *
 * Parameters:      bd_addr - address of the peer
 *
 * Returns          true if BR/EDR Secure Connections are supported by both
 *                  local and the remote device.
 *                  else false.
 *
 ******************************************************************************/
bool BTM_BothEndsSupportSecureConnections(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_PeerSupportsSecureConnections
 *
 * Description      This function is called to check if the peer supports
 *                  BR/EDR Secure Connections.
 *
 * Parameters:      bd_addr - address of the peer
 *
 * Returns          true if BR/EDR Secure Connections are supported by the peer,
 *                  else false.
 *
 ******************************************************************************/
bool BTM_PeerSupportsSecureConnections(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         BTM_SecReadDevName
 *
 * Description      Looks for the device name in the security database for the
 *                  specified BD address.
 *
 * Returns          Pointer to the name or NULL
 *
 ******************************************************************************/
char* BTM_SecReadDevName(const RawAddress& bd_addr);

/*****************************************************************************
 *  POWER MANAGEMENT FUNCTIONS
 ****************************************************************************/
/*******************************************************************************
 *
 * Function         BTM_PmRegister
 *
 * Description      register or deregister with power manager
 *
 * Returns          BTM_SUCCESS if successful,
 *                  BTM_NO_RESOURCES if no room to hold registration
 *                  BTM_ILLEGAL_VALUE
 *
 ******************************************************************************/
tBTM_STATUS BTM_PmRegister(uint8_t mask, uint8_t* p_pm_id,
                           tBTM_PM_STATUS_CBACK* p_cb);

/*******************************************************************************
 *
 * Function         BTM_SetPowerMode
 *
 * Description      store the mode in control block or
 *                  alter ACL connection behavior.
 *
 * Returns          BTM_SUCCESS if successful,
 *                  BTM_UNKNOWN_ADDR if bd addr is not active or bad
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetPowerMode(uint8_t pm_id, const RawAddress& remote_bda,
                             const tBTM_PM_PWR_MD* p_mode);

/*******************************************************************************
 *
 * Function         BTM_ReadPowerMode
 *
 * Description      This returns the current mode for a specific
 *                  ACL connection.
 *
 * Input Param      remote_bda - device address of desired ACL connection
 *
 * Output Param     p_mode - address where the current mode is copied into.
 *                          BTM_ACL_MODE_NORMAL
 *                          BTM_ACL_MODE_HOLD
 *                          BTM_ACL_MODE_SNIFF
 *                          BTM_ACL_MODE_PARK
 *                          (valid only if return code is BTM_SUCCESS)
 *
 * Returns          BTM_SUCCESS if successful,
 *                  BTM_UNKNOWN_ADDR if bd addr is not active or bad
 *
 ******************************************************************************/
tBTM_STATUS BTM_ReadPowerMode(const RawAddress& remote_bda,
                              tBTM_PM_MODE* p_mode);

/*******************************************************************************
 *
 * Function         BTM_SetSsrParams
 *
 * Description      This sends the given SSR parameters for the given ACL
 *                  connection if it is in ACTIVE mode.
 *
 * Input Param      remote_bda - device address of desired ACL connection
 *                  max_lat    - maximum latency (in 0.625ms)(0-0xFFFE)
 *                  min_rmt_to - minimum remote timeout
 *                  min_loc_to - minimum local timeout
 *
 *
 * Returns          BTM_SUCCESS if the HCI command is issued successful,
 *                  BTM_UNKNOWN_ADDR if bd addr is not active or bad
 *                  BTM_CMD_STORED if the command is stored
 *
 ******************************************************************************/
tBTM_STATUS BTM_SetSsrParams(const RawAddress& remote_bda, uint16_t max_lat,
                             uint16_t min_rmt_to, uint16_t min_loc_to);

/*******************************************************************************
 *
 * Function         BTM_GetHCIConnHandle
 *
 * Description      This function is called to get the handle for an ACL
 *                  connection to a specific remote BD Address.
 *
 * Returns          the handle of the connection, or 0xFFFF if none.
 *
 ******************************************************************************/
uint16_t BTM_GetHCIConnHandle(const RawAddress& remote_bda,
                              tBT_TRANSPORT transport);

/*******************************************************************************
 *
 * Function         BTM_DeleteStoredLinkKey
 *
 * Description      This function is called to delete link key for the specified
 *                  device addresses from the NVRAM storage attached to the
 *                  Bluetooth controller.
 *
 * Parameters:      bd_addr      - Addresses of the devices
 *                  p_cb         - Call back function to be called to return
 *                                 the results
 *
 ******************************************************************************/
tBTM_STATUS BTM_DeleteStoredLinkKey(const RawAddress* bd_addr,
                                    tBTM_CMPL_CB* p_cb);

tBTM_CONTRL_STATE BTM_PM_ReadControllerState(void);

/**
 *
 * BLE API
 */
/*******************************************************************************
 *
 * Function         BTM_BleObtainVendorCapabilities
 *
 * Description      This function is called to obatin vendor capabilties
 *
 * Parameters       p_cmn_vsc_cb - Returns the vednor capabilities
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleObtainVendorCapabilities(tBTM_BLE_VSC_CB* p_cmn_vsc_cb);

/**
 * This function is called to set scan parameters. |cb| is called with operation
 * status
 **/
void BTM_BleSetScanParams(uint32_t scan_interval, uint32_t scan_window,
                          tBLE_SCAN_MODE scan_type,
                          base::Callback<void(uint8_t)> cb);

/*******************************************************************************
 *
 * Function         BTM_BleGetVendorCapabilities
 *
 * Description      This function reads local LE features
 *
 * Parameters       p_cmn_vsc_cb : Locala LE capability structure
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleGetVendorCapabilities(tBTM_BLE_VSC_CB* p_cmn_vsc_cb);

/*******************************************************************************
 *
 * Function         BTM_BleSetStorageConfig
 *
 * Description      This function is called to setup storage configuration and
 *                  setup callbacks.
 *
 * Parameters       uint8_t batch_scan_full_max -Batch scan full maximum
 *                  uint8_t batch_scan_trunc_max - Batch scan truncated value
 *maximum uint8_t batch_scan_notify_threshold - Threshold value cb - Setup
 *callback tBTM_BLE_SCAN_THRESHOLD_CBACK *p_thres_cback -Threshold callback void
 **p_ref - Reference value
 *
 *
 ******************************************************************************/
void BTM_BleSetStorageConfig(uint8_t batch_scan_full_max,
                             uint8_t batch_scan_trunc_max,
                             uint8_t batch_scan_notify_threshold,
                             base::Callback<void(uint8_t /* status */)> cb,
                             tBTM_BLE_SCAN_THRESHOLD_CBACK* p_thres_cback,
                             tBTM_BLE_REF_VALUE ref_value);

/* This function is called to enable batch scan */
void BTM_BleEnableBatchScan(tBTM_BLE_BATCH_SCAN_MODE scan_mode,
                            uint32_t scan_interval, uint32_t scan_window,
                            tBTM_BLE_DISCARD_RULE discard_rule,
                            tBLE_ADDR_TYPE addr_type,
                            base::Callback<void(uint8_t /* status */)> cb);

/* This function is called to disable batch scanning */
void BTM_BleDisableBatchScan(base::Callback<void(uint8_t /* status */)> cb);

/* This function is called to read batch scan reports */
void BTM_BleReadScanReports(tBLE_SCAN_MODE scan_mode,
                            tBTM_BLE_SCAN_REP_CBACK cb);

/* This function is called to setup the callback for tracking */
void BTM_BleTrackAdvertiser(tBTM_BLE_TRACK_ADV_CBACK* p_track_cback,
                            tBTM_BLE_REF_VALUE ref_value);

/*******************************************************************************
 *
 * Function         BTM_BleWriteScanRsp
 *
 * Description      This function is called to write LE scan response.
 *
 * Parameters:      p_scan_rsp: scan response.
 *
 * Returns          status
 *
 ******************************************************************************/
void BTM_BleWriteScanRsp(uint8_t* data, uint8_t length,
                         tBTM_BLE_ADV_DATA_CMPL_CBACK* p_adv_data_cback);

/******************************************************************************
 *
 * Function         BTM_BleReadControllerFeatures
 *
 * Description      Reads BLE specific controller features
 *
 * Parameters:      tBTM_BLE_CTRL_FEATURES_CBACK : Callback to notify when
 *                  features are read
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleReadControllerFeatures(tBTM_BLE_CTRL_FEATURES_CBACK* p_vsc_cback);

/*******************************************************************************
 *
 * Function         BTM__BLEReadDiscoverability
 *
 * Description      This function is called to read the current LE
 *                  discoverability mode of the device.
 *
 * Returns          BTM_BLE_NON_DISCOVERABLE ,BTM_BLE_LIMITED_DISCOVERABLE or
 *                     BTM_BLE_GENRAL_DISCOVERABLE
 *
 ******************************************************************************/
uint16_t BTM_BleReadDiscoverability();

/*******************************************************************************
 *
 * Function         BTM__BLEReadConnectability
 *
 * Description      This function is called to read the current LE
 *                  connectibility mode of the device.
 *
 * Returns          BTM_BLE_NON_CONNECTABLE or BTM_BLE_CONNECTABLE
 *
 ******************************************************************************/
uint16_t BTM_BleReadConnectability();

/*******************************************************************************
 *
 * Function         BTM_BleConfigPrivacy
 *
 * Description      This function is called to enable or disable the privacy in
 *                  the local device.
 *
 * Parameters       enable: true to enable it; false to disable it.
 *
 * Returns          bool    privacy mode set success; otherwise failed.
 *
 ******************************************************************************/
bool BTM_BleConfigPrivacy(bool enable);

/*******************************************************************************
 *
 * Function         BTM_BleLocalPrivacyEnabled
 *
 * Description        Checks if local device supports private address
 *
 * Returns          Return true if local privacy is enabled else false
 *
 ******************************************************************************/
bool BTM_BleLocalPrivacyEnabled(void);

/*******************************************************************************
 *
 * Function         BTM_BleEnableMixedPrivacyMode
 *
 * Description      This function is called to enabled Mixed mode if privacy 1.2
 *                  is applicable in controller.
 *
 * Parameters       mixed_on:  mixed mode to be used or not.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleEnableMixedPrivacyMode(bool mixed_on);

/*******************************************************************************
 *
 * Function         BTM_BleSetConnectableMode
 *
 * Description      This function is called to set BLE connectable mode for a
 *                  peripheral device.
 *
 * Parameters       connectable_mode:  directed connectable mode, or
 *                                     non-directed. It can be
 *                                     BTM_BLE_CONNECT_EVT,
 *                                     BTM_BLE_CONNECT_DIR_EVT or
 *                                     BTM_BLE_CONNECT_LO_DUTY_DIR_EVT
 *
 * Returns          BTM_ILLEGAL_VALUE if controller does not support BLE.
 *                  BTM_SUCCESS is status set successfully; otherwise failure.
 *
 ******************************************************************************/
tBTM_STATUS BTM_BleSetConnectableMode(tBTM_BLE_CONN_MODE connectable_mode);

/*******************************************************************************
 *
 * Function         BTM_BleTurnOnPrivacyOnRemote
 *
 * Description      This function is called to enable or disable the privacy on
 *                  the remote device.
 *
 * Parameters       bd_addr: remote device address.
 *                  privacy_on: true to enable it; false to disable it.
 *
 * Returns          void
 *
 ******************************************************************************/
void BTM_BleTurnOnPrivacyOnRemote(const RawAddress& bd_addr, bool privacy_on);

/*******************************************************************************
 *
 * Function         BTM_BleStackEnable
 *
 * Description      Enable/Disable BLE functionality on stack regardless of
 *                  controller capability.
 *
 * Parameters:      enable: true to enable, false to disable.
 *
 * Returns          true if added OK, else false
 *
 ******************************************************************************/
tBTM_STATUS BTM_BleStackEnable(bool enable);

/**
 * This functions are called to configure the adv data payload filter condition
 */
/*******************************************************************************
 *
 * Function         BTM_BleGetEnergyInfo
 *
 * Description      This function obtains the energy info
 *
 * Parameters       p_ener_cback - Callback pointer
 *
 * Returns          status
 *
 ******************************************************************************/
tBTM_STATUS BTM_BleGetEnergyInfo(tBTM_BLE_ENERGY_INFO_CBACK* p_ener_cback);

/*******************************************************************************
 *
 * Function         BTM_SecBond
 *
 * Description      This function is called to perform bonding with peer device.
 *                  If the connection is already up, but not secure, pairing
 *                  is attempted.  If already paired BTM_SUCCESS is returned.
 *
 * Parameters:      bd_addr      - Address of the device to bond
 *                  addr_type    - address type for LE transport
 *                  transport    - doing SSP over BR/EDR or SMP over LE
 *                  pin_len      - length in bytes of the PIN Code
 *                  p_pin        - pointer to array with the PIN Code
 *                  trusted_mask - bitwise OR of trusted services
 *                                 (array of uint32_t)
 *
 *  Note: After 2.1 parameters are not used and preserved here not to change API
 ******************************************************************************/
tBTM_STATUS BTM_SecBond(const RawAddress& bd_addr, tBLE_ADDR_TYPE addr_type,
                        tBT_TRANSPORT transport, uint8_t pin_len,
                        uint8_t* p_pin, uint32_t trusted_mask[]);

/*******************************************************************************
 *
 * Function         BTM_SecRegister
 *
 * Description      Application manager calls this function to register for
 *                  security services.  There can be one and only one
 *                  application saving link keys.  BTM allows only first
 *                  registration.
 *
 * Returns          true if registered OK, else false
 *
 ******************************************************************************/
bool BTM_SecRegister(const tBTM_APPL_INFO* p_cb_info);

/** Free resources associated with the device associated with |bd_addr| address.
 *
 * *** WARNING ***
 * tBTM_SEC_DEV_REC associated with bd_addr becomes invalid after this function
 * is called, also any of it's fields. i.e. if you use p_dev_rec->bd_addr, it is
 * no longer valid!
 * *** WARNING ***
 *
 * Returns true if removed OK, false if not found or ACL link is active.
 */
bool BTM_SecDeleteDevice(const RawAddress& bd_addr);

}  // namespace shim
}  // namespace bluetooth
