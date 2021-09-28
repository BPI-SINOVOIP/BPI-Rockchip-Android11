# Copyright 2014 The Android Open Source Project. Lint as python2
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
import textwrap

import its.caps
import its.device
import its.image
import its.objects

from matplotlib import pylab
import matplotlib.pyplot as plt
import numpy as np
import scipy.signal
import scipy.stats

BAYER_LIST = ['R', 'GR', 'GB', 'B']
BRACKET_MAX = 8  # Exposure bracketing range in stops
COLORS = 'rygcbm'  # Colors used for plotting the data for each exposure.
MAX_SCALE_FUDGE = 1.1
MAX_SIGNAL_VALUE = 0.25  # Maximum value to allow mean of the tiles to go.
NAME = os.path.basename(__file__).split('.')[0]
RTOL_EXP_GAIN = 0.97
STEPS_PER_STOP = 2  # How many sensitivities per stop to sample.
# How large of tiles to use to compute mean/variance.
# Large tiles may have their variance corrupted by low frequency
# image changes (lens shading, scene illumination).
TILE_SIZE = 32


def main():
    """Capture a set of raw images with increasing analog gains and measure the noise.
    """

    bracket_factor = math.pow(2, BRACKET_MAX)

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)

        # Get basic properties we need.
        sens_min, sens_max = props['android.sensor.info.sensitivityRange']
        sens_max_analog = props['android.sensor.maxAnalogSensitivity']
        sens_max_meas = sens_max_analog
        white_level = props['android.sensor.info.whiteLevel']

        print 'Sensitivity range: [%f, %f]' % (sens_min, sens_max)
        print 'Max analog sensitivity: %f' % (sens_max_analog)

        # Do AE to get a rough idea of where we are.
        s_ae, e_ae, _, _, _ = cam.do_3a(
                get_results=True, do_awb=False, do_af=False)
        # Underexpose to get more data for low signal levels.
        auto_e = s_ae * e_ae / bracket_factor
        # Focus at zero to intentionally blur the scene as much as possible.
        f_dist = 0.0

        # If the auto-exposure result is too bright for the highest
        # sensitivity or too dark for the lowest sensitivity, report
        # an error.
        min_exposure_ns, max_exposure_ns = props[
                'android.sensor.info.exposureTimeRange']
        if auto_e < min_exposure_ns*sens_max_meas:
            raise its.error.Error('Scene is too bright to properly expose '
                                  'at the highest sensitivity')
        if auto_e*bracket_factor > max_exposure_ns*sens_min:
            raise its.error.Error('Scene is too dark to properly expose '
                                  'at the lowest sensitivity')

        # Start the sensitivities at the minimum.
        s = sens_min

        samples = [[], [], [], []]
        plots = []
        measured_models = [[], [], [], []]
        color_plane_plots = {}
        while int(round(s)) <= sens_max_meas:
            s_int = int(round(s))
            print 'ISO %d' % s_int
            fig, [[plt_r, plt_gr], [plt_gb, plt_b]] = plt.subplots(2, 2, figsize=(11, 11))
            fig.gca()
            color_plane_plots[s_int] = [plt_r, plt_gr, plt_gb, plt_b]
            fig.suptitle('ISO %d' % s_int, x=0.54, y=0.99)
            for i, plot in enumerate(color_plane_plots[s_int]):
                plot.set_title('%s' % BAYER_LIST[i])
                plot.set_xlabel('Mean signal level')
                plot.set_ylabel('Variance')

            samples_s = [[], [], [], []]
            for b in range(BRACKET_MAX):
                # Get the exposure for this sensitivity and exposure time.
                e = int(math.pow(2, b)*auto_e/float(s))
                print 'exp %.3fms' % round(e*1.0E-6, 3)
                req = its.objects.manual_capture_request(s_int, e, f_dist)
                fmt_raw = {'format': 'rawStats',
                           'gridWidth': TILE_SIZE,
                           'gridHeight': TILE_SIZE}
                cap = cam.do_capture(req, fmt_raw)
                mean_image, var_image = its.image.unpack_rawstats_capture(cap)
                idxs = its.image.get_canonical_cfa_order(props)
                means = [mean_image[:, :, i] for i in idxs]
                vars_ = [var_image[:, :, i] for i in idxs]

                s_read = cap['metadata']['android.sensor.sensitivity']
                s_err = 's_write: %d, s_read: %d, RTOL: %.2f' % (
                        s, s_read, RTOL_EXP_GAIN)
                assert (1.0 >= s_read/float(s_int) >= RTOL_EXP_GAIN), s_err
                print 'ISO_write: %d, ISO_read: %d' %  (s_int, s_read)

                for pidx in range(len(means)):
                    plot = color_plane_plots[s_int][pidx]

                    # convert_capture_to_planes normalizes the range
                    # to [0, 1], but without subtracting the black
                    # level.
                    black_level = its.image.get_black_level(
                            pidx, props, cap['metadata'])
                    means_p = (means[pidx] - black_level)/(white_level - black_level)
                    vars_p = vars_[pidx]/((white_level - black_level)**2)

                    # TODO(dsharlet): It should be possible to account for low
                    # frequency variation by looking at neighboring means, but I
                    # have not been able to make this work.

                    means_p = np.asarray(means_p).flatten()
                    vars_p = np.asarray(vars_p).flatten()

                    samples_e = []
                    for (mean, var) in zip(means_p, vars_p):
                        # Don't include the tile if it has samples that might
                        # be clipped.
                        if mean + 2*math.sqrt(max(var, 0)) < MAX_SIGNAL_VALUE:
                            samples_e.append([mean, var])

                    if samples_e:
                        means_e, vars_e = zip(*samples_e)
                        color_plane_plots[s_int][pidx].plot(
                                means_e, vars_e, COLORS[b%len(COLORS)] + '.',
                                alpha=0.5, markersize=1)
                        samples_s[pidx].extend(samples_e)

            for (pidx, p) in enumerate(samples_s):
                [S, O, R, _, _] = scipy.stats.linregress(samples_s[pidx])
                measured_models[pidx].append([s_int, S, O])
                print "Sensitivity %d: %e*y + %e (R=%f)" % (s_int, S, O, R)

                # Add the samples for this sensitivity to the global samples list.
                samples[pidx].extend([(s_int, mean, var) for (mean, var) in samples_s[pidx]])

                # Add the linear fit to subplot for this sensitivity.
                color_plane_plots[s_int][pidx].plot(
                        [0, MAX_SIGNAL_VALUE], [O, O + S*MAX_SIGNAL_VALUE],
                        'rgkb'[pidx]+'--', label='Linear fit')

                xmax = max([max([x for (x, _) in p]) for p in samples_s])*MAX_SCALE_FUDGE
                ymax = (O + S*xmax)*MAX_SCALE_FUDGE
                color_plane_plots[s_int][pidx].set_xlim(xmin=0, xmax=xmax)
                color_plane_plots[s_int][pidx].set_ylim(ymin=0, ymax=ymax)
                color_plane_plots[s_int][pidx].legend()
                pylab.tight_layout()

            fig.savefig('%s_samples_iso%04d.png' % (NAME, s_int))
            plots.append([s_int, fig])

            # Move to the next sensitivity.
            s *= math.pow(2, 1.0/STEPS_PER_STOP)

        # do model plots
        (fig, (plt_S, plt_O)) = plt.subplots(2, 1, figsize=(11, 8.5))
        plt_S.set_title("Noise model")
        plt_S.set_ylabel("S")
        plt_O.set_xlabel("ISO")
        plt_O.set_ylabel("O")

        A = []
        B = []
        C = []
        D = []
        for (pidx, p) in enumerate(measured_models):
            # Grab the sensitivities and line parameters from each sensitivity.
            S_measured = [e[1] for e in measured_models[pidx]]
            O_measured = [e[2] for e in measured_models[pidx]]
            sens = np.asarray([e[0] for e in measured_models[pidx]])
            sens_sq = np.square(sens)

            # Use a global linear optimization to fit the noise model.
            gains = np.asarray([s[0] for s in samples[pidx]])
            means = np.asarray([s[1] for s in samples[pidx]])
            vars_ = np.asarray([s[2] for s in samples[pidx]])
            gains = gains.flatten()
            means = means.flatten()
            vars_ = vars_.flatten()

            # Define digital gain as the gain above the max analog gain
            # per the Camera2 spec. Also, define a corresponding C
            # expression snippet to use in the generated model code.
            digital_gains = np.maximum(gains/sens_max_analog, 1)
            assert np.all(digital_gains == 1)
            digital_gain_cdef = '(sens / %d.0) < 1.0 ? 1.0 : (sens / %d.0)' % \
                (sens_max_analog, sens_max_analog)

            # Divide the whole system by gains*means.
            f = lambda x, a, b, c, d: (c*(x[0]**2) + d + (x[1])*a*x[0] + (x[1])*b)/(x[0])
            [result, _] = scipy.optimize.curve_fit(f, (gains, means), vars_/(gains))

            [A_p, B_p, C_p, D_p] = result[0:4]
            A.append(A_p)
            B.append(B_p)
            C.append(C_p)
            D.append(D_p)

            # Plot the noise model components with the values predicted by the
            # noise model.
            S_model = A_p*sens + B_p
            O_model = \
                C_p*sens_sq + D_p*np.square(np.maximum(sens/sens_max_analog, 1))

            plt_S.loglog(sens, S_measured, 'rgkb'[pidx]+'+', basex=10, basey=10,
                         label='Measured')
            plt_S.loglog(sens, S_model, 'rgkb'[pidx]+'x', basex=10, basey=10,
                         label='Model')
            plt_O.loglog(sens, O_measured, 'rgkb'[pidx]+'+', basex=10, basey=10,
                         label='Measured')
            plt_O.loglog(sens, O_model, 'rgkb'[pidx]+'x', basex=10, basey=10,
                         label='Model')
        plt_S.legend()
        plt_O.legend()

        fig.savefig('%s.png' % (NAME))

        # add models to subplots and re-save
        for [s, fig] in plots:  # re-step through figs...
            dg = max(s/sens_max_analog, 1)
            fig.gca()
            for (pidx, p) in enumerate(measured_models):
                S = A[pidx]*s + B[pidx]
                O = C[pidx]*s*s + D[pidx]*dg*dg
                color_plane_plots[s][pidx].plot(
                        [0, MAX_SIGNAL_VALUE], [O, O + S*MAX_SIGNAL_VALUE],
                        'rgkb'[pidx]+'-', label='Model', alpha=0.5)
                color_plane_plots[s][pidx].legend(loc='upper left')
            fig.savefig('%s_samples_iso%04d.png' % (NAME, s))

        # Generate the noise model implementation.
        A_array = ",".join([str(i) for i in A])
        B_array = ",".join([str(i) for i in B])
        C_array = ",".join([str(i) for i in C])
        D_array = ",".join([str(i) for i in D])
        noise_model_code = textwrap.dedent("""\
            /* Generated test code to dump a table of data for external validation
             * of the noise model parameters.
             */
            #include <stdio.h>
            #include <assert.h>
            double compute_noise_model_entry_S(int plane, int sens);
            double compute_noise_model_entry_O(int plane, int sens);
            int main(void) {
                for (int plane = 0; plane < %d; plane++) {
                    for (int sens = %d; sens <= %d; sens += 100) {
                        double o = compute_noise_model_entry_O(plane, sens);
                        double s = compute_noise_model_entry_S(plane, sens);
                        printf("%%d,%%d,%%lf,%%lf\\n", plane, sens, o, s);
                    }
                }
                return 0;
            }

            /* Generated functions to map a given sensitivity to the O and S noise
             * model parameters in the DNG noise model. The planes are in
             * R, Gr, Gb, B order.
             */
            double compute_noise_model_entry_S(int plane, int sens) {
                static double noise_model_A[] = { %s };
                static double noise_model_B[] = { %s };
                double A = noise_model_A[plane];
                double B = noise_model_B[plane];
                double s = A * sens + B;
                return s < 0.0 ? 0.0 : s;
            }

            double compute_noise_model_entry_O(int plane, int sens) {
                static double noise_model_C[] = { %s };
                static double noise_model_D[] = { %s };
                double digital_gain = %s;
                double C = noise_model_C[plane];
                double D = noise_model_D[plane];
                double o = C * sens * sens + D * digital_gain * digital_gain;
                return o < 0.0 ? 0.0 : o;
            }
            """ % (len(A), sens_min, sens_max, A_array, B_array, C_array, D_array, digital_gain_cdef))
        print noise_model_code
        for i, _ in enumerate(BAYER_LIST):
            read_noise = C[i] * sens_min * sens_min + D[i]
            e_msg = '%s model min ISO noise < 0! C: %.4e, D: %.4e, rn: %.4e' % (
                    BAYER_LIST[i], C[i], D[i], read_noise)
            assert read_noise > 0, e_msg
            assert C[i] > 0, '%s model slope is negative. slope=%.4e' % (
                    BAYER_LIST[i], C[i])
        text_file = open("noise_model.c", "w")
        text_file.write("%s" % noise_model_code)
        text_file.close()

if __name__ == '__main__':
    main()
