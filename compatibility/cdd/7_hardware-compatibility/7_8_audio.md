## 7.8\. Audio

### 7.8.1\. Microphone

If device implementations include a microphone, they:

*   [C-1-1] MUST report the `android.hardware.microphone` feature constant.
*   [C-1-2] MUST meet the audio recording requirements in
[section 5.4](#5_4_audio_recording).
*   [C-1-3] MUST meet the audio latency requirements in
[section 5.6](#5_6_audio_latency).
*   [SR] Are STRONGLY RECOMMENDED to support near-ultrasound recording as described
in [section 7.8.3](#7_8_3_near_ultrasound).

If device implementations omit a microphone, they:

*    [C-2-1] MUST NOT report the `android.hardware.microphone` feature constant.
*    [C-2-2] MUST implement the audio recording API at least as no-ops, per
     [section 7](#7_hardware_compatibility).


### 7.8.2\. Audio Output

If device implementations include a speaker or an audio/multimedia output
port for an audio output peripheral such as a 4 conductor 3.5mm audio jack or
USB host mode port using [USB audio class](
https://source.android.com/devices/audio/usb#audioClass), they:

*   [C-1-1] MUST report the `android.hardware.audio.output` feature constant.
*   [C-1-2] MUST meet the audio playback requirements in
[section 5.5](#5_5_audio_playback).
*   [C-1-3] MUST meet the audio latency requirements in
[section 5.6](#5_6_audio_latency).
*   [SR] STRONGLY RECOMMENDED to support near-ultrasound playback as described
in [section 7.8.3](#7_8_3_near_ultrasound).

If device implementations do not include a speaker or audio output port, they:

*   [C-2-1] MUST NOT report the `android.hardware.audio.output` feature.
*   [C-2-2] MUST implement the Audio Output related APIs as no-ops at least.


For the purposes of this section, an "output port" is a
[physical interface](https://en.wikipedia.org/wiki/Computer_port_%28hardware%29)
such as a 3.5mm audio jack, HDMI, or USB host mode port with USB audio class.
Support for audio output over radio-based protocols such as Bluetooth,
WiFi, or cellular network does not qualify as including an "output port".

#### 7.8.2.1\. Analog Audio Ports

In order to be compatible with the [headsets and other audio accessories](
https://source.android.com/devices/accessories/headset/plug-headset-spec)
using the 3.5mm audio plug across the Android ecosystem, if device
implementations include one or more analog audio ports, they:

*   [C-SR] Are STRONGLY RECOMMENDED to include at least one of the
audio port(s) to be a 4 conductor 3.5mm audio jack.

If device implementations have a 4 conductor 3.5mm audio jack, they:

*   [C-1-1] MUST support audio playback to stereo headphones and stereo headsets
with a microphone.
*   [C-1-2] MUST support TRRS audio plugs with the CTIA pin-out order.
*   [C-1-3] MUST support the detection and mapping to the keycodes for the
following 3 ranges of equivalent impedance between the microphone and ground
conductors on the audio plug:
    *   **70 ohm or less**: `KEYCODE_HEADSETHOOK`
    *   **210-290 ohm**: `KEYCODE_VOLUME_UP`
    *   **360-680 ohm**: `KEYCODE_VOLUME_DOWN`
*   [C-1-4] MUST trigger `ACTION_HEADSET_PLUG` upon a plug insert, but
only after all contacts on plug are touching their relevant segments
on the jack.
*   [C-1-5] MUST be capable of driving at least 150mV ± 10% of output voltage on
a 32 ohm speaker impedance.
*   [C-1-6] MUST have a microphone bias voltage between 1.8V ~ 2.9V.
*   [C-1-7] MUST detect and map to the keycode for the following
range of equivalent impedance between the microphone and ground conductors
on the audio plug:
    *   **110-180 ohm:** `KEYCODE_VOICE_ASSIST`
*   [C-SR] Are STRONGLY RECOMMENDED to support audio plugs with the OMTP
    pin-out order.
*   [C-SR] Are STRONGLY RECOMMEND to support audio recording from stereo
    headsets with a microphone.

If device implementations have a 4 conductor 3.5mm audio jack and support a
microphone, and broadcast the `android.intent.action.HEADSET_PLUG` with the
extra value microphone set as 1, they:

*   [C-2-1] MUST support the detection of microphone on the plugged in audio
accessory.

#### 7.8.2.2\. Digital Audio Ports

In order to be compatible with the headsets and other audio accessories using
USB-C connectors and implementing (USB audio class) across the Android ecosystem
as defined in [Android USB headset specification](https://source.android.com/devices/accessories/headset/usb-device).

See Section [2.2.1](#2_2_1_hardware) for device-specific requirements.

### 7.8.3\. Near-Ultrasound

Near-Ultrasound audio is the 18.5 kHz to 20 kHz band.

Device implementations:

*    MUST correctly report the support of
near-ultrasound audio capability via the [AudioManager.getProperty](
http://developer.android.com/reference/android/media/AudioManager.html#getProperty%28java.lang.String%29)
API as follows:

If [`PROPERTY_SUPPORT_MIC_NEAR_ULTRASOUND`](
http://developer.android.com/reference/android/media/AudioManager.html#PROPERTY_SUPPORT_MIC_NEAR_ULTRASOUND)
is "true", the following requirements MUST be met by the
`VOICE_RECOGNITION` and `UNPROCESSED` audio sources:

*    [C-1-1] The microphone's mean power response in the 18.5 kHz to 20 kHz band
     MUST be no more than 15 dB below the response at 2 kHz.
*    [C-1-2] The microphone's unweighted signal to noise ratio over 18.5 kHz to 20 kHz
     for a 19 kHz tone at -26 dBFS MUST be no lower than 50 dB.

If [`PROPERTY_SUPPORT_SPEAKER_NEAR_ULTRASOUND`](
http://developer.android.com/reference/android/media/AudioManager.html#PROPERTY_SUPPORT_SPEAKER_NEAR_ULTRASOUND)
is "true":

*    [C-2-1] The speaker's mean response in 18.5 kHz - 20 kHz MUST be no lower than 40 dB below the response at 2 kHz.

### 7.8.4\. Signal Integrity

Device implementations:
*   SHOULD provide a glitch-free audio signal path for both input
    and output streams on handheld devices, as defined by zero glitches
    measured during a test of one minute per path.
    Test using [OboeTester]
    (https://github.com/google/oboe/tree/master/apps/OboeTester)
    “Automated Glitch Test”.

The test requires an [audio loopback dongle]
(https://source.android.com/devices/audio/latency/loopback),
used directly in a 3.5mm jack, and/or in combination with a USB-C to 3.5mm adapter.
All audio output ports SHOULD be tested.

OboeTester currently supports AAudio paths, so the
following combinations SHOULD be tested for glitches using AAudio:

<table>
 <tr>
  <th>Perf Mode
  <th>Sharing
  <th>Out Sample Rate
  <th>In Chans
  <th>Out Chans
 </tr>
 <tr>
  <td>LOW_LATENCY</td>
  <td>EXCLUSIVE</td>
  <td>UNSPECIFIED</td>
  <td>1</td>
  <td>2</td>
 </tr>
 <tr>
  <td>LOW_LATENCY</td>
  <td>EXCLUSIVE</td>
  <td>UNSPECIFIED</td>
  <td>2</td>
  <td>1</td>
 </tr>
 <tr>
  <td>LOW_LATENCY</td>
  <td>SHARED</td>
  <td>UNSPECIFIED</td>
  <td>1</td>
  <td>2</td>
 </tr>
 <tr>
  <td>LOW_LATENCY</td>
  <td>SHARED</td>
  <td>UNSPECIFIED</td>
  <td>2</td>
  <td>1</td>
 </tr>
 <tr>
  <td>NONE</td>
  <td>SHARED</td>
  <td>48000</td>
  <td>1</td>
  <td>2</td>
 </tr>
 <tr>
  <td>NONE</td>
  <td>SHARED</td>
  <td>48000</td>
  <td>2</td>
  <td>1</td>
 </tr>
 <tr>
  <td>NONE</td>
  <td>SHARED</td>
  <td>44100</td>
  <td>1</td>
  <td>2</td>
 </tr>
 <tr>
  <td>NONE</td>
  <td>SHARED</td>
  <td>44100</td>
  <td>2</td>
  <td>1</td>
 </tr>
 <tr>
  <td>NONE</td>
  <td>SHARED</td>
  <td>16000</td>
  <td>1</td>
  <td>2</td>
 </tr>
 <tr>
  <td>NONE</td>
  <td>SHARED</td>
  <td>16000</td>
  <td>2</td>
  <td>1</td>
 </tr>
</table>

A reliable stream SHOULD meet the following criteria for Signal to Noise
Ratio (SNR) and Total Harmonic Distortion (THD) for 2000 Hz sine.

<table>
 <tr>
  <th>Transducer</th>
  <th>THD</th>
  <th>SNR</th>
 </tr>
 <tr>
  <td>primary built-in speaker, measured using an external reference microphone</td>
  <td>&lt; 3.0%</td>
  <td>&gt;= 50 dB</td>
 </tr>
 <tr>
  <td>primary built-in microphone, measured using an external reference speaker</td>
  <td>&lt; 3.0%</td>
  <td>&gt;= 50 dB</td>
 </tr>
 <tr>
  <td>built-in analog 3.5 mm jacks, tested using loopback adapter</td>
  <td>&lt; 1%</td>
  <td>&gt;= 60 dB</td>
 </tr>
 <tr>
  <td>USB adapters supplied with the phone, tested using loopback adapter</td>
  <td>&lt; 1.0%</td>
  <td>&gt;= 60 dB</td>
 </tr>
</table>
