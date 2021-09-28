# Copyright 2014 The Android Open Source Project
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

import math
import os.path
import its.caps
import its.device
import its.image
import its.objects
import matplotlib
from matplotlib import pylab

NAME = os.path.basename(__file__).split('.')[0]
BAYER_LIST = ['R', 'GR', 'GB', 'B']
DIFF_THRESH = 0.0012  # absolute variance delta threshold
FRAC_THRESH = 0.2  # relative variance delta threshold
NUM_STEPS = 4
SENS_TOL = 0.97  # specification is <= 3%


def main():
    """Verify that the DNG raw model parameters are correct."""

    # Pass if the difference between expected and computed variances is small,
    # defined as being within an absolute variance delta or relative variance
    # delta of the expected variance, whichever is larger. This is to allow the
    # test to pass in the presence of some randomness (since this test is
    # measuring noise of a small patch) and some imperfect scene conditions
    # (since ITS doesn't require a perfectly uniformly lit scene).

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)
        its.caps.skip_unless(
                its.caps.raw(props) and
                its.caps.raw16(props) and
                its.caps.manual_sensor(props) and
                its.caps.read_3a(props) and
                its.caps.per_frame_control(props) and
                not its.caps.mono_camera(props))

        white_level = float(props['android.sensor.info.whiteLevel'])
        cfa_idxs = its.image.get_canonical_cfa_order(props)

        # Expose for the scene with min sensitivity
        sens_min, _ = props['android.sensor.info.sensitivityRange']
        sens_max_ana = props['android.sensor.maxAnalogSensitivity']
        sens_step = (sens_max_ana - sens_min) / NUM_STEPS
        s_ae, e_ae, _, _, _ = cam.do_3a(get_results=True)
        s_e_prod = s_ae * e_ae
        # Focus at zero to intentionally blur the scene as much as possible.
        f_dist = 0.0
        sensitivities = range(sens_min, sens_max_ana+1, sens_step)

        var_expected = [[], [], [], []]
        var_measured = [[], [], [], []]
        sens_valid = []
        for sens in sensitivities:
            # Capture a raw frame with the desired sensitivity
            exp = int(s_e_prod / float(sens))
            req = its.objects.manual_capture_request(sens, exp, f_dist)
            cap = cam.do_capture(req, cam.CAP_RAW)
            planes = its.image.convert_capture_to_planes(cap, props)
            s_read = cap['metadata']['android.sensor.sensitivity']
            print 'iso_write: %d, iso_read: %d' % (sens, s_read)

            # Test each raw color channel (R, GR, GB, B)
            noise_profile = cap['metadata']['android.sensor.noiseProfile']
            assert len(noise_profile) == len(BAYER_LIST)
            for i in range(len(BAYER_LIST)):
                print BAYER_LIST[i],
                # Get the noise model parameters for this channel of this shot.
                ch = cfa_idxs[i]
                s, o = noise_profile[ch]

                # Use a very small patch to ensure gross uniformity (i.e. so
                # non-uniform lighting or vignetting doesn't affect the variance
                # calculation)
                black_level = its.image.get_black_level(i, props,
                                                        cap['metadata'])
                level_range = white_level - black_level
                plane = its.image.get_image_patch(planes[i], 0.49, 0.49,
                                                  0.02, 0.02)
                tile_raw = plane * white_level
                tile_norm = ((tile_raw - black_level) / level_range)

                # exit if distribution is clipped at 0, otherwise continue
                mean_img_ch = tile_norm.mean()
                var_model = s * mean_img_ch + o
                # This computation is a suspicious because if the data were
                # clipped, the mean and standard deviation could be affected
                # in a way that affects this check. However, empirically,
                # the mean and standard deviation change more slowly than the
                # clipping point itself does, so the check remains correct
                # even after the signal starts to clip.
                mean_minus_3sigma = mean_img_ch - math.sqrt(var_model) * 3
                if mean_minus_3sigma < 0:
                    e_msg = '\nPixel distribution crosses 0.\n'
                    e_msg += 'Likely black level over-clips.\n'
                    e_msg += 'Linear model is not valid.\n'
                    e_msg += 'mean: %.3e, var: %.3e, u-3s: %.3e' % (
                            mean_img_ch, var_model, mean_minus_3sigma)
                    assert 0, e_msg
                else:
                    print 'mean:', mean_img_ch,
                    var_measured[i].append(
                            its.image.compute_image_variances(tile_norm)[0])
                    print 'var:', var_measured[i][-1],
                    var_expected[i].append(var_model)
                    print 'var_model:', var_expected[i][-1]
            print ''
            sens_valid.append(sens)

    # plot data and models
    for i, ch in enumerate(BAYER_LIST):
        pylab.plot(sens_valid, var_expected[i], 'rgkb'[i],
                   label=ch+' expected')
        pylab.plot(sens_valid, var_measured[i], 'rgkb'[i]+'.--',
                   label=ch+' measured')
    pylab.xlabel('Sensitivity')
    pylab.ylabel('Center patch variance')
    pylab.legend(loc=2)
    matplotlib.pyplot.savefig('%s_plot.png' % NAME)

    # PASS/FAIL check
    for i, ch in enumerate(BAYER_LIST):
        diffs = [abs(var_measured[i][j] - var_expected[i][j])
                 for j in range(len(sens_valid))]
        print 'Diffs (%s):'%(ch), diffs
        for j, diff in enumerate(diffs):
            thresh = max(DIFF_THRESH, FRAC_THRESH*var_expected[i][j])
            assert diff <= thresh, 'diff: %.5f, thresh: %.4f' % (diff, thresh)

if __name__ == '__main__':
    main()
