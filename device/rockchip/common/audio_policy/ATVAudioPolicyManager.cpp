/*
 * Copyright 2018 Fuzhou Rockchip Electronics Co. LTD
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

#define LOG_TAG "RKATVAudioPolicyManager"
//#define LOG_NDEBUG 0
#include <media/AudioParameter.h>
#include <media/mediarecorder.h>
#include <utils/Log.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>
#include <cutils/properties.h>

#include "ATVAudioPolicyManager.h"


namespace android {

// ---  class factory
extern "C" AudioPolicyInterface* createAudioPolicyManager(
        AudioPolicyClientInterface *clientInterface)
{
    ALOGD("%s: RKATVAudioPolicyManager ",__FUNCTION__);
    ATVAudioPolicyManager *apm = new ATVAudioPolicyManager(clientInterface);
    status_t status = apm->initialize();
    if (status != NO_ERROR) {
        delete apm;
        apm = nullptr;
    }
    return apm;
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

ATVAudioPolicyManager::ATVAudioPolicyManager(
        AudioPolicyClientInterface *clientInterface)
    : AudioPolicyManager(clientInterface)
{
    ALOGD("%s",__FUNCTION__);
    mBitstreamDevice = AUDIO_DEVICE_NONE;
}

status_t ATVAudioPolicyManager::initialize() {
    return AudioPolicyManager::initialize();
}


bool ATVAudioPolicyManager::isAlreadConnect(audio_devices_t device,
                                            audio_policy_dev_state_t state,
                                            const char *device_address,
                                            const char *device_name,
                                            audio_format_t encodedFormat)
{
    // if device already in mAvailableOutputDevices, just return NO_ERROR
    sp<DeviceDescriptor> devDesc =
            mHwModules.getDeviceDescriptor(device, device_address, device_name, encodedFormat,
                                           state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE);
    if (audio_is_output_device(device)) {
        ssize_t index = mAvailableOutputDevices.indexOf(devDesc);
        if(state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
            if (index >= 0) {
                return true;
            }
        }
    }

    return false;
}

/*
 * set device for audio pass throught/bitstream (HDMI or spdif)
 * there is no interface for android to set bitstream device
 * we reusing setDeviceConnectionState to set the bitsteram device,
 * if device_name = "RK_BITSTREAM_DEVICE_NAME", then set the bitstream device
 */
status_t ATVAudioPolicyManager::setBitStreamDevice(audio_devices_t device,audio_policy_dev_state_t state,
                             const char *device_address,const char *device_name, audio_format_t encodedFormat)
{
    const char* setBitstreamDevice = "RK_BITSTREAM_DEVICE_NAME";
    if((device_name != NULL) && (strcmp(device_name,setBitstreamDevice) == 0)){
        audio_devices_t newDevice = mBitstreamDevice;
        if((device == AUDIO_DEVICE_OUT_AUX_DIGITAL) && (state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE)){
            newDevice = AUDIO_DEVICE_OUT_AUX_DIGITAL;
        }else if((device == AUDIO_DEVICE_OUT_SPDIF) && (state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE)){
            newDevice = AUDIO_DEVICE_OUT_SPDIF;
        }

        if((device == AUDIO_DEVICE_OUT_AUX_DIGITAL) || (device == AUDIO_DEVICE_OUT_SPDIF)){
            // if device is changed, the surround format need clear
            if(mBitstreamDevice != newDevice){
                mBitstreamDevice = newDevice;
            }
            bool already = isAlreadConnect(device,state,device_address,device_name, encodedFormat);
            if(already){
                return NO_ERROR;
            }

            return AudioPolicyManager::setDeviceConnectionState(device, state, device_address, device_name, encodedFormat);
        }
    }

    return -1;
}

status_t ATVAudioPolicyManager::setDeviceConnectionState(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address,
                                              const char *device_name,
                                              audio_format_t encodedFormat)

{
    ALOGD("ATVAudioPolicyManager::setDeviceConnectionState() device: 0x%X, state %d, address %s name %s format 0x%X",
            device, state, device_address, device_name, encodedFormat);

    if (!audio_is_output_device(device) && !audio_is_input_device(device)) return BAD_VALUE;

    if(setBitStreamDevice(device,state,device_address,device_name, encodedFormat) == NO_ERROR) {
        return NO_ERROR;
    }

    /*
     * some device may be set connect already(For example: HDMI/SPDIF set connect for audio bitstream/pass throught by AudioSetting),
     * so we just return NO_ERROR to tell AudioService or App this device is connect success
     */
    bool already = isAlreadConnect(device,state,device_address,device_name, encodedFormat);
    if(already){
        ALOGD("setDeviceConnectionState() device already connected: %x", device);
        return NO_ERROR;
    }

    return AudioPolicyManager::setDeviceConnectionState(device, state, device_address, device_name, encodedFormat);
}


/*
   String deviceAddress = "RK_BITSTREAM_DEVICE_ADDRESS";
   String deviceName = "RK_BITSTREAM_DEVICE_NAME";
*/
status_t ATVAudioPolicyManager::getOutputForAttr(const audio_attributes_t *attr,
                                              audio_io_handle_t *output,
                                              audio_session_t session,
                                              audio_stream_type_t *stream,
                                              uid_t uid,
                                              const audio_config_t *config,
                                              audio_output_flags_t *flags,
                                              audio_port_handle_t *selectedDeviceId,
                                              audio_port_handle_t *portId,
                                              std::vector<audio_io_handle_t> *secondaryOutputs,
                                              output_type_t *outputType) {
    if ((config->format == AUDIO_FORMAT_IEC61937) && (*flags == AUDIO_OUTPUT_FLAG_DIRECT)) {
        String8 address("RK_BITSTREAM_DEVICE_ADDRESS");
        ALOGD("%s : getDevice for mBitstreamDevice = 0x%x", __FUNCTION__,mBitstreamDevice);
        sp<DeviceDescriptor> device =
            mAvailableOutputDevices.getDevice(mBitstreamDevice, address, AUDIO_FORMAT_IEC61937);
        if (device != nullptr) {
            *selectedDeviceId = device->getId();
        } else{
            ALOGD("%s,%d, connot get selectedDeviceId",__FUNCTION__,__LINE__);
        }
    }

    return AudioPolicyManager::getOutputForAttr(attr, output, session, stream,uid,
           config, flags, selectedDeviceId, portId, secondaryOutputs, outputType);
}

}  // namespace android
