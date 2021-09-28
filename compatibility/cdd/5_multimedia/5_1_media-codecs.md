## 5.1\. Media Codecs

### 5.1.1\. Audio Encoding

See more details in [5.1.3. Audio Codecs Details](#5_1_3_audio_codecs_details).

If device implementations declare `android.hardware.microphone`,
they MUST support encoding the following audio formats and make them available
to third-party apps:

*    [C-1-1] PCM/WAVE
*    [C-1-2] FLAC
*    [C-1-3] Opus

All audio encoders MUST support:

*  [C-3-1] PCM 16-bit native byte order audio frames via the [`android.media.MediaCodec`](
https://developer.android.com/reference/android/media/MediaCodec.html#raw-audio-buffers)
API.


### 5.1.2\. Audio Decoding

See more details in [5.1.3. Audio Codecs Details](#5_1_3_audio_codecs_details).


If device implementations declare support for the
`android.hardware.audio.output` feature, they must support decoding the
following audio formats:

*    [C-1-1] MPEG-4 AAC Profile (AAC LC)
*    [C-1-2] MPEG-4 HE AAC Profile (AAC+)
*    [C-1-3] MPEG-4 HE AACv2 Profile (enhanced AAC+)
*    [C-1-4] AAC ELD (enhanced low delay AAC)
*    [C-1-11] xHE-AAC (ISO/IEC 23003-3 Extended HE AAC Profile, which includes
             the USAC Baseline Profile, and ISO/IEC 23003-4 Dynamic Range
             Control Profile)
*    [C-1-5] FLAC
*    [C-1-6] MP3
*    [C-1-7] MIDI
*    [C-1-8] Vorbis
*    [C-1-9] PCM/WAVE including high-resolution audio
formats up to 24 bits, 192 kHz sample rate, and 8 channels.
Note that this requirement is for decoding only, and that a device
is permitted to downsample and downmix during the playback phase.
*    [C-1-10] Opus

If device implementations support the decoding of AAC input buffers of
multichannel streams (i.e. more than two channels) to PCM through the default
AAC audio decoder in the `android.media.MediaCodec` API, the following MUST be
supported:

*    [C-2-1] Decoding MUST be performed without downmixing (e.g. a 5.0 AAC
stream must be decoded to five channels of PCM, a 5.1 AAC stream must be decoded
to six channels of PCM).
*    [C-2-2] Dynamic range metadata MUST be as defined in "Dynamic Range Control
(DRC)" in ISO/IEC 14496-3, and the `android.media.MediaFormat` DRC keys to
configure the dynamic range-related behaviors of the audio decoder. The
AAC DRC keys were introduced in API 21, and are:
`KEY_AAC_DRC_ATTENUATION_FACTOR`, `KEY_AAC_DRC_BOOST_FACTOR`,
`KEY_AAC_DRC_HEAVY_COMPRESSION`, `KEY_AAC_DRC_TARGET_REFERENCE_LEVEL` and
`KEY_AAC_ENCODED_TARGET_LEVEL`.
*    [SR] It is STRONGLY RECOMMENDED that requirements C-2-1 and C-2-2 above are
satisfied by all AAC audio decoders.

When decoding USAC audio, MPEG-D (ISO/IEC 23003-4):

*    [C-3-1] Loudness and DRC metadata MUST be interpreted and applied
according to MPEG-D DRC Dynamic Range Control Profile Level 1.
*    [C-3-2] The decoder MUST behave according to the configuration
set with the following `android.media.MediaFormat` keys:
`KEY_AAC_DRC_TARGET_REFERENCE_LEVEL` and `KEY_AAC_DRC_EFFECT_TYPE`.

MPEG-4 AAC, HE AAC, and HE AACv2 profile decoders:

*    MAY support loudness and dynamic range control using ISO/IEC 23003-4
Dynamic Range Control Profile.

If ISO/IEC 23003-4 is supported and if both ISO/IEC 23003-4 and
ISO/IEC 14496-3 metadata are present in a decoded bitstream, then:

*    ISO/IEC 23003-4 metadata SHALL take precedence.

All audio decoders MUST support outputting:

*  [C-6-1] PCM 16-bit native byte order audio frames via the [`android.media.MediaCodec`](
https://developer.android.com/reference/android/media/MediaCodec.html#raw-audio-buffers)
API.

### 5.1.3\. Audio Codecs Details

<table>
 <tr>
    <th>Format/Codec</th>
    <th>Details</th>
    <th>File Types/Container Formats to be supported</th>
 </tr>
 <tr>
    <td>MPEG-4 AAC Profile<br />(AAC LC)</td>
    <td>Support for mono/stereo/5.0/5.1 content with standard
    sampling rates from 8 to 48 kHz.</td>
    <td>
    <ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a)</li>
    <li class="table_list">ADTS raw AAC (.aac, ADIF not supported)</li>
    <li class="table_list">MPEG-TS (.ts, not seekable, decode only)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul>
    </td>
 </tr>
 <tr>
    <td>MPEG-4 HE AAC Profile (AAC+)</td>
    <td>Support for mono/stereo/5.0/5.1 content with standard
    sampling rates from 16 to 48 kHz.</td>
    <td>
    <ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a)</li></ul>
    </td>
 </tr>
 <tr>
    <td>MPEG-4 HE AACv2<br />

Profile (enhanced AAC+)</td>
    <td>Support for mono/stereo/5.0/5.1 content with standard
    sampling rates from 16 to 48 kHz.</td>
    <td>
    <ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a)</li></ul>
    </td>
 </tr>
 <tr>
    <td>AAC ELD (enhanced low delay AAC)</td>
    <td>Support for mono/stereo content with standard sampling rates from 16 to
    48 kHz.</td>
    <td>
    <ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a)</li></ul>
    </td>
 </tr>
 <tr>
    <td>USAC</td>
    <td>Support for mono/stereo content with standard sampling rates from 7.35
    to 48 kHz.</td>
    <td>
    MPEG-4 (.mp4, .m4a)
    </td>
 </tr>
 <tr>
    <td>AMR-NB</td>
    <td>4.75 to 12.2 kbps sampled @ 8 kHz</td>
    <td>3GPP (.3gp)</td>
 </tr>
 <tr>
    <td>AMR-WB</td>
    <td>9 rates from 6.60 kbit/s to 23.85 kbit/s sampled @ 16 kHz, as defined at
      <a href="https://www.loc.gov/preservation/digital/formats/fdd/fdd000255.shtml">
      AMR-WB, Adaptive Multi-Rate - Wideband Speech Codec</a></td>
    <td>3GPP (.3gp)</td>
 </tr>
 <tr>
    <td>FLAC</td>
    <td>For both encoder and decoder: at least Mono and Stereo modes MUST be
      supported. Sample rates up to 192 kHz MUST be supported; 16-bit and 24-bit
      resolution MUST be supported. FLAC 24-bit audio data handling MUST be
      available with floating point audio configuration.</td>
    <td>
    <ul>
    <li class="table_list">FLAC (.flac)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a, decode only)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul>
    </td>
 </tr>
 <tr>
    <td>MP3</td>
    <td>Mono/Stereo 8-320Kbps constant (CBR) or variable bitrate (VBR)</td>
    <td>
    <ul>
    <li class="table_list">MP3 (.mp3)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a, decode only)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></td>
 </tr>
 <tr>
    <td>MIDI</td>
    <td>MIDI Type 0 and 1. DLS Version 1 and 2. XMF and Mobile XMF. Support for
    ringtone formats RTTTL/RTX, OTA, and iMelody</td>
    <td><ul>
    <li class="table_list">Type 0 and 1 (.mid, .xmf, .mxmf)</li>
    <li class="table_list">RTTTL/RTX (.rtttl, .rtx)</li>
    <li class="table_list">OTA (.ota)</li>
    <li class="table_list">iMelody (.imy)</li></ul></td>
 </tr>
 <tr>
    <td>Vorbis</td>
    <td></td>
    <td>
    <ul>
    <li class="table_list">Ogg (.ogg)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a, decode only)</li>
    <li class="table_list">Matroska (.mkv)</li>
    <li class="table_list">Webm (.webm)</li></ul></td>
 </tr>
 <tr>
    <td>PCM/WAVE</td>
    <td>PCM codec MUST support 16-bit linear PCM and 16-bit float. WAVE
      extractor must support 16-bit, 24-bit, 32-bit linear PCM and 32-bit float
      (rates up to limit of hardware). Sampling rates MUST be supported from
      8 kHz to 192 kHz.</td>
    <td>WAVE (.wav)</td>
 </tr>
 <tr>
    <td>Opus</td>
    <td>Decoding: Support for mono, stereo, 5.0 and 5.1 content
      with sampling rates of 8000, 12000, 16000, 24000, and 48000 Hz.
      <br>
      Encoding: Support for mono and stereo content
      with sampling rates of 8000, 12000, 16000, 24000, and 48000 Hz.
    </td>
    <td>
    <ul>
    <li class="table_list">Ogg (.ogg)</li>
    <li class="table_list">MPEG-4 (.mp4, .m4a, decode only)</li>
    <li class="table_list">Matroska (.mkv)</li>
    <li class="table_list">Webm (.webm)</li></ul></td>
 </tr>
