# Copyright 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
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

import os.path

import its.caps
import its.device
import its.image
import its.objects

import matplotlib
from matplotlib import pylab
import numpy as np

AE_STATE_CONVERGED = 2
AE_STATE_FLASH_REQUIRED = 4
DELTA_GAIN_THRESH = 0.03  # >3% gain change --> luma change in same dir
DELTA_LUMA_THRESH = 0.03  # 3% frame-to-frame noise test_burst_sameness_manual
DELTA_NO_GAIN_THRESH = 0.01  # <1% gain change --> min luma change
LSC_TOL = 0.005  # allow <0.5% change in lens shading correction
NAME = os.path.basename(__file__).split('.')[0]
NUM_CAPS = 1
NUM_FRAMES = 30
VALID_STABLE_LUMA_MIN = 0.1
VALID_STABLE_LUMA_MAX = 0.9


def lsc_unchanged(lsc_avlb, lsc, idx):
    """Determine if lens shading correction unchanged.

    Args:
        lsc_avlb:   bool; True if lens shading correction available
        lsc:        list; lens shading correction matrix
        idx:        int; frame index
    Returns:
        boolean
    """
    if lsc_avlb:
        diff = list((np.array(lsc[idx]) - np.array(lsc[idx-1])) /
                    np.array(lsc[idx-1]))
        diff = map(abs, diff)
        max_abs_diff = max(diff)
        if max_abs_diff > LSC_TOL:
            print '  max abs(LSC) change:', round(max_abs_diff, 4)
            return False
        else:
            return True
    else:
        return True


def tonemap_unchanged(raw_cap, tonemap_g, idx):
    """Determine if tonemap unchanged.

    Args:
        raw_cap:    bool; True if RAW capture
        tonemap_g:  list; green tonemap
        idx:        int; frame index
    Returns:
        boolean
    """
    if not raw_cap:
        return tonemap_g[idx-1] == tonemap_g[idx]
    else:
        return True


def is_awb_af_stable(cap_info, i):
    awb_gains_0 = cap_info[i-1]['awb_gains']
    awb_gains_1 = cap_info[i]['awb_gains']
    ccm_0 = cap_info[i-1]['ccm']
    ccm_1 = cap_info[i]['ccm']
    fd_0 = cap_info[i-1]['fd']
    fd_1 = cap_info[i]['fd']

    return (np.allclose(awb_gains_0, awb_gains_1, rtol=0.01) and
            ccm_0 == ccm_1 and np.isclose(fd_0, fd_1, rtol=0.01))


