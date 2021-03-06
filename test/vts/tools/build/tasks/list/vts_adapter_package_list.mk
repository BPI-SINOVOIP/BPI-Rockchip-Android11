#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

vts_adapter_package_list := \
  android.frameworks.displayservice@1.0-adapter \
  android.frameworks.displayservice@1.0-adapter-helper \
  android.frameworks.schedulerservice@1.0-adapter \
  android.frameworks.schedulerservice@1.0-adapter-helper \
  android.frameworks.sensorservice@1.0-adapter \
  android.frameworks.sensorservice@1.0-adapter-helper \
  android.frameworks.vr.composer@1.0-adapter \
  android.frameworks.vr.composer@1.0-adapter-helper \
  android.hardware.audio@2.0-adapter \
  android.hardware.audio@2.0-adapter-helper \
  android.hardware.audio@4.0-adapter \
  android.hardware.audio@4.0-adapter-helper \
  android.hardware.audio.common@2.0-adapter \
  android.hardware.audio.common@2.0-adapter-helper \
  android.hardware.audio.common@4.0-adapter \
  android.hardware.audio.common@4.0-adapter-helper \
  android.hardware.audio.effect@2.0-adapter \
  android.hardware.audio.effect@2.0-adapter-helper \
  android.hardware.audio.effect@4.0-adapter \
  android.hardware.audio.effect@4.0-adapter-helper \
  android.hardware.authsecret@1.0-adapter \
  android.hardware.authsecret@1.0-adapter-helper \
  android.hardware.automotive.audiocontrol@1.0-adapter \
  android.hardware.automotive.audiocontrol@1.0-adapter-helper \
  android.hardware.automotive.evs@1.0-adapter \
  android.hardware.automotive.evs@1.0-adapter-helper \
  android.hardware.automotive.vehicle@2.0-adapter \
  android.hardware.automotive.vehicle@2.0-adapter-helper \
  android.hardware.biometrics.fingerprint@2.1-adapter \
  android.hardware.biometrics.fingerprint@2.1-adapter-helper \
  android.hardware.bluetooth@1.0-adapter \
  android.hardware.bluetooth@1.0-adapter-helper \
  android.hardware.bluetooth.a2dp@1.0-adapter \
  android.hardware.bluetooth.a2dp@1.0-adapter-helper \
  android.hardware.boot@1.0-adapter \
  android.hardware.boot@1.0-adapter-helper \
  android.hardware.broadcastradio@1.0-adapter \
  android.hardware.broadcastradio@1.0-adapter-helper \
  android.hardware.broadcastradio@1.1-adapter \
  android.hardware.broadcastradio@1.1-adapter-helper \
  android.hardware.broadcastradio@2.0-adapter \
  android.hardware.broadcastradio@2.0-adapter-helper \
  android.hardware.camera.common@1.0-adapter \
  android.hardware.camera.common@1.0-adapter-helper \
  android.hardware.camera.device@1.0-adapter \
  android.hardware.camera.device@1.0-adapter-helper \
  android.hardware.camera.device@3.2-adapter \
  android.hardware.camera.device@3.2-adapter-helper \
  android.hardware.camera.device@3.3-adapter \
  android.hardware.camera.device@3.3-adapter-helper \
  android.hardware.camera.device@3.4-adapter \
  android.hardware.camera.device@3.4-adapter-helper \
  android.hardware.camera.metadata@3.2-adapter \
  android.hardware.camera.metadata@3.2-adapter-helper \
  android.hardware.camera.metadata@3.3-adapter \
  android.hardware.camera.metadata@3.3-adapter-helper \
  android.hardware.camera.provider@2.4-adapter \
  android.hardware.camera.provider@2.4-adapter-helper \
  android.hardware.cas@1.0-adapter \
  android.hardware.cas@1.0-adapter-helper \
  android.hardware.cas.native@1.0-adapter \
  android.hardware.cas.native@1.0-adapter-helper \
  android.hardware.configstore@1.0-adapter \
  android.hardware.configstore@1.0-adapter-helper \
  android.hardware.confirmationui@1.0-adapter \
  android.hardware.confirmationui@1.0-adapter-helper \
  android.hardware.contexthub@1.0-adapter \
  android.hardware.contexthub@1.0-adapter-helper \
  android.hardware.drm@1.0-adapter \
  android.hardware.drm@1.0-adapter-helper \
  android.hardware.drm@1.1-adapter \
  android.hardware.drm@1.1-adapter-helper \
  android.hardware.dumpstate@1.0-adapter \
  android.hardware.dumpstate@1.0-adapter-helper \
  android.hardware.gatekeeper@1.0-adapter \
  android.hardware.gatekeeper@1.0-adapter-helper \
  android.hardware.gnss@1.0-adapter \
  android.hardware.gnss@1.0-adapter-helper \
  android.hardware.gnss@1.1-adapter \
  android.hardware.gnss@1.1-adapter-helper \
  android.hardware.graphics.allocator@2.0-adapter \
  android.hardware.graphics.allocator@2.0-adapter-helper \
  android.hardware.graphics.bufferqueue@1.0-adapter \
  android.hardware.graphics.bufferqueue@1.0-adapter-helper \
  android.hardware.graphics.common@1.0-adapter \
  android.hardware.graphics.common@1.0-adapter-helper \
  android.hardware.graphics.common@1.1-adapter \
  android.hardware.graphics.common@1.1-adapter-helper \
  android.hardware.graphics.composer@2.1-adapter \
  android.hardware.graphics.composer@2.1-adapter-helper \
  android.hardware.graphics.composer@2.2-adapter \
  android.hardware.graphics.composer@2.2-adapter-helper \
  android.hardware.graphics.mapper@2.0-adapter \
  android.hardware.graphics.mapper@2.0-adapter-helper \
  android.hardware.graphics.mapper@2.1-adapter \
  android.hardware.graphics.mapper@2.1-adapter-helper \
  android.hardware.health@1.0-adapter \
  android.hardware.health@1.0-adapter-helper \
  android.hardware.health@2.0-adapter \
  android.hardware.health@2.0-adapter-helper \
  android.hardware.ir@1.0-adapter \
  android.hardware.ir@1.0-adapter-helper \
  android.hardware.keymaster@3.0-adapter \
  android.hardware.keymaster@3.0-adapter-helper \
  android.hardware.keymaster@4.0-adapter \
  android.hardware.keymaster@4.0-adapter-helper \
  android.hardware.light@2.0-adapter \
  android.hardware.light@2.0-adapter-helper \
  android.hardware.media@1.0-adapter \
  android.hardware.media@1.0-adapter-helper \
  android.hardware.media.bufferpool@1.0-adapter \
  android.hardware.media.bufferpool@1.0-adapter-helper \
  android.hardware.media.omx@1.0-adapter \
  android.hardware.media.omx@1.0-adapter-helper \
  android.hardware.memtrack@1.0-adapter \
  android.hardware.memtrack@1.0-adapter-helper \
  android.hardware.neuralnetworks@1.0-adapter \
  android.hardware.neuralnetworks@1.0-adapter-helper \
  android.hardware.neuralnetworks@1.1-adapter \
  android.hardware.neuralnetworks@1.1-adapter-helper \
  android.hardware.neuralnetworks@1.2-adapter \
  android.hardware.neuralnetworks@1.2-adapter-helper \
  android.hardware.nfc@1.0-adapter \
  android.hardware.nfc@1.0-adapter-helper \
  android.hardware.nfc@1.1-adapter \
  android.hardware.nfc@1.1-adapter-helper \
  android.hardware.oemlock@1.0-adapter \
  android.hardware.oemlock@1.0-adapter-helper \
  android.hardware.power@1.0-adapter \
  android.hardware.power@1.0-adapter-helper \
  android.hardware.power@1.1-adapter \
  android.hardware.power@1.1-adapter-helper \
  android.hardware.power@1.2-adapter \
  android.hardware.power@1.2-adapter-helper \
  android.hardware.radio@1.0-adapter \
  android.hardware.radio@1.0-adapter-helper \
  android.hardware.radio@1.1-adapter \
  android.hardware.radio@1.1-adapter-helper \
  android.hardware.radio@1.2-adapter \
  android.hardware.radio@1.2-adapter-helper \
  android.hardware.radio.config@1.0-adapter \
  android.hardware.radio.config@1.0-adapter-helper \
  android.hardware.radio.deprecated@1.0-adapter \
  android.hardware.radio.deprecated@1.0-adapter-helper \
  android.hardware.renderscript@1.0-adapter \
  android.hardware.renderscript@1.0-adapter-helper \
  android.hardware.secure_element@1.0-adapter \
  android.hardware.secure_element@1.0-adapter-helper \
  android.hardware.sensors@1.0-adapter \
  android.hardware.sensors@1.0-adapter-helper \
  android.hardware.soundtrigger@2.0-adapter \
  android.hardware.soundtrigger@2.0-adapter-helper \
  android.hardware.soundtrigger@2.1-adapter \
  android.hardware.soundtrigger@2.1-adapter-helper \
  android.hardware.soundtrigger@2.2-adapter \
  android.hardware.soundtrigger@2.2-adapter-helper \
  android.hardware.soundtrigger@2.3-adapter \
  android.hardware.soundtrigger@2.3-adapter-helper \
  android.hardware.tests.bar@1.0-adapter \
  android.hardware.tests.bar@1.0-adapter-helper \
  android.hardware.tests.baz@1.0-adapter \
  android.hardware.tests.baz@1.0-adapter-helper \
  android.hardware.tests.expression@1.0-adapter \
  android.hardware.tests.expression@1.0-adapter-helper \
  android.hardware.tests.extension.light@2.0-adapter \
  android.hardware.tests.extension.light@2.0-adapter-helper \
  android.hardware.tests.foo@1.0-adapter \
  android.hardware.tests.foo@1.0-adapter-helper \
  android.hardware.tests.hash@1.0-adapter \
  android.hardware.tests.hash@1.0-adapter-helper \
  android.hardware.tests.inheritance@1.0-adapter \
  android.hardware.tests.inheritance@1.0-adapter-helper \
  android.hardware.tests.libhwbinder@1.0-adapter \
  android.hardware.tests.libhwbinder@1.0-adapter-helper \
  android.hardware.tests.memory@1.0-adapter \
  android.hardware.tests.memory@1.0-adapter-helper \
  android.hardware.tests.msgq@1.0-adapter \
  android.hardware.tests.msgq@1.0-adapter-helper \
  android.hardware.tests.multithread@1.0-adapter \
  android.hardware.tests.multithread@1.0-adapter-helper \
  android.hardware.tests.pointer@1.0-adapter \
  android.hardware.tests.pointer@1.0-adapter-helper \
  android.hardware.tests.trie@1.0-adapter \
  android.hardware.tests.trie@1.0-adapter-helper \
  android.hardware.tetheroffload.config@1.0-adapter \
  android.hardware.tetheroffload.config@1.0-adapter-helper \
  android.hardware.tetheroffload.control@1.0-adapter \
  android.hardware.tetheroffload.control@1.0-adapter-helper \
  android.hardware.thermal@1.0-adapter \
  android.hardware.thermal@1.0-adapter-helper \
  android.hardware.thermal@1.1-adapter \
  android.hardware.thermal@1.1-adapter-helper \
  android.hardware.tv.cec@1.0-adapter \
  android.hardware.tv.cec@1.0-adapter-helper \
  android.hardware.tv.input@1.0-adapter \
  android.hardware.tv.input@1.0-adapter-helper \
  android.hardware.usb@1.0-adapter \
  android.hardware.usb@1.0-adapter-helper \
  android.hardware.usb@1.1-adapter \
  android.hardware.usb@1.1-adapter-helper \
  android.hardware.usb.gadget@1.0-adapter \
  android.hardware.usb.gadget@1.0-adapter-helper \
  android.hardware.vibrator@1.0-adapter \
  android.hardware.vibrator@1.0-adapter-helper \
  android.hardware.vibrator@1.1-adapter \
  android.hardware.vibrator@1.1-adapter-helper \
  android.hardware.vibrator@1.2-adapter \
  android.hardware.vibrator@1.2-adapter-helper \
  android.hardware.vibrator@1.3-adapter \
  android.hardware.vibrator@1.3-adapter-helper \
  android.hardware.vr@1.0-adapter \
  android.hardware.vr@1.0-adapter-helper \
  android.hardware.weaver@1.0-adapter \
  android.hardware.weaver@1.0-adapter-helper \
  android.hardware.wifi@1.0-adapter \
  android.hardware.wifi@1.0-adapter-helper \
  android.hardware.wifi@1.1-adapter \
  android.hardware.wifi@1.1-adapter-helper \
  android.hardware.wifi@1.2-adapter \
  android.hardware.wifi@1.2-adapter-helper \
  android.hardware.wifi.hostapd@1.0-adapter \
  android.hardware.wifi.hostapd@1.0-adapter-helper \
  android.hardware.wifi.offload@1.0-adapter \
  android.hardware.wifi.offload@1.0-adapter-helper \
  android.hardware.wifi.supplicant@1.0-adapter \
  android.hardware.wifi.supplicant@1.0-adapter-helper \
  android.hardware.wifi.supplicant@1.1-adapter \
  android.hardware.wifi.supplicant@1.1-adapter-helper \
  android.hardware.wifi.supplicant@1.2-adapter \
  android.hardware.wifi.supplicant@1.2-adapter-helper \
  android.hardware.wifi.supplicant@1.3-adapter \
  android.hardware.wifi.supplicant@1.3-adapter-helper \
  android.hidl.allocator@1.0-adapter \
  android.hidl.allocator@1.0-adapter-helper \
  android.hidl.base@1.0-adapter \
  android.hidl.base@1.0-adapter-helper \
  android.hidl.manager@1.0-adapter \
  android.hidl.manager@1.0-adapter-helper \
  android.hidl.manager@1.1-adapter \
  android.hidl.manager@1.1-adapter-helper \
  android.hidl.memory@1.0-adapter \
  android.hidl.memory@1.0-adapter-helper \
  android.hidl.token@1.0-adapter \
  android.hidl.token@1.0-adapter-helper \
  android.system.wifi.keystore@1.0-adapter \
  android.system.wifi.keystore@1.0-adapter-helper \
  tests.vendor@1.0-adapter \
  tests.vendor@1.0-adapter-helper \
  tests.vendor@1.1-adapter \
  tests.vendor@1.1-adapter-helper \
