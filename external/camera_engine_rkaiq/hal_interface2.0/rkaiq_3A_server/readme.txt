1、rkaiq_3A_server文件说明：
	rkaiq_3A_server是直接单独运行librkisp库的可执行程序.
	它不调用CameraHal库，而是直接单独运行librkisp库（即3A自动运行）
	然后就可以通过v4l2-ctl或者其他应用获取3A处理后的NV12数据了。

2、使用说明：
1）首先确定数据流链路都已经配置好
可以使用命令：media-ctl -d /dev/mediax -p 查看；
例如：如下是rk1126 的数据链路情况
rk1126_r:/ # media-ctl -d /dev/media0 -p
Opening media device /dev/media0
Enumerating entities
Found 18 entities
Enumerating pads and links
Media controller API version 0.0.149

Media device information
------------------------
driver          rkisp
model           rkisp0
serial
bus info
hw revision     0x0
driver version  0.0.149

Device topology
- entity 1: rkisp-isp-subdev (4 pads, 10 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev1
        pad0: Sink
                [fmt:SBGGR10/2688x1520
                 crop.bounds:(0,0)/2688x1520
                 crop:(0,0)/2688x1520]
                <- "rkisp-csi-subdev":1 [ENABLED]
                <- "rkisp_rawrd0_m":0 [ENABLED]
                <- "rkisp_rawrd1_l":0 [ENABLED]
                <- "rkisp_rawrd2_s":0 [ENABLED]
        pad1: Sink
                <- "rkisp-input-params":0 [ENABLED]
        pad2: Source
                [fmt:YUYV2X8/2688x1520
                 crop.bounds:(0,0)/2688x1520
                 crop:(0,0)/2688x1520]
                -> "rkisp-bridge-ispp":0 [ENABLED]
                -> "rkisp_mainpath":0 []
                -> "rkisp_selfpath":0 []
        pad3: Source
                -> "rkisp-statistics":0 [ENABLED]
                -> "rkisp-mipi-luma":0 [ENABLED]

- entity 6: rkisp-csi-subdev (6 pads, 6 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev2
        pad0: Sink
                [fmt:SBGGR10/2688x1520]
                <- "rockchip-mipi-dphy-rx":1 [ENABLED]
        pad1: Source
                [fmt:SBGGR10/2688x1520]
                -> "rkisp-isp-subdev":0 [ENABLED]
        pad2: Source
                [fmt:SBGGR10/2688x1520]
                -> "rkisp_rawwr0":0 [ENABLED]
        pad3: Source
                [fmt:SBGGR10/2688x1520]
                -> "rkisp_rawwr1":0 [ENABLED]
        pad4: Source
                [fmt:SBGGR10/2688x1520]
                -> "rkisp_rawwr2":0 [ENABLED]
        pad5: Source
                [fmt:SBGGR10/2688x1520]
                -> "rkisp_rawwr3":0 [ENABLED]

- entity 13: rkisp-bridge-ispp (1 pad, 1 link)
             type V4L2 subdev subtype Unknown
        pad0: Sink
v4l2_subdev_open: Failed to open subdev device node
                <- "rkisp-isp-subdev":2 [ENABLED]

- entity 17: rkisp_mainpath (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video0
        pad0: Sink
                <- "rkisp-isp-subdev":2 []

- entity 23: rkisp_selfpath (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video1
        pad0: Sink
                <- "rkisp-isp-subdev":2 []

- entity 29: rkisp_rawwr0 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video2
        pad0: Sink
                <- "rkisp-csi-subdev":2 [ENABLED]

- entity 35: rkisp_rawwr1 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video3
        pad0: Sink
                <- "rkisp-csi-subdev":3 [ENABLED]

- entity 41: rkisp_rawwr2 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video4
        pad0: Sink
                <- "rkisp-csi-subdev":4 [ENABLED]

- entity 47: rkisp_rawwr3 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video5
        pad0: Sink
                <- "rkisp-csi-subdev":5 [ENABLED]

- entity 53: rkisp_rawrd0_m (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video6
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 59: rkisp_rawrd1_l (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video7
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 65: rkisp_rawrd2_s (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video8
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 71: rkisp-statistics (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video9
        pad0: Sink
                <- "rkisp-isp-subdev":3 [ENABLED]

- entity 77: rkisp-input-params (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video10
        pad0: Source
                -> "rkisp-isp-subdev":1 [ENABLED]

- entity 83: rkisp-mipi-luma (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video11
        pad0: Sink
                <- "rkisp-isp-subdev":3 [ENABLED]

- entity 89: rockchip-mipi-dphy-rx (2 pads, 2 links)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev3
        pad0: Sink
                [fmt:SBGGR10/2688x1520]
                <- "m01_f_os04a10 1-0036-1":0 [ENABLED]
        pad1: Source
                [fmt:SBGGR10/2688x1520]
                -> "rkisp-csi-subdev":0 [ENABLED]

- entity 92: m01_f_os04a10 1-0036-1 (1 pad, 1 link)
             type V4L2 subdev subtype Sensor
             device node name /dev/v4l-subdev4
        pad0: Source
                [fmt:SBGGR10/2688x1520]
                -> "rockchip-mipi-dphy-rx":0 [ENABLED]

- entity 96: m01_f_ircut (0 pad, 0 link)
             type V4L2 subdev subtype Lens
             device node name /dev/v4l-subdev5

rk1126_r:/ # media-ctl -d /dev/media1 -p
Opening media device /dev/media1
Enumerating entities
Found 8 entities
Enumerating pads and links
Media controller API version 0.0.149

Media device information
------------------------
driver          rkispp
model           rkispp0
serial
bus info
hw revision     0x0
driver version  0.0.149

Device topology
- entity 1: rkispp_input_image (1 pad, 1 link)
            type Node subtype V4L
            device node name /dev/video12
        pad0: Source
                -> "rkispp-subdev":0 []

- entity 5: rkispp_m_bypass (1 pad, 1 link)
            type Node subtype V4L
            device node name /dev/video13
        pad0: Sink
                <- "rkispp-subdev":2 [ENABLED]

- entity 9: rkispp_scale0 (1 pad, 1 link)
            type Node subtype V4L
            device node name /dev/video14
        pad0: Sink
                <- "rkispp-subdev":2 [ENABLED]

- entity 13: rkispp_scale1 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video15
        pad0: Sink
                <- "rkispp-subdev":2 [ENABLED]

- entity 17: rkispp_scale2 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video16
        pad0: Sink
                <- "rkispp-subdev":2 [ENABLED]

- entity 21: rkispp_input_params (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video17
        pad0: Source
                -> "rkispp-subdev":1 [ENABLED]

- entity 25: rkispp-stats (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video18
        pad0: Sink
                <- "rkispp-subdev":3 [ENABLED]

- entity 29: rkispp-subdev (4 pads, 7 links)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev0
        pad0: Sink
                [fmt:YUYV2X8/2688x1520
                 crop.bounds:(0,0)/2688x1520
                 crop:(0,0)/2688x1520]
                <- "rkispp_input_image":0 []
        pad1: Sink
                <- "rkispp_input_params":0 [ENABLED]
        pad2: Source
                [fmt:YUYV2X8/2688x1520]
                -> "rkispp_m_bypass":0 [ENABLED]
                -> "rkispp_scale0":0 [ENABLED]
                -> "rkispp_scale1":0 [ENABLED]
                -> "rkispp_scale2":0 [ENABLED]
        pad3: Source
                -> "rkispp-stats":0 [ENABLED]

2）运行rkaiq_3A_server程序；
开一个cmd窗口:adb shell
rk1126_r:/ # rkaiq_3A_server &

3）获取带效果处理的数据流
开另外一个cmd窗口,运行如下命令：/data/NV12.yuv 就是处理后的YUV数据
adb shell
rk1126_r:/ # v4l2-ctl --verbose -d /dev/video13 --set-fmt-video=width=2688,height=1520,pixelformat='NV12' \
--stream-mmap=4 --set-selection=target=crop,flags=0,top=0,left=0,width=2688,height=1520 \
--stream-count=1 --stream-skip=10 --stream-to=/data/NV12.yuv --stream-poll 

4）media-ctl 以及 v4l2-ctl工具说明
media-ctl 以及 v4l2-ctl工具使用说明，本文不做详细介绍，可自行百度；
media-ctl 以及 v4l2-ctl是v4l-utils中包含的两个命令行工具。
源代码下载地址：https://w ww.linuxtv.org/downloads/v4l-utils/
