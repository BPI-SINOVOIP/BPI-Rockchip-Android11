1、rkaiq_3A_server文件说明：
	rkaiq_3A_server是直接单独运行librkisp库的可执行程序.
	它不调用CameraHal库，而是直接单独运行librkisp库（即3A自动运行）
	然后就可以通过v4l2-ctl或者其他应用获取3A处理后的NV12数据了。

2、使用说明：
1）首先确定数据流链路都已经配置好
可以使用命令：media-ctl -d /dev/mediax -p 查看；
例如：如下是RK3568双摄的数据链路情况
rk3568_r:/ # media-ctl -d /dev/media0 -p
Opening media device /dev/media0
Enumerating entities
Found 8 entities
Enumerating pads and links
Media controller API version 0.0.172

Media device information
------------------------
driver          rkcif
model           rkcif_mipi_lvds
serial
bus info
hw revision     0x0
driver version  0.0.172

Device topology
- entity 1: stream_cif_mipi_id0 (1 pad, 4 links)
            type Node subtype V4L
            device node name /dev/video0
        pad0: Sink
                <- "rockchip-mipi-csi2":1 [ENABLED]
                <- "rockchip-mipi-csi2":2 []
                <- "rockchip-mipi-csi2":3 []
                <- "rockchip-mipi-csi2":4 []

- entity 5: stream_cif_mipi_id1 (1 pad, 4 links)
            type Node subtype V4L
            device node name /dev/video1
        pad0: Sink
                <- "rockchip-mipi-csi2":1 []
                <- "rockchip-mipi-csi2":2 [ENABLED]
                <- "rockchip-mipi-csi2":3 []
                <- "rockchip-mipi-csi2":4 []

- entity 9: stream_cif_mipi_id2 (1 pad, 4 links)
            type Node subtype V4L
            device node name /dev/video2
        pad0: Sink
                <- "rockchip-mipi-csi2":1 []
                <- "rockchip-mipi-csi2":2 []
                <- "rockchip-mipi-csi2":3 [ENABLED]
                <- "rockchip-mipi-csi2":4 []

- entity 13: stream_cif_mipi_id3 (1 pad, 4 links)
             type Node subtype V4L
             device node name /dev/video3
        pad0: Sink
                <- "rockchip-mipi-csi2":1 []
                <- "rockchip-mipi-csi2":2 []
                <- "rockchip-mipi-csi2":3 []
                <- "rockchip-mipi-csi2":4 [ENABLED]