</table>

### 5.1.4\. Image Encoding

See more details in [5.1.6. Image Codecs Details](#5_1_6_image_codecs_details).

Device implementations MUST support encoding the following image encoding:

*    [C-0-1] JPEG
*    [C-0-2] PNG
*    [C-0-3] WebP

If device implementations support HEIC encoding via `android.media.MediaCodec`
for media type [`MIMETYPE_IMAGE_ANDROID_HEIC`](
https://developer.android.com/reference/android/media/MediaFormat.html#MIMETYPE_IMAGE_ANDROID_HEIC),
they:

*    [C-1-1] MUST provide a hardware-accelerated HEVC encoder codec that
supports [`BITRATE_MODE_CQ`](https://developer.android.com/reference/android/media/MediaCodecInfo.EncoderCapabilities.html#BITRATE_MODE_CQ)
bitrate control mode, [`HEVCProfileMainStill`](
https://developer.android.com/reference/android/media/MediaCodecInfo.CodecProfileLevel.html#HEVCProfileMainStill)
profile and 512 x 512 px frame size.

### 5.1.5\. Image Decoding

See more details in [5.1.6. Image Codecs Details](#5_1_6_image_codecs_details).

Device implementations MUST support decoding the following image encoding:

*    [C-0-1] JPEG
*    [C-0-2] GIF
*    [C-0-3] PNG
*    [C-0-4] BMP
*    [C-0-5] WebP
*    [C-0-6] Raw
*    [C-0-7] HEIF (HEIC)

Image decoders that support a high bit-depth format (9+ bits per channel)

*   [C-1-1] MUST support outputting an 8-bit equivalent format if requested by
the application, for example, via the [`ARGB_8888`](
https://developer.android.com/reference/android/graphics/Bitmap.Config.html#ARGB_8888)
config of `android.graphics.Bitmap`.

### 5.1.6\. Image Codecs Details

<table>
 <tr>
    <th>Format/Codec</th>
    <th>Details</th>
    <th>Supported File Types/Container Formats</th>
 </tr>
 <tr>
    <td>JPEG</td>
    <td>Base+progressive</td>
    <td>JPEG (.jpg)</td>
 </tr>
 <tr>
    <td>GIF</td>
    <td></td>
    <td>GIF (.gif)</td>
 </tr>
 <tr>
    <td>PNG</td>
    <td></td>
    <td>PNG (.png)</td>
 </tr>
 <tr>
    <td>BMP</td>
    <td></td>
    <td>BMP (.bmp)</td>
 </tr>
 <tr>
    <td>WebP</td>
    <td></td>
    <td>WebP (.webp)</td>
 </tr>
 <tr>
    <td>Raw</td>
    <td></td>
    <td>ARW (.arw), CR2 (.cr2), DNG (.dng), NEF (.nef), NRW (.nrw), ORF (.orf),
        PEF (.pef), RAF (.raf), RW2 (.rw2), SRW (.srw)</td>
 </tr>
 <tr>
    <td>HEIF</td>
    <td>Image, Image collection, Image sequence</td>
    <td>HEIF (.heif), HEIC (.heic)</td>
 </tr>
</table>

Image encoder and decoders exposed through the [MediaCodec](
https://developer.android.com/reference/android/media/MediaCodec) API

*   [C-1-1] MUST support YUV420 8:8:8 flexible color
format (`COLOR_FormatYUV420Flexible`) through [`CodecCapabilities`](
https://developer.android.com/reference/android/media/MediaCodecInfo.CodecCapabilities).

*   [SR] STRONGLY RECOMMENDED to support RGB888 color format for input Surface
mode.

*   [C-1-3] MUST support at least one of a planar or semiplanar
YUV420 8:8:8 color format: `COLOR_FormatYUV420PackedPlanar` (equivalent to
`COLOR_FormatYUV420Planar`) or `COLOR_FormatYUV420PackedSemiPlanar` (equivalent
to `COLOR_FormatYUV420SemiPlanar`). They are STRONGLY RECOMMENDED to support
both.

### 5.1.7\. Video Codecs

*   For acceptable quality of web video streaming and video-conference
services, device implementations SHOULD use a hardware VP8 codec that meets the
[requirements](http://www.webmproject.org/hardware/rtc-coding-requirements/).

If device implementations include a video decoder or encoder:

*   [C-1-1] Video codecs MUST support output and input bytebuffer sizes that
accommodate the largest feasible compressed and uncompressed frame as dictated
by the standard and configuration but also not overallocate.

*   [C-1-2] Video encoders and decoders MUST support YUV420 8:8:8 flexible color
formats (`COLOR_FormatYUV420Flexible`) through [`CodecCapabilities`](
https://developer.android.com/reference/android/media/MediaCodecInfo.CodecCapabilities).

*   [C-1-3] Video encoders and decoders MUST support at least one of a planar or
semiplanar YUV420 8:8:8 color format: `COLOR_FormatYUV420PackedPlanar`
(equivalent to `COLOR_FormatYUV420Planar`) or
`COLOR_FormatYUV420PackedSemiPlanar` (equivalent to `COLOR_FormatYUV420SemiPlanar`).
They are STRONGLY RECOMMENDED to support both.

*   [SR] Video encoders and decoders are STRONGLY RECOMMENDED to support
at least one of a hardware optimized planar or semiplanar YUV420 8:8:8 color
format (YV12, NV12, NV21 or equivalent vendor optimized format.)

*   [C-1-5] Video decoders that support a high bit-depth format
(9+ bits per channel) MUST support outputting an 8-bit equivalent format if
requested by the application. This MUST be reflected by supporting an
YUV420 8:8:8 color format via `android.media.MediaCodecInfo`.

If device implementations advertise HDR profile support through
[`Display.HdrCapabilities`](
https://developer.android.com/reference/android/view/Display.HdrCapabilities.html),
they:

*   [C-2-1] MUST support HDR static metadata parsing and handling.

If device implementations advertise intra refresh support through
`FEATURE_IntraRefresh` in the [`MediaCodecInfo.CodecCapabilities`](
https://developer.android.com/reference/android/media/MediaCodecInfo.CodecCapabilities.html#FEATURE_IntraRefresh)
class, they:

*   [C-3-1] MUST support the refresh periods in the range of 10 - 60 frames and
accurately operate within 20% of configured refresh period.

Unless the application specifies otherwise using the [`KEY_COLOR_FORMAT`](
https://developer.android.com/reference/android/media/MediaFormat.html#KEY_COLOR_FORMAT)
format key, video decoder implementations:

*   [C-4-1] MUST default to the color format optimized for hardware display
if configured using Surface output.
*   [C-4-2] MUST default to a YUV420 8:8:8 color format optimized for CPU
reading if configured to not use Surface output.

### 5.1.8\. Video Codecs List


<table>
 <tr>
    <th>Format/Codec</th>
    <th>Details</th>
    <th>File Types/Container Formats to be supported</th>
 </tr>
 <tr>
    <td>H.263</td>
    <td></td>
    <td><ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul></td>
 </tr>
 <tr>
    <td>H.264 AVC</td>
    <td>See <a href="#5_2_video_encoding">section 5.2 </a>and
    <a href="#5_3_video_decoding">5.3</a> for details</td>
    <td><ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4)</li>
    <li class="table_list">MPEG-2 TS (.ts, not seekable)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul></td>
 </tr>
 <tr>
    <td>H.265 HEVC</td>
    <td>See <a href="#5_3_video_decoding">section 5.3</a> for details</td>
    <td><ul>
    <li class="table_list">MPEG-4 (.mp4)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul></td>
 </tr>
 <tr>
    <td>MPEG-2</td>
    <td>Main Profile</td>
    <td><ul>
    <li class="table_list">MPEG2-TS (.ts, not seekable)</li>
    <li class="table_list">MPEG-4 (.mp4, decode only)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul></td>
 </tr>
 <tr>
    <td>MPEG-4 SP</td>
    <td></td>
    <td><ul>
    <li class="table_list">3GPP (.3gp)</li>
    <li class="table_list">MPEG-4 (.mp4)</li>
    <li class="table_list">Matroska (.mkv, decode only)</li></ul></td>
 </tr>
 <tr>
    <td>VP8</td>
    <td>See <a href="#5_2_video_encoding">section 5.2</a> and
    <a href="#5_3_video_decoding">5.3</a> for details</td>
    <td><ul>
    <li class="table_list"><a href="http://www.webmproject.org/">WebM
    (.webm)</a></li>
    <li class="table_list">Matroska (.mkv)</li></ul>
    </td>
 </tr>
 <tr>
    <td>VP9</td>
    <td>See <a href="#5_3_video_decoding">section 5.3</a> for details</td>
    <td><ul>
    <li class="table_list"><a href="http://www.webmproject.org/">WebM
    (.webm)</a></li>
    <li class="table_list">Matroska (.mkv)</li></ul>
    </td>
 </tr>
</table>


### 5.1.9\. Media Codec Security

Device implementations MUST ensure compliance with media codec security features
as described below.

Android includes support for OMX, a cross-platform multimedia acceleration API,
as well as Codec 2.0, a low-overhead multimedia acceleration API.

If device implementations support multimedia, they:

*   [C-1-1] MUST provide support for media codecs either via OMX or Codec 2.0
APIs (or both) as in the Android Open Source Project and not disable or
circumvent the security protections. This specifically does not mean that every
codec MUST use either the OMX or Codec 2.0 API, only that support for at least
one of these APIs MUST be available, and support for the available APIs MUST
include the security protections present.
*   [C-SR] Are STRONGLY RECOMMENDED to include support for Codec 2.0 API.

If device implementations do not support the Codec 2.0 API, they:

*   [C-2-1] MUST include the corresponding OMX software codec from the Android
Open Source Project (if it is available) for each media format and type
(encoder or decoder) supported by the device.
*   [C-2-2] Codecs that have names starting with "OMX.google." MUST be based
on their Android Open Source Project source code.
*   [C-SR]  Are STRONGLY RECOMMENDED that the OMX software codecs run in a codec
process that does not have access to hardware drivers other than memory mappers.

If device implementations support Codec 2.0 API, they:

*   [C-3-1] MUST include the corresponding Codec 2.0 software codec from the
Android Open Source Project (if it is available) for each media format and type
(encoder or decoder) supported by the device.
*   [C-3-2] MUST house the Codec 2.0 software codecs in the software codec
process as provided in the Android Open Source Project to make it possible
to more narrowly grant access to software codecs.
*   [C-3-3] Codecs that have names starting with "c2.android." MUST be based
on their Android Open Source Project source code.

### 5.1.10\. Media Codec Characterization

If device implementations support media codecs, they:

*  [C-1-1] MUST return correct values of media codec characterization via the
   [`MediaCodecInfo`](
   https://developer.android.com/reference/android/media/MediaCodecInfo)
   API.

In particular:

*  [C-1-2] Codecs with names starting with "OMX." MUST use the OMX APIs
and have names that conform to OMX IL naming guidelines.
*  [C-1-3] Codecs with names starting with "c2." MUST use the Codec 2.0 API and
have names that conform to Codec 2.0 naming guidelines for Android.
*  [C-1-4] Codecs with names starting with "OMX.google." or "c2.android." MUST
NOT be characterized as vendor or as hardware-accelerated.
*  [C-1-5] Codecs that run in a codec process (vendor or system) that have
access to hardware drivers other than memory allocators and mappers MUST NOT
be characterized as software-only.
*  [C-1-6] Codecs not present in the Android Open Source Project or not based
on the source code in that project MUST be characterized as vendor.
*  [C-1-7] Codecs that utilize hardware acceleration MUST be characterized
as hardware accelerated.
*  [C-1-8] Codec names MUST NOT be misleading. For example, codecs named
"decoders" MUST support decoding, and those named "encoders" MUST support
encoding. Codecs with names containing media formats MUST support those
formats.

If device implementations support video codecs:

*  [C-2-1] All video codecs MUST publish achievable frame rate data for the
following sizes if supported by the codec:

<table>
  <tr>
    <th></th>
    <th>SD (low quality)</th>
    <th>SD (high quality)</th>
    <th>HD 720p</th>
    <th>HD 1080p</th>
    <th>UHD</th>
  </tr>
  <tr>
    <th>Video resolution</th>
    <td><ul>
    <li class="table_list">176 x 144 px (H263, MPEG2, MPEG4)</li>
    <li class="table_list">352 x 288 px (MPEG4 encoder, H263, MPEG2)</li>
    <li class="table_list">320 x 180 px (VP8, VP8)</li>
    <li class="table_list">320 x 240 px (other)</li></ul>
    </td>
    <td><ul>
    <li class="table_list">704 x 576 px (H263)</li>
    <li class="table_list">640 x 360 px (VP8, VP9)</li>
    <li class="table_list">640 x 480 px (MPEG4 encoder)</li>
    <li class="table_list">720 x 480 px (other)</li></ul>
    </td>
    <td><ul>
    <li class="table_list">1408 x 1152 px (H263)</li>
    <li class="table_list">1280 x 720 px (other)</li></ul>
    </td>
    <td>1920 x 1080 px (other than MPEG4)</td>
    <td>3840 x 2160 px (HEVC, VP9)</td>
  </tr>
</table>

*  [C-2-2] Video codecs that are characterized as hardware accelerated MUST
publish performance points information. They MUST each list all supported
standard performance points (listed in [`PerformancePoint`](
https://developer.android.com/reference/android/media/MediaCodecInfo.VideoCapabilities)
API), unless they are covered by another supported standard performance point.
*  Additionally they SHOULD publish extended performance points if they
support sustained video performance other than one of the standard ones listed.
