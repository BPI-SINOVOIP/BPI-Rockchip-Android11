1、rkaiq_multi_cam_3A_server文件说明：
	rkaiq_multi_cam_3A_server文件是直接单独运行librkisp库的可执行程序.
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
Media controller API version 0.0.193

Media device information
------------------------
driver          rkcif
model           rkcif_mipi_lvds
serial
bus info
hw revision     0x0
driver version  0.0.193

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
             device node name /dev/v4l-subdev0
        pad0: Sink
                [fmt:SBGGR10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                <- "rockchip-csi2-dphy2":1 [ENABLED]
        pad1: Source
                [fmt:SBGGR10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 [ENABLED]
                -> "stream_cif_mipi_id1":0 []
                -> "stream_cif_mipi_id2":0 []
                -> "stream_cif_mipi_id3":0 []
        pad2: Source
                [fmt:SBGGR10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 []
                -> "stream_cif_mipi_id1":0 [ENABLED]
                -> "stream_cif_mipi_id2":0 []
                -> "stream_cif_mipi_id3":0 []
        pad3: Source
                [fmt:SBGGR10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 []
                -> "stream_cif_mipi_id1":0 []
                -> "stream_cif_mipi_id2":0 [ENABLED]
                -> "stream_cif_mipi_id3":0 []
        pad4: Source
                [fmt:SBGGR10/1920x1080
                 crop.bounds:(0,0)/1920x1080
                 crop:(0,0)/1920x1080]
                -> "stream_cif_mipi_id0":0 []
                -> "stream_cif_mipi_id1":0 []
                -> "stream_cif_mipi_id2":0 []
                -> "stream_cif_mipi_id3":0 [ENABLED]

- entity 23: rockchip-csi2-dphy2 (2 pads, 2 links)
             type V4L2 subdev subtype Unknown
             device node name /dev/v4l-subdev1
        pad0: Sink
                [fmt:SBGGR10/1920x1080]
                <- "m00_b_os02h10 4-003c":0 [ENABLED]
        pad1: Source
                [fmt:SBGGR10/1920x1080]
                -> "rockchip-mipi-csi2":0 [ENABLED]

- entity 28: rkcif-mipi-luma (0 pad, 0 link)
             type Node subtype V4L
             device node name /dev/video4

- entity 31: m00_b_os02h10 4-003c (1 pad, 1 link)
             type V4L2 subdev subtype Sensor
             device node name /dev/v4l-subdev2
        pad0: Source
                [fmt:SBGGR10/1920x1080]
                -> "rockchip-csi2-dphy2":0 [ENABLED]


ts3672:/ # media-ctl -d /dev/media1 -p
Opening media device /dev/media1
Enumerating entities
Found 13 entities
Enumerating pads and links
Media controller API version 0.0.193

Media device information
------------------------
driver          rkisp-vir0
model           rkisp0
serial
bus info
hw revision     0x0
driver version  0.0.193

Device topology
- entity 1: rkisp-isp-subdev (4 pads, 7 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev3
        pad0: Sink
                [fmt:SBGGR10/1920x1080
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
            device node name /dev/v4l-subdev4
        pad0: Sink
                [fmt:SBGGR10/1920x1080]
                <- "rockchip-csi2-dphy1":1 [ENABLED]
        pad1: Source
                [fmt:SBGGR10/1920x1080]
                -> "rkisp-isp-subdev":0 [ENABLED]
        pad2: Source
                [fmt:SBGGR10/1920x1080]
                -> "rkisp_rawwr0":0 [ENABLED]
        pad3: Source
                [fmt:SBGGR10/1920x1080]
        pad4: Source
                [fmt:SBGGR10/1920x1080]
                -> "rkisp_rawwr2":0 [ENABLED]
        pad5: Source
                [fmt:SBGGR10/1920x1080]
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
             device node name /dev/v4l-subdev5
        pad0: Sink
                [fmt:SBGGR10/1920x1080]
                <- "m01_f_os02h10b 1-003c":0 [ENABLED]
        pad1: Source
                [fmt:SBGGR10/1920x1080]
                -> "rkisp-csi-subdev":0 [ENABLED]

- entity 70: m01_f_os02h10b 1-003c (1 pad, 1 link)
             type V4L2 subdev subtype Sensor
             device node name /dev/v4l-subdev6
        pad0: Source
                [fmt:SBGGR10/1920x1080]
                -> "rockchip-csi2-dphy1":0 [ENABLED]


ts3672:/ #
ts3672:/ # media-ctl -d /dev/media2 -p
Opening media device /dev/media2
Enumerating entities
Found 12 entities
Enumerating pads and links
Media controller API version 0.0.193

Media device information
------------------------
driver          rkisp-vir1
model           rkisp1
serial
bus info
hw revision     0x0
driver version  0.0.193

Device topology
- entity 1: rkisp-isp-subdev (4 pads, 7 links)
            type V4L2 subdev subtype Unknown
            device node name /dev/v4l-subdev7
        pad0: Sink
                [fmt:SBGGR10/1920x1080
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
                [fmt:SBGGR10/1920x1080]
                -> "rkisp-isp-subdev":0 [ENABLED]

rk3568_r:/ #

2）运行 rkaiq_multi_cam_3A_server 程序；
开一个cmd窗口:adb shell
rk3568_r:/ # rkaiq_multi_cam_3A_server

参数说明：
使用 multi_camera_config.xml文件配置各个sensor的HDR模式；
例如根据上面pipeline情况；
moduleId="0" 对应：m00_b_os02h10 4-003c的HDR模式配置
moduleId="1" 对应：m01_f_os02h10b 1-003c的HDR模式配置

3）获取带效果处理的数据流
开另外一个cmd窗口,运行如下命令抓取数据流：
adb shell "v4l2-ctl --verbose -d /dev/video5 --set-fmt-video=width=1920,height=1080,pixelformat='NV12' --stream-mmap=4 --set-selection=target=crop,flags=0,top=0,left=0,width=1920,height=1080"
adb shell "v4l2-ctl --verbose -d /dev/video6 --set-fmt-video=width=1920,height=1080,pixelformat='NV12' --stream-mmap=4 --set-selection=target=crop,flags=0,top=0,left=0,width=1920,height=1080"

adb shell "v4l2-ctl --verbose -d /dev/video14 --set-fmt-video=width=1920,height=1080,pixelformat='NV12' --stream-mmap=4 --set-selection=target=crop,flags=0,top=0,left=0,width=1920,height=1080"
adb shell "v4l2-ctl --verbose -d /dev/video15 --set-fmt-video=width=1920,height=1080,pixelformat='NV12' --stream-mmap=4 --set-selection=target=crop,flags=0,top=0,left=0,width=1920,height=1080"


4）附录：
cmd窗口运行打印示例：
C:\Users\lzw\Desktop>adb root
adbd is already running as root

C:\Users\lzw\Desktop>adb shell "rkaiq_multi_cam_3A_server"
media get entity by name: rkcif-lvds-subdev is null
media get entity by name: rkcif-lite-lvds-subdev is null
media get entity by name: rockchip-mipi-dphy-rx is null
media get entity by name: rockchip-csi2-dphy0 is null
media get entity by name: stream_cif is null
media get entity by name: rkcif-dvp-sof is null
media get entity by name: rkisp-mpfbc-subdev is null
media get entity by name: rkisp_rawwr1 is null
media get entity by name: rkisp_dmapath is null
media get entity by name: rkisp_rawrd1_l is null
media get entity by name: rkisp-mipi-luma is null
media get entity by name: rockchip-mipi-dphy-rx is null
media get entity by name: rockchip-csi2-dphy0 is null
media get entity by name: rkcif_dvp is null
media get entity by name: rkcif_mipi_lvds is null
media get entity by name: rkcif_dvp is null
media get entity by name: rkcif_lite_mipi_lvds is null
media get entity by name: rkcif_mipi_lvds is null
media get entity by name: rkisp-mpfbc-subdev is null
media get entity by name: rkisp_rawwr1 is null
media get entity by name: rkisp_dmapath is null
media get entity by name: rkisp_rawrd1_l is null
media get entity by name: rkisp-mipi-luma is null
media get entity by name: rockchip-mipi-dphy-rx is null
media get entity by name: rockchip-csi2-dphy0 is null
media get entity by name: rkcif_dvp is null
media get entity by name: rkcif_dvp is null
media get entity by name: rkcif_lite_mipi_lvds is null
DBG: ----------------------------------------------

DBG: media get sys_path: /dev/media0
DBG: rkisp_enumrate_modules(286) sensor_entity_name(m00_b_os02h10 4-003c)
DBG: parseModuleInfo:228, real sensor name os02h10, module ori b, module id m00
DBG: module_idx(0) sensor_entity_name(m00_b_os02h10 4-003c), linkedModelName(rkcif_mipi_lvds).
DBG: media get sys_path: /dev/media1
DBG: rkisp_enumrate_modules(286) sensor_entity_name(m01_f_os02h10b 1-003c)
DBG: parseModuleInfo:228, real sensor name os02h10b, module ori f, module id m01
DBG: module_idx(1) sensor_entity_name(m01_f_os02h10b 1-003c), linkedModelName(rkisp0).
DBG: media get sys_path: /dev/media2
DBG: media path: /dev/media2, no camera sensor found!
DBG: media get sys_path: /dev/media3
DBG: media get sys_path: /dev/media4
DBG: media get sys_path: /dev/media5
DBG: media get sys_path: /dev/media6
DBG: media get sys_path: /dev/media7
DBG: media get sys_path: /dev/media8
DBG: media get sys_path: /dev/media9
DBG: rkisp_get_sensor_info:380, find available camera num:2!
DBG: media_infos: module_idx(0) sensor_entity_name(m00_b_os02h10 4-003c)
DBG: media_infos: module_idx(1) sensor_entity_name(m01_f_os02h10b 1-003c)
DBG: media get sys_path: /dev/media0
DBG: media: /dev/media0 is cif node, skip!
DBG: media get sys_path: /dev/media1
DBG: @rkisp_enumrate_ispdev_info, Target sensor name: m00_b_os02h10 4-003c, candidate media: /dev/media1.
media get entity by name: rkcif_mipi_lvds is null
DBG: /dev/media1 is direct linked to sensor, not linked to cif!
DBG: @rkisp_enumrate_ispdev_info, Target sensor name: m01_f_os02h10b 1-003c, candidate media: /dev/media1.
DBG: get m01_f_os02h10b 1-003c devname: /dev/v4l-subdev6
DBG: get rkisp-isp-subdev devname: /dev/v4l-subdev3
DBG: get rkisp-input-params devname: /dev/video13
DBG: get rkisp-statistics devname: /dev/video12
DBG: media get sys_path: /dev/media2
DBG: @rkisp_enumrate_ispdev_info, Target sensor name: m00_b_os02h10 4-003c, candidate media: /dev/media2.
DBG: get rkcif_mipi_lvds devname: /dev/v4l-subdev9
DBG: get rkisp-isp-subdev devname: /dev/v4l-subdev7
DBG: get rkisp-input-params devname: /dev/video22
DBG: get rkisp-statistics devname: /dev/video21
DBG: @rkisp_enumrate_ispdev_info, Target sensor name: m01_f_os02h10b 1-003c, candidate media: /dev/media2.
media get entity by name: m01_f_os02h10b 1-003c is null
DBG: /dev/media2 not direct linked to sensor!
DBG: media get sys_path: /dev/media3
DBG: media get sys_path: /dev/media4
DBG: media get sys_path: /dev/media5
DBG: media get sys_path: /dev/media6
DBG: media get sys_path: /dev/media7
DBG: media get sys_path: /dev/media8
DBG: media get sys_path: /dev/media9
DBG: loadFromCfg: load aiq camera config succeed!
DBG: @updateModeList enter!
DBG: camera:0, hdrmode: HDR2.
DBG: camera:1, hdrmode: HDR2.
DBG: -------------pthread_create start------------------

DBG: engine_thread current thread id:3908813248
DBG: subscribe events from /dev/video22 !
DBG: subscribe events from /dev/video22 success !
DBG: /dev/media2: init engine success...
DBG: prepare_engine--set workingmode(HDR2)
DBG: /dev/v4l-subdev2 - m00_b_os02h10 4-003c: link enabled : 1
DBG: wait for aiq init time: 0 sec, 96 ms
DBG: -------------pthread_create success------------------

DBG: -------------pthread_create start------------------

DBG: engine_thread current thread id:3890975168
DBG: subscribe events from /dev/video13 !
DBG: subscribe events from /dev/video13 success !
media get entity by name: rkisp_rawrd1_l is null
DBG: /dev/media2: wait stream start event...
DBG: /dev/media1: init engine success...
DBG: prepare_engine--set workingmode(HDR2)
DBG: /dev/v4l-subdev6 - m01_f_os02h10b 1-003c: link enabled : 1
DBG: wait for aiq init time: 0 sec, 82 ms
DBG: -------------pthread_create success------------------

DBG: -------------pthread_join start------------------

media get entity by name: rkisp_rawrd1_l is null
DBG: /dev/media1: wait stream start event...
