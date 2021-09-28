/*
 *
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

#ifndef RK_ANDROID_ATV_AUDIO_POLICY_MANAGER_H
#define RK_ANDROID_ATV_AUDIO_POLICY_MANAGER_H

#include <managerdefault/AudioPolicyManager.h>
namespace android {

/*
 * AudioPolicyManager of ATV/BOX
 */
class ATVAudioPolicyManager: public AudioPolicyManager
{
public:
    ATVAudioPolicyManager(AudioPolicyClientInterface *clientInterface);
    virtual ~ATVAudioPolicyManager() {}

     // AudioPolicyInterface
    virtual status_t initialize();
    virtual status_t setDeviceConnectionState(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address,
                                              const char *device_name,
                                              audio_format_t encodedFormat);
 
    virtual status_t getOutputForAttr(const audio_attributes_t *attr,
                                              audio_io_handle_t *output,
                                              audio_session_t session,
                                              audio_stream_type_t *stream,
                                              uid_t uid,
                                              const audio_config_t *config,
                                              audio_output_flags_t *flags,
                                              audio_port_handle_t *selectedDeviceId,
                                              audio_port_handle_t *portId,
                                              std::vector<audio_io_handle_t> *secondaryOutputs,
                                              output_type_t *outputType);

protected:
    bool isAlreadConnect(audio_devices_t device,audio_policy_dev_state_t state,
                                                const char *device_address,
                                                const char *device_name,
                                                audio_format_t encodedFormat);

    virtual status_t setBitStreamDevice(audio_devices_t device,audio_policy_dev_state_t state,
                             const char *device_address,const char *device_name, audio_format_t encodedFormat);
private:
    audio_devices_t  mBitstreamDevice;
};

}// namespace android
#endif  // ATV_ANDROID_AUDIO_POLICY_MANAGER_H
