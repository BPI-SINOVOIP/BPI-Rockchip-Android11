## Media V2 CTS Tests
Current folder comprises of files necessary for testing media extractor, media muxer, media codec SDK and NDK Api.  These test aims to test all codecs advertised in MediaCodecList, available muxers and extractors. The aim of these tests is not verify the CDD requirements but to test components, their plugins and their interactions with media framework. The test vectors used by the test suite is available at [link](https://storage.googleapis.com/android_media/cts/tests/media/CtsMediaV2TestCases.zip) and is downloaded automatically while running tests. The test suite looks to cover sdk/ndk api in normal and error scenarios. Error scenarios are seperated from regular usage and are placed under class *UnitTest (MuxerUnitTest, ExtractorUnitTest, ...).

### Commands
```sh
$ atest android.mediav2.cts
$ atest android.mediav2.cts.CodecEncoderTest android.mediav2.cts.CodecDecoderTest
$ atest android.mediav2.cts.MuxerTest android.mediav2.cts.MuxerUnitTest
$ atest android.mediav2.cts.ExtractorTest android.mediav2.cts.ExtractorUnitTest
```

### Features
All tests accepts attributes that offer selective run of tests. Media codec tests parses the value of key *codec-sel* to determine the list of components on which the tests are to be tried. Similarly for Media extractor and media muxer parses the value of keys *ext-sel* and *mux-sel* to determine the list of components on which the tests are to be tried.

To limit media codec decoder tests to mp3 and vorbis decoder,
```sh
adb shell am instrument -w -r  -e codec-sel 'mp3;vorbis'  -e debug false -e class 'android.mediav2.cts.CodecDecoderTest' android.mediav2.cts.test/androidx.test.runner.AndroidJUnitRunner
```
### Appendix
| Identifier for codec-sel | Mime |
| ------ | ------ |
|default|all|
|vp8|mimetype_video_vp8|
|vp9|mimetype_video_vp9|
|av1|mimetype_video_av1|
|avc|mimetype_video_avc|
|hevc|mimetype_video_hevc|
|mpeg4|mimetype_video_mpeg4|
|h263|mimetype_video_h263|
|mpeg2|mimetype_video_mpeg2|
|vraw|mimetype_video_raw|
|amrnb|mimetype_audio_amr_nb|
|amrwb|mimetype_audio_amr_wb|
|mp3|mimetype_audio_mpeg|
|aac|mimetype_audio_aac|
|vorbis|mimetype_audio_vorbis|
|opus|mimetype_audio_opus|
|g711alaw|mimetype_audio_g711_alaw|
|g711mlaw|mimetype_audio_g711_mlaw|
|araw|mimetype_audio_raw|
|flac|mimetype_audio_flac|
|gsm|mimetype_audio_msgsm|


| Identifier for ext-sel | Extractor format |
| ------ | ------ |
|mp4|Mpeg4|
|webm|Matroska|
|3gp|Mpeg4|
|mkv|Matroska|
|ogg|Ogg|


| Identifier for mux-sel | Muxer Format |
| ------ | ------ |
|mp4|muxer_output_mpeg4|
|webm|muxer_output_webm|
|3gp|muxer_output_3gpp|
|ogg|muxer_output_ogg|