- entity 17: rockchip-mipi-csi2 (5 pads, 17 links)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev4
        pad0: Sink
                [fmt:SRGGB10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                <- "rockchip-csi2-dphy2":1 [ENABLED]
        pad1: Source
                [fmt:SRGGB10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 [ENABLED]
                -> "stream_cif_mipi_id1":0 []
                -> "stream_cif_mipi_id2":0 []
                -> "stream_cif_mipi_id3":0 []
        pad2: Source
                [fmt:SRGGB10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 []
                -> "stream_cif_mipi_id1":0 [ENABLED]
                -> "stream_cif_mipi_id2":0 []
                -> "stream_cif_mipi_id3":0 []
        pad3: Source
                [fmt:SRGGB10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 []
                -> "stream_cif_mipi_id1":0 []
                -> "stream_cif_mipi_id2":0 [ENABLED]
                -> "stream_cif_mipi_id3":0 []
        pad4: Source
                [fmt:SRGGB10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 []
                -> "stream_cif_mipi_id1":0 []
                -> "stream_cif_mipi_id2":0 []
                -> "stream_cif_mipi_id3":0 [ENABLED]

- entity 23: rockchip-csi2-dphy2 (2 pads, 2 links)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev5
        pad0: Sink
                [fmt:SRGGB10/1920x1080]
                <- "m00_f_gc2093 2-007e":0 [ENABLED]
        pad1: Source
                [fmt:SRGGB10/1920x1080]
                -> "rockchip-mipi-csi2":0 [ENABLED]

- entity 28: rkcif-mipi-luma (0 pad, 0 link)
             type Node subtype V4L
             device node name /dev/video4

- entity 31: m00_f_gc2093 2-007e (1 pad, 1 link)
             type V4L2 subdev subtype Sensor
             device node name /dev/v4l-subdev6
        pad0: Source
                [fmt:SRGGB10/1920x1080]
                -> "rockchip-csi2-dphy2":0 [ENABLED]


rk3568_r:/ #
rk3568_r:/ #
rk3568_r:/ # media-ctl -d /dev/media1 -p
Opening media device /dev/media1
Enumerating entities
Found 13 entities
Enumerating pads and links
Media controller API version 0.0.172

Media device information
------------------------
driver          rkisp-vir0
model           rkisp0
serial
bus info
hw revision     0x0
driver version  0.0.172

Device topology
- entity 1: rkisp-isp-subdev (4 pads, 7 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev0
        pad0: Sink
                [fmt:SGRBG10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                <- "rkisp-csi-subdev":1 [ENABLED]
                <- "rkisp_rawrd0_m":0 [ENABLED]
                <- "rkisp_rawrd2_s":0 [ENABLED]
        pad1: Sink
                <- "rkisp-input-params":0 [ENABLED]
        pad2: Source
                [fmt:YUYV2X8/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "rkisp_mainpath":0 [ENABLED]
                -> "rkisp_selfpath":0 [ENABLED]
        pad3: Source
                -> "rkisp-statistics":0 [ENABLED]

- entity 6: rkisp-csi-subdev (6 pads, 5 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev1
        pad0: Sink
                [fmt:SGRBG10/1920x1080]
                <- "rockchip-csi2-dphy1":1 [ENABLED]
        pad1: Source
                [fmt:SGRBG10/1920x1080]
                -> "rkisp-isp-subdev":0 [ENABLED]
        pad2: Source
                [fmt:SGRBG10/1920x1080]
                -> "rkisp_rawwr0":0 [ENABLED]
        pad3: Source
                [fmt:SGRBG10/1920x1080]
        pad4: Source
                [fmt:SGRBG10/1920x1080]
                -> "rkisp_rawwr2":0 [ENABLED]
        pad5: Source
                [fmt:SGRBG10/1920x1080]
                -> "rkisp_rawwr3":0 [ENABLED]

- entity 13: rkisp_mainpath (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video5
        pad0: Sink
                <- "rkisp-isp-subdev":2 [ENABLED]

- entity 19: rkisp_selfpath (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video6
        pad0: Sink
                <- "rkisp-isp-subdev":2 [ENABLED]

- entity 25: rkisp_rawwr0 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video7
        pad0: Sink
                <- "rkisp-csi-subdev":2 [ENABLED]

- entity 31: rkisp_rawwr2 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video8
        pad0: Sink
                <- "rkisp-csi-subdev":4 [ENABLED]

- entity 37: rkisp_rawwr3 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video9
        pad0: Sink
                <- "rkisp-csi-subdev":5 [ENABLED]

- entity 43: rkisp_rawrd0_m (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video10
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 49: rkisp_rawrd2_s (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video11
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 55: rkisp-statistics (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video12
        pad0: Sink
                <- "rkisp-isp-subdev":3 [ENABLED]

- entity 61: rkisp-input-params (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video13
        pad0: Source
                -> "rkisp-isp-subdev":1 [ENABLED]

- entity 67: rockchip-csi2-dphy1 (2 pads, 2 links)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev2
        pad0: Sink
                [fmt:SGRBG10/1920x1080]
                <- "m01_b_gc2053 2-0037":0 [ENABLED]
        pad1: Source
                [fmt:SGRBG10/1920x1080]
                -> "rkisp-csi-subdev":0 [ENABLED]

- entity 70: m01_b_gc2053 2-0037 (1 pad, 1 link)
             type V4L2 subdev subtype Sensor
             device node name /dev/v4l-subdev3
        pad0: Source
                [fmt:SGRBG10/1920x1080]
                -> "rockchip-csi2-dphy1":0 [ENABLED]


rk3568_r:/ #
rk3568_r:/ # media-ctl -d /dev/media2 -p
Opening media device /dev/media2
Enumerating entities
Found 12 entities
Enumerating pads and links
Media controller API version 0.0.172

Media device information
------------------------
driver          rkisp-vir1
model           rkisp1
serial
bus info
hw revision     0x0
driver version  0.0.172

Device topology
- entity 1: rkisp-isp-subdev (4 pads, 7 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev7
        pad0: Sink
                [fmt:SRGGB10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                <- "rkisp_rawrd0_m":0 [ENABLED]
                <- "rkisp_rawrd2_s":0 [ENABLED]
                <- "rkcif_mipi_lvds":0 [ENABLED]
        pad1: Sink
                <- "rkisp-input-params":0 [ENABLED]
        pad2: Source
                [fmt:YUYV2X8/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "rkisp_mainpath":0 [ENABLED]
                -> "rkisp_selfpath":0 [ENABLED]
        pad3: Source
                -> "rkisp-statistics":0 [ENABLED]

- entity 6: rkisp-csi-subdev (6 pads, 3 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev8
        pad0: Sink
        pad1: Source
        pad2: Source
                -> "rkisp_rawwr0":0 [ENABLED]
        pad3: Source
        pad4: Source
                -> "rkisp_rawwr2":0 [ENABLED]
        pad5: Source
                -> "rkisp_rawwr3":0 [ENABLED]

- entity 13: rkisp_mainpath (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video14
        pad0: Sink
                <- "rkisp-isp-subdev":2 [ENABLED]

- entity 19: rkisp_selfpath (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video15
        pad0: Sink
                <- "rkisp-isp-subdev":2 [ENABLED]

- entity 25: rkisp_rawwr0 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video16
        pad0: Sink
                <- "rkisp-csi-subdev":2 [ENABLED]

- entity 31: rkisp_rawwr2 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video17
        pad0: Sink
                <- "rkisp-csi-subdev":4 [ENABLED]

- entity 37: rkisp_rawwr3 (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video18
        pad0: Sink
                <- "rkisp-csi-subdev":5 [ENABLED]

- entity 43: rkisp_rawrd0_m (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video19
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 49: rkisp_rawrd2_s (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video20
        pad0: Source
                -> "rkisp-isp-subdev":0 [ENABLED]

- entity 55: rkisp-statistics (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video21
        pad0: Sink
                <- "rkisp-isp-subdev":3 [ENABLED]

- entity 61: rkisp-input-params (1 pad, 1 link)
             type Node subtype V4L
             device node name /dev/video22
        pad0: Source
                -> "rkisp-isp-subdev":1 [ENABLED]

- entity 67: rkcif_mipi_lvds (1 pad, 1 link)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev9
        pad0: Source
                [fmt:SRGGB10/1920x1080]
                -> "rkisp-isp-subdev":0 [ENABLED]


rk3568_r:/ #

2）运行rkaiq_3A_server程序；
开一个cmd窗口:adb shell
rk1126_r:/ # rkaiq_3A_server --sensor_index=0 --hdrmode=HDR2
参数说明：
--sensor_index：
其中sensor_index=0这个参数对应底层sensor的module index，这里匹配的就是：m00_f_gc2093 2-007e这个sensor
其中sensor_index=1匹配的就是：m01_b_gc2053 2-0037这个sensor

--hdrmode：
这个必带参数，否则运行出错，参数可以配置为：NORMAL/HDR2/HDR3; 根据sensor支持的模式确定；
例如：这里gc2053只支持Normal模式，所以配置为：--hdrmode=NORMAL
而gc2093即支持NORMAL模式也支持HDR2模式，所以可以配置为：--hdrmode=NORMAL 或 --hdrmode=HDR2

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