def main():
    """Tests PER_FRAME_CONTROL properties for auto capture requests.

    If debug is required, MANUAL_POSTPROCESSING capability is implied
    since its.caps.read_3a is valid for test. Debug can performed with
    a defined tonemap curve:
    req['android.tonemap.mode'] = 0
    gamma = sum([[i/63.0,math.pow(i/63.0,1/2.2)] for i in xrange(64)],[])
    req['android.tonemap.curve'] = {
            'red': gamma, 'green': gamma, 'blue': gamma}
    """

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.per_frame_control(props) and
                             its.caps.read_3a(props))
        debug = its.caps.debug_mode()
        raw_avlb = its.caps.raw16(props)
        largest_yuv = its.objects.get_largest_yuv_format(props)
        match_ar = (largest_yuv['width'], largest_yuv['height'])
        fmts = [its.objects.get_smallest_yuv_format(props, match_ar=match_ar)]
        if raw_avlb and debug:
            fmts.insert(0, cam.CAP_RAW)

        failed = []
        for f, fmt in enumerate(fmts):
            print 'fmt:', fmt['format']
            cam.do_3a()
            req = its.objects.auto_capture_request()
            cap_info = {}
            ae_states = []
            lumas = []
            total_gains = []
            tonemap_g = []
            lsc = []
            num_caps = NUM_CAPS
            num_frames = NUM_FRAMES
            raw_cap = f == 0 and raw_avlb and debug
            lsc_avlb = its.caps.lsc_map(props) and not raw_cap
            print 'lens shading correction available:', lsc_avlb
            if lsc_avlb:
                req['android.statistics.lensShadingMapMode'] = 1
            name_suffix = 'YUV'
            if raw_cap:
                name_suffix = 'RAW'
                # break up caps if RAW to reduce load
                num_caps = NUM_CAPS * 6
                num_frames = NUM_FRAMES / 6
            for j in range(num_caps):
                caps = cam.do_capture([req]*num_frames, fmt)
                for i, cap in enumerate(caps):
                    frame = {}
                    idx = i + j * num_frames
                    print '=========== frame %d ==========' % idx
                    # RAW --> GR, YUV --> Y plane
                    if raw_cap:
                        plane = its.image.convert_capture_to_planes(
                                cap, props=props)[1]
                    else:
                        plane = its.image.convert_capture_to_planes(cap)[0]
                    tile = its.image.get_image_patch(
                            plane, 0.45, 0.45, 0.1, 0.1)
                    luma = its.image.compute_image_means(tile)[0]
                    ae_state = cap['metadata']['android.control.aeState']
                    iso = cap['metadata']['android.sensor.sensitivity']
                    isp_gain = cap['metadata']['android.control.postRawSensitivityBoost']
                    exp_time = cap['metadata']['android.sensor.exposureTime']
                    total_gain = iso * isp_gain / 100.0 * exp_time / 1000000.0
                    if raw_cap:
                        total_gain = iso * exp_time / 1000000.0
                    awb_state = cap['metadata']['android.control.awbState']
                    frame['awb_gains'] = cap['metadata']['android.colorCorrection.gains']
                    frame['ccm'] = cap['metadata']['android.colorCorrection.transform']
                    frame['fd'] = cap['metadata']['android.lens.focusDistance']

                    # Convert CCM from rational to float, as numpy arrays.
                    awb_ccm = np.array(its.objects.rational_to_float(frame['ccm'])).reshape(3, 3)

                    print 'AE: %d ISO: %d ISP_sen: %d exp(ns): %d tot_gain: %f' % (
                            ae_state, iso, isp_gain, exp_time, total_gain),
                    print 'luma: %f' % luma
                    print 'fd: %f' % frame['fd']
                    print 'AWB state: %d, AWB gains: %s\n AWB matrix: %s' % (
                            awb_state, str(frame['awb_gains']),
                            str(awb_ccm))
                    if not raw_cap:
                        tonemap = cap['metadata']['android.tonemap.curve']
                        tonemap_g.append(tonemap['green'])
                        print 'G tonemap curve:', tonemap_g[idx]
                    if lsc_avlb:
                        lsc.append(cap['metadata']['android.statistics.lensShadingCorrectionMap']['map'])

                    img = its.image.convert_capture_to_rgb_image(
                            cap, props=props)
                    its.image.write_image(img, '%s_frame_%s_%d.jpg' % (
                            NAME, name_suffix, idx))
                    cap_info[idx] = frame
                    ae_states.append(ae_state)
                    lumas.append(luma)
                    total_gains.append(total_gain)

            norm_gains = [x/max(total_gains)*max(lumas) for x in total_gains]
            pylab.figure(name_suffix)
            pylab.plot(range(len(lumas)), lumas, '-g.',
                       label='Center patch brightness')
            pylab.plot(range(len(norm_gains)), norm_gains, '-r.',
                       label='Metadata AE setting product')
            pylab.title(NAME + ' ' + name_suffix)
            pylab.xlabel('frame index')

            # expand y axis for low delta results
            ymin = min(norm_gains + lumas)
            ymax = max(norm_gains + lumas)
            yavg = (ymax + ymin)/2.0
            if ymax-ymin < 3*DELTA_LUMA_THRESH:
                ymin = round(yavg - 1.5*DELTA_LUMA_THRESH, 3)
                ymax = round(yavg + 1.5*DELTA_LUMA_THRESH, 3)
                pylab.ylim(ymin, ymax)
            pylab.legend()
            matplotlib.pyplot.savefig('%s_plot_%s.png' % (NAME, name_suffix))

            print '\nfmt:', fmt['format']
            for i in range(1, num_caps*num_frames):
                if is_awb_af_stable(cap_info, i):
                    prev_total_gain = total_gains[i-1]
                    total_gain = total_gains[i]
                    delta_gain = total_gain - prev_total_gain
                    prev_luma = lumas[i-1]
                    luma = lumas[i]
                    delta_luma = luma - prev_luma
                    delta_gain_rel = delta_gain / prev_total_gain
                    delta_luma_rel = delta_luma / prev_luma
                    # luma and total_gain should change in same direction
                    msg = '%s: frame %d: gain %.1f -> %.1f (%.1f%%), ' % (
                            fmt['format'], i, prev_total_gain, total_gain,
                            delta_gain_rel*100)
                    msg += 'luma %f -> %f (%.2f%%)  GAIN/LUMA OPPOSITE DIR' % (
                            prev_luma, luma, delta_luma_rel*100)
                    # Threshold change to trigger check. Small delta_gain might
                    # not be enough to generate a reliable delta_luma to
                    # overcome frame-to-frame variation.
                    if (tonemap_unchanged(raw_cap, tonemap_g, i) and
                                lsc_unchanged(lsc_avlb, lsc, i)):
                        if abs(delta_gain_rel) > DELTA_GAIN_THRESH:
                            print ' frame %d: %.2f%% delta gain,' % (
                                    i, delta_gain_rel*100),
                            print '%.2f%% delta luma' % (delta_luma_rel*100)
                            if delta_gain * delta_luma < 0.0:
                                failed.append(msg)
                        elif abs(delta_gain_rel) < DELTA_NO_GAIN_THRESH:
                            print ' frame %d: <|%.1f%%| delta gain,' % (
                                    i, DELTA_NO_GAIN_THRESH*100),
                            print '%.2f%% delta luma' % (delta_luma_rel*100)
                            msg = '%s: ' % fmt['format']
                            msg += 'frame %d: gain %.1f -> %.1f (%.1f%%), ' % (
                                    i, prev_total_gain, total_gain,
                                    delta_gain_rel*100)
                            msg += 'luma %f -> %f (%.1f%%)  ' % (
                                    prev_luma, luma, delta_luma_rel*100)
                            msg += '<|%.1f%%| GAIN, >|%.f%%| LUMA DELTA' % (
                                    DELTA_NO_GAIN_THRESH*100, DELTA_LUMA_THRESH*100)
                            if abs(delta_luma_rel) > DELTA_LUMA_THRESH:
                                failed.append(msg)
                        else:
                            print ' frame %d: %.1f%% delta gain,' % (
                                    i, delta_gain_rel*100),
                            print '%.2f%% delta luma' % (delta_luma_rel*100)
                    else:
                        print ' frame %d -> %d: tonemap' % (i-1, i),
                        print 'or lens shading correction changed'
                else:
                    print ' frame %d -> %d: AWB/AF changed' % (i-1, i)

            for i in range(len(lumas)):
                luma = lumas[i]
                ae_state = ae_states[i]
                if (ae_state == AE_STATE_CONVERGED or
                            ae_state == AE_STATE_FLASH_REQUIRED):
                    msg = '%s: frame %d AE converged ' % (fmt['format'], i)
                    msg += 'luma %f. valid range: (%f, %f)' % (
                            luma, VALID_STABLE_LUMA_MIN, VALID_STABLE_LUMA_MAX)
                    if VALID_STABLE_LUMA_MIN > luma > VALID_STABLE_LUMA_MAX:
                        failed.append(msg)
        if failed:
            print '\nError summary'
            for fail in failed:
                print fail
            assert not failed

if __name__ == '__main__':
    main()
