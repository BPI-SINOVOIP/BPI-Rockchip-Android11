/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_VERSION_H
#define _RKISP_VERSION_H
#include <linux/version.h>
#include <linux/rkisp21-config.h>

/*
 * RKISP DRIVER VERSION NOTE
 *
 * v0.1.0:
 * 1. First version;
 *
 * v0.1.1:
 * 1. support lvds interface
 *
 * v0.1.2:
 * support multi virtual device;
 *
 * v0.1.3:
 * 1. support link with interface of cif
 * 2. fix picture of hdr is abnormal in multi-visual isp when run second time
 * 3. adjust hurry priority to middle
 * 4. mi lum burst to 16 for dmatx
 * 5. add cru reset for rv1126
 *
 * v0.1.4:
 * 1. add more clk rate for rv1126
 * 2. support hal to control hdrtmo on/off
 * 3. switch hdr frame mode for read back
 *
 * v0.1.5:
 * 1. add proc fs
 * 2. add iq part information to procfs
 * 3. fix stream failure in thunderboot mode
 *
 * v0.1.6:
 * 1. raw length 256 align
 * 2. soft reset for Dehaze
 *
 * v0.1.7:
 * 1. fix rawaf is disabled in config function
 * 2. clear csi rdbk fifo when first open
 * 3. vb2 support cache hints
 *
 * v0.1.8:
 * 1. add monitor to restart if abnormal
 * 2. isp/ispp procfs add work info
 * 3. fix scr clock is not disabled after app run
 * 4. request buf to alloc dummy buf
 * 5. set tmo bit in gain by tmo enable
 * 6. only rx mode can use when link with cif
 *
 * v0.1.9:
 * 1. isp and ispp sync to power off
 * 2. fix lsc error when ldch is on
 * 3. fix error status of stream off
 * 4. skip frame when change hdr/normal mode
 * 5. use ldch share buffer to reduce buffer size
 *
 * v1.2.0:
 * 1. resolution write directly to reg for first dev
 * 2. normal read back to enable hdr merge
 * 3. enable LDCH in 2th frame
 *
 * v1.2.1:
 * 1. fix normal merge enable config
 * 2. fix size no update for multi sensor switch
 * 3. dmatx add yuyv format
 *
 * v1.2.2:
 * 1. isp/ispp add lock for multi dev runtime
 * 2. support output isp/ispp reg on each frame
 * 3. fix error detected by depmod
 *
 * v1.3.0:
 * 1. capture to different version
 * 2. add isp21
 * 3. add rk3568 config
 * 4. support iq part of isp21
 * 5. remove hdrtmo to fix crash when connect to yuv sensor
 * 6. fix enable function of ynr/cnr/bay3d/dhaz/adrc is not correct
 * 7. fix can not get correct awb rawdata
 * 8. add get awb data from ddr function
 * 9. fix frame id error for isp21
 * 10. config lsc by sram in rdbk mode
 * 11. add force update to enable dehaze
 * 12. fix bug of scheduling while atomic
 * 13. fix setting drc register is not correct
 * 14. extend line to fix merge bypass bug for isp20
 * 15. vb2 dma sg for iommu enable
 * 16. config dmatx to valid buf addr
 *
 * v1.4.1:
 * 1. support motion detection mode
 * 2. get stats only when meas done is on
 * 3. fix lsc lut error in start/stop test
 *
 * v1.5.1:
 * 1. support to set format if no streaming
 * 2. add vb2_rdma_sg_memops to support contiguous page
 * 3. fix gain buf update
 * 4. 64 align y size for fbcgain format
 * 5. add trigger mode ioctl
 * 6. fix config of clk_dbg
 * 7. fix path select of cif input
 * 8. fix mpfbc buf update if readback off
 * 9. fix array overflow
 * 10. use force big mode when auto big mode is incorrect
 * 11. fix extend line with isp input crop case
 * 12. set lgmean related regs for tmo in hdr isr
 *
 * v1.6.0:
 * 1. reorder of subdev stream
 * 2. fix media link err for name don't match
 * 3. switch hdr_done interrupt according to hdrtmo cnt mode
 * 4. support output isp/ispp reg in nv12 format
 * 5. isp and ispp add shutdown
 * 6. image input from user
 * 7. import dma API for memory synchronisation for thunderboot
 * 8. don't start ldch asynchronously in multi-isp mode
 * 9. fix err of mp dump raw for isp20
 * 10. make sure 3dlut no continuous read twice
 * 11. adjust rdbk times with mulit dev for isp2.0
 *
 * v1.6.1:
 * 1.fix multi dev refcnt
 * 2.update procfs info
 * 3.add check for params subscribe event
 * 4.resolution limit for isp21
 * 5.dma buf alloc limit to DMA32
 * 6.add monitor to restart if abnormal
 * 7.adjust probe order
 * 8.max height 3072 for isp21
 * 9.dmatx support embedded and shield pixels data
 * 10.separate rdbk from csi subdev
 * 11.add bt601/bt709/bt2020 colorspace
 * 12.apply en params if no match for isp20
 * 13.apply en params if no match for isp21
 * 14.isp21 get 3a stats from ddr
 * 15.reserved memory using rdma_sg ops
 *
 * v1.6.2:
 * 1.hdr direct for isp21
 * 2.fix same frame id
 * 3.fix isp and ispp share dmabuf release fail
 * 4.clear rdbk fifo at dmarx stop
 * 5.add lock for isp stream
 * 6.disable params when it stream off
 * 7.dmarx support yuv format
 * 8.frame buffer done early
 * 9.fix set pdaf in dpcc error
 * 10.add v-blank to procfs
 *
 * v1.7.0:
 * 1.off unused interrupt of csi
 * 2.fix sp no output when hdr dynamic switch
 * 3.check the output status of statistics v2x
 * 4.selfpath bytesperline 16 align
 * 5.compiled with differe hardware version
 * 6.add frame loss info to procfs
 * 7.remove associated of cproc and ie
 * 8.fix input crop config for isp21 multi device
 * 9.enable soft reset for other isp version
 * 10.rawrd support uncompact mode
 * 11.fix default params config for mode switch
 * 12.before frame start to update bridge mi
 * 13.disable tmo interrupt
 * 14.fix bottom image for debayer with extend line
 * 15.unregister dmarx at driver remove
 *
 * v1.8.0:
 * 1.sync alloc buf with dma sg case
 * 2.sync multi vir dev stream on/off
 * 3.replace iommu detach/attach
 * 4.fix CSI2RX_DATA_IDS_1 config err
 * 5.improve snapshot feature(tb in RISC-V)
 * 6.adjust params common api
 * 7.fix first params config two times for readback mode
 * 8.useless version return -EINVAL
 * 9.fix first frame abnormal
 * 10.dynamic memory alloc for params and stats function
 * 11.fix rawhist weight config error for multi device
 * 12.add csm params config for isp21
 */

#define RKISP_DRIVER_VERSION RKISP_API_VERSION

#endif
