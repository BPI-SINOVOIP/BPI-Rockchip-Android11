Copyright 2017 The Android Open Source Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
------------------------------------------------------------------

This directory contains models data for the Android Neural Networks API benchmarks.

Included models:

------------------------------------------------------------------
- mobilenet_v1_(0.25_128|0.5_160|0.75_192|1.0_224).tflite
MobileNet tensorflow lite model based on:
"MobileNets: Efficient Convolutional Neural Networks for Mobile Vision Applications"
https://arxiv.org/abs/1704.04861
Apache License, Version 2.0

Downloaded from
http://download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_${variant}.tgz
on Oct 5 2018 and converted using ToT toco.
Golden output generated with ToT tensorflow (Linux, CPU).

------------------------------------------------------------------
- mobilenet_v1_(0.25_128|0.5_160|0.75_192|1.0_224)_quant.tflite
8bit quantized MobileNet tensorflow lite model based on:
"MobileNets: Efficient Convolutional Neural Networks for Mobile Vision Applications"
https://arxiv.org/abs/1704.04861
Apache License, Version 2.0

Downloaded from
http://download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_${variant}_quant.tgz
on Oct 5 2018.
Golden output generated with ToT tflite (Linux, CPU).

------------------------------------------------------------------
- mobilenet_v2_(0.35_128|0.5_160|0.75_192|1.0_224).tflite
MobileNet v2 tensorflow lite model based on:
"MobileNetV2: Inverted Residuals and Linear Bottlenecks"
https://arxiv.org/abs/1801.04381
Apache License, Version 2.0

Downloaded from
https://storage.googleapis.com/mobilenet_v2/checkpoints/mobilenet_v2_${variant}.tgz
on Oct 16 2018 and converted using ToT toco.
Golden output generated with ToT tensorflow (Linux, CPU).

------------------------------------------------------------------
- mobilenet_v2_1.0_224_quant.tflite
8bit quantized MobileNet v2 tensorflow lite model based on:
"MobileNetV2: Inverted Residuals and Linear Bottlenecks"
https://arxiv.org/abs/1801.04381
Apache License, Version 2.0

Downloaded from
http://download.tensorflow.org/models/tflite_11_05_08/mobilenet_v2_1.0_224_quant.tgz
on Oct 30 2018.
Golden output generated with ToT tflite (Linux, CPU).

------------------------------------------------------------------
- ssd_mobilenet_v1_coco_float.tflite
Float version of MobileNet SSD tensorflow model based on:
"Speed/accuracy trade-offs for modern convolutional object detectors."
https://arxiv.org/abs/1611.10012
Apache License, Version 2.0

Generated from
http://download.tensorflow.org/models/object_detection/ssd_mobilenet_v1_coco_2018_01_28.tar.gz
on Sep 24 2018.
See also: https://github.com/tensorflow/models/tree/master/research/object_detection
Golden output generated with ToT tflite (Linux, x86_64 CPU).

------------------------------------------------------------------
- ssd_mobilenet_v1_coco_quantized.tflite
8bit quantized MobileNet SSD tensorflow lite model based on:
"Speed/accuracy trade-offs for modern convolutional object detectors."
https://arxiv.org/abs/1611.10012
Apache License, Version 2.0

Generated from
http://download.tensorflow.org/models/object_detection/ssd_mobilenet_v1_quantized_300x300_coco14_sync_2018_07_18.tar.gz
on Sep 19 2018.
See also: https://github.com/tensorflow/models/tree/master/research/object_detection
Golden output generated with ToT tflite (Linux, CPU).

------------------------------------------------------------------
- tts_float.tflite
TTS tensorflow lite model based on:
"Fast, Compact, and High Quality LSTM-RNN Based Statistical Parametric Speech Synthesizers for
Mobile Devices"
https://ai.google/research/pubs/pub45379
Apache License, Version 2.0

Note that the tensorflow lite model is the acoustic model in the paper. It is used because it is
much heavier than the duration model.
------------------------------------------------------------------
- asr_float.tflite
ASR tensorflow lite model based on the ASR acoustic model in:
"Personalized Speech recognition on mobile devices"
https://arxiv.org/abs/1603.03185
Apache License, Version 2.0

------------------------------------------------------------------
Input files:
------------------------------------------------------------------
- ssd_mobilenet_v1_coco_*/tarmac.input
Photo of airport tarmac by krtaylor@google.com, Apache License, Version 2.0
- cup_(128|160|192|224).input
Photo of cup by pszczepaniak@google.com, Apache License, Version 2.0
- banana_(128|160|192|224).input
Photo of banana by pszczepaniak@google.com, Apache License, Version 2.0
- tts_float/arctic_*.input
Linguistic features and durations generated from text sentences from the CMU Arctic set
(http://www.festvox.org/cmu_arctic/cmuarctic.data), Apache License, Version 2.0
- asr_float/*.input
Acoustic features generated from audio files from the LibriSpeech dataset
(http://www.openslr.org/12/), Creative Commons Attribution 4.0 International License
------------------------------------------------------------------

TODO(pszczepaniak): Provide at least 5 inputs outputs for each model

