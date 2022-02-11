1 概述
HwJpeg 库用于支持 Rockchip 平台 JPEG 硬编解码，是平台 MPP（Media Process Platform）
库 JPEG 编解码逻辑的封装。其中，MpiJpegEncoder 类封装了硬编码相关操作，MpiJpegDecoder
类封装了硬解码相关操作，用于支持图片或 MJPEG 码流解码。

工程包含主要目录：
- inc：HwJpeg 库头文件
- src：HwJpeg 库实现代码
- test：HwJpeg 测试实例

> 工程代码使用 mk 文件组织，在 Android SDK 环境下直接编译使用即可。

2 MpiJpegDecoder
MpiJpegDecoder 类是平台 JPEG 硬解码的封装，支持输入 JPG 图片及 MJPEG 码流，同时支持同步及
异步解码方式。

- decodePacket(char* data, size_t size, OutputFrame_t *aframeOut);
- decodeFile(const char *input_file, const char *output_file);

decodePacket & decodeFile 为同步解码方式，同步解码方式使用简单，阻塞等待解码输出，
OutputFrame_t 为解码输出封装，包含输出帧宽、高、物理地址、虚拟地址等信息。

- sendpacket(char* input_buf, size_t buf_len);
- getoutframe(OutputFrame_t *aframeOut);

sendpacket & getoutframe 用于配合实现异步解码输出，应用端处理开启两个线程，一
个线程送输入 sendpacket，另一个线程异步取输出 getoutframe。

**Note:**
1. HwJpeg 解码默认输出 RAW NV12 数据
2. 平台硬解码器只处理对齐过的 buffer，因此 MpiJpegDecoder 输出的 YUV buffer 经过 16 位对齐。
如果原始宽高非 16 位对齐，直接显示可能出现底部绿边等问题，MpiJpegDecoder 实现代码中 mOutputCrop
用于实现 OutFrame 的裁剪操作，可手动开启，也可以外部获取 OutFrame 句柄再进行相应的裁剪。
3. OutFrame buffer 在解码库内部循环使用，在解码显示完成之后使用 deinitOutputFrame 释放内存。
4. 默认情况下，输出 buffer 使用内部申请的 buffer 轮转池，但是也允许外部传递 dma buffer fd 用作
解码输出，额外传递的 dma buffer fd 由 decodePacket 函数参数 OutputFrame_t->outputPhyAddr 传入。

解码使用示例：
```
MpiJpegDecoder decoder;
MpiJpegDecoder::OutputFrame_t frameOut;

ret = decoder.prepareDecoder();
if (!ret) {
    ALOGE("failed to prepare JPEG decoder");
    goto DECODE_OUT;
}

memset(&frameOut, 0, sizeof(MpiJpegDecoder::OutputFrame_t));
ret = decoder.decodePacket(buf, size, &frameOut);
if (!ret) {
    ALOGE("failed to decode packet");
    goto DECODE_OUT;
}

decoder.deinitOutputFrame(&frameOut);
decoder.flushBuffer();
```

3 MpiJpegEncoder
MpiJpegEncoder 类是平台 JPEG 硬编码的封装，目前主要有三类接口提供：

- encodeFrame(char *data, OutputPacket_t *aPktOut);
- encodeFile(const char *input_file, const char *output_file);

encodeFrame & encodeFile 为同步阻塞编码方式，OutputPacket_t 为编码输出封装，
包含输出数据的内存地址信息。

- encode(EncInInfo *aInInfo, EncOutInfo *aOutInfo);

encode 输入数据类型为文件 fd，是为 cameraHal 设计的一套编码方式，输出 JPEG 图片
包含编码缩略图、APP1 EXIF 头信息等。

**Note:**
1. OutputPacket_t buffer 在编码库内部循环使用，在编码处理完成之后使用 deinitOutputPacket 释放内存

编码使用示例：
```
MpiJpegEncoder encoder;
MpiJpegEncoder::OutputPacket_t pktOut;

ret = encoder.prepareEncoder();
if (!ret) {
    ALOGE("failed to prepare JPEG encoder");
    goto ENCODE_OUT;
}

ret = encoder.updateEncodeCfg(720 /*width*/, 1080 /*height*/,
                              MpiJpegEncoder::INPUT_FMT_YUV420SP);
if (!ret) {
    ALOGE("failed to update encode config");
    goto ENCODE_OUT;
}

memset(&pktOut, 0, sizeof(MpiJpegEncoder::OutputPacket_t));
ret = encoder.encodeFrame(buf, &pktOut);
if (!ret) {
    ALOGE("failed to encode packet");
    goto ENCODE_OUT;
}

/* TODO - Get diaplay for the PacketOut.
   * - Pakcet address: pktOut.data
   * - Pakcet size: pktOut.size */

/* Output frame buffers within limits, so release frame buffer if one
   frame has been display successful. */
encoder.deinitOutputPacket(&pktOut);
```
