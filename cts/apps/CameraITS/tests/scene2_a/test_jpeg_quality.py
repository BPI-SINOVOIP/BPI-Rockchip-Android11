# Copyright 2020 The Android Open Source Project
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

from matplotlib import pylab
import matplotlib.pyplot
import numpy as np

JPEG_APPN_MARKERS = [[255, 224], [255, 225], [255, 226], [255, 227], [255, 228],
                     [255, 235]]
JPEG_DHT_MARKER = [255, 196]  # JPEG Define Huffman Table
JPEG_DQT_MARKER = [255, 219]  # JPEG Define Quantization Table
JPEG_DQT_TOL = 0.8  # -20% for each +20 in jpeg.quality (empirical number)
JPEG_EOI_MARKER = [255, 217]  # JPEG End of Image
JPEG_SOI_MARKER = [255, 216]  # JPEG Start of Image
JPEG_SOS_MARKER = [255, 218]  # JPEG Start of Scan
NAME = os.path.basename(__file__).split('.')[0]
QUALITIES = [25, 45, 65, 85]
SYMBOLS = ['o', 's', 'v', '^', '<', '>']


def is_square(integer):
    root = math.sqrt(integer)
    return integer == int(root + 0.5) ** 2


def strip_soi_marker(jpeg):
    """strip off start of image marker.

    SOI is of form [xFF xD8] and JPEG needs to start with marker.

    Args:
        jpeg:   1-D numpy int [0:255] array; values from JPEG capture

    Returns:
        jpeg with SOI marker stripped off.
    """

    soi = jpeg[0:2]
    assert list(soi) == JPEG_SOI_MARKER, 'JPEG has no Start Of Image marker'
    return jpeg[2:]


def strip_appn_data(jpeg):
    """strip off application specific data at beginning of JPEG.

    APPN markers are of form [xFF, xE*, size_msb, size_lsb] and should follow
    SOI marker.

    Args:
        jpeg:   1-D numpy int [0:255] array; values from JPEG capture

    Returns:
        jpeg with APPN marker(s) and data stripped off.
    """

    length = 0
    i = 0
    # find APPN markers and strip off payloads at beginning of jpeg
    while i < len(jpeg)-1:
        if [jpeg[i], jpeg[i+1]] in JPEG_APPN_MARKERS:
            length = jpeg[i+2] * 256 + jpeg[i+3] + 2
            print ' stripped APPN length:', length
            jpeg = np.concatenate((jpeg[0:i], jpeg[length:]), axis=None)
        elif ([jpeg[i], jpeg[i+1]] == JPEG_DQT_MARKER or
              [jpeg[i], jpeg[i+1]] == JPEG_DHT_MARKER):
            break
        else:
            i += 1

    return jpeg


def find_dqt_markers(marker, jpeg):
    """Find location(s) of marker list in jpeg.

    DQT marker is of form [xFF, xDB].

    Args:
        marker: list; marker values
        jpeg:   1-D numpy int [0:255] array; JPEG capture w/ SOI & APPN stripped

    Returns:
        locs:       list; marker locations in jpeg
    """
    locs = []
    marker_len = len(marker)
    for i in xrange(len(jpeg)-marker_len+1):
        if list(jpeg[i:i+marker_len]) == marker:
            locs.append(i)
    return locs


def extract_dqts(jpeg, debug=False):
    """Find and extract the DQT info in the JPEG.

    SOI marker and APPN markers plus data are stripped off front of JPEG.
    DQT marker is of form [xFF, xDB] followed by [size_msb, size_lsb].
    Size includes the size values, but not the marker values.
    Luma DQT is prefixed by 0, Chroma DQT by 1.
    DQTs can have both luma & chroma or each individually.
    There can be more than one DQT table for luma and chroma.

    Args:
        jpeg:   1-D numpy int [0:255] array; values from JPEG capture
        debug:  bool; command line flag to print debug data

    Returns:
        lumas, chromas: lists of numpy means of luma & chroma DQT matrices.
                        Higher values represent higher compression.
    """

    dqt_markers = find_dqt_markers(JPEG_DQT_MARKER, jpeg)
    print 'DQT header loc(s):', dqt_markers
    lumas = []
    chromas = []
    for i, dqt in enumerate(dqt_markers):
        if debug:
            print '\n DQT %d start: %d, marker: %s, length: %s' % (
                    i, dqt, jpeg[dqt:dqt+2], jpeg[dqt+2:dqt+4])
        dqt_size = jpeg[dqt+2]*256 + jpeg[dqt+3] - 2  # strip off size marker
        if dqt_size % 2 == 0:  # even payload means luma & chroma
            print ' both luma & chroma DQT matrices in marker'
            dqt_size = (dqt_size - 2) / 2  # subtact off luma/chroma markers
            assert is_square(dqt_size), 'DQT size: %d' % dqt_size
            luma_start = dqt + 5  # skip header, length, & matrix id
            chroma_start = luma_start + dqt_size + 1  # skip lumen &  matrix_id
            luma = np.array(jpeg[luma_start:luma_start+dqt_size])
            chroma = np.array(jpeg[chroma_start:chroma_start+dqt_size])
            lumas.append(np.mean(luma))
            chromas.append(np.mean(chroma))
            if debug:
                h = int(math.sqrt(dqt_size))
                print ' luma:', luma.reshape(h, h)
                print ' chroma:', chroma.reshape(h, h)
        else:  # odd payload means only 1 matrix
            print ' single DQT matrix in marker'
            dqt_size = dqt_size - 1  # subtract off luma/chroma marker
            assert is_square(dqt_size), 'DQT size: %d' % dqt_size
            start = dqt + 5
            matrix = np.array(jpeg[start:start+dqt_size])
            if jpeg[dqt+4]:  # chroma == 1
                chromas.append(np.mean(matrix))
                if debug:
                    h = int(math.sqrt(dqt_size))
                    print ' chroma:', matrix.reshape(h, h)
            else:  # luma == 0
                lumas.append(np.mean(matrix))
                if debug:
                    h = int(math.sqrt(dqt_size))
                    print ' luma:', matrix.reshape(h, h)

    return lumas, chromas


def plot_data(qualities, lumas, chromas):
    """Create plot of data."""
    print 'qualities: %s' % str(qualities)
    print 'luma DQT avgs: %s' % str(lumas)
    print 'chroma DQT avgs: %s' % str(chromas)
    pylab.title(NAME)
    for i in range(lumas.shape[1]):
        pylab.plot(qualities, lumas[:, i], '-g'+SYMBOLS[i],
                   label='luma_dqt'+str(i))
        pylab.plot(qualities, chromas[:, i], '-r'+SYMBOLS[i],
                   label='chroma_dqt'+str(i))
    pylab.xlim([0, 100])
    pylab.ylim([0, None])
    pylab.xlabel('jpeg.quality')
    pylab.ylabel('DQT luma/chroma matrix averages')
    pylab.legend(loc='upper right', numpoints=1, fancybox=True)
    matplotlib.pyplot.savefig('%s_plot.png' % NAME)


def main():
    """Test the camera JPEG compression quality.

    Step JPEG qualities through android.jpeg.quality. Ensure quanitization
    matrix decreases with quality increase. Matrix should decrease as the
    matrix represents the division factor. Higher numbers --> fewer quantization
    levels.
    """

    # determine debug
    debug = its.caps.debug_mode()

    # init variables
    lumas = []
    chromas = []

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.jpeg_quality(props))
        cam.do_3a()

        # do captures over jpeg quality range
        req = its.objects.auto_capture_request()
        for q in QUALITIES:
            print '\njpeg.quality: %.d' % q
            req['android.jpeg.quality'] = q
            cap = cam.do_capture(req, cam.CAP_JPEG)
            jpeg = cap['data']

            # strip off start of image
            jpeg = strip_soi_marker(jpeg)

            # strip off application specific data
            jpeg = strip_appn_data(jpeg)
            print 'remaining JPEG header:', jpeg[0:4]

            # find and extract DQTs
            lumas_i, chromas_i = extract_dqts(jpeg, debug)
            lumas.append(lumas_i)
            chromas.append(chromas_i)

            # save JPEG image
            img = its.image.convert_capture_to_rgb_image(cap, props=props)
            its.image.write_image(img, '%s_%d.jpg' % (NAME, q))

    # turn lumas/chromas into np array to ease multi-dimensional plots/asserts
    lumas = np.array(lumas)
    chromas = np.array(chromas)

    # create plot of luma & chroma averages vs quality
    plot_data(QUALITIES, lumas, chromas)

    # assert decreasing luma/chroma with improved jpeg quality
    for i in range(lumas.shape[1]):
        l = lumas[:, i]
        c = chromas[:, i]
        emsg = 'luma DQT avgs: %s, TOL: %.1f' % (str(l), JPEG_DQT_TOL)
        assert all(y < x * JPEG_DQT_TOL for x, y in zip(l, l[1:])), emsg
        emsg = 'chroma DQT avgs: %s, TOL: %.1f' % (str(c), JPEG_DQT_TOL)
        assert all(y < x * JPEG_DQT_TOL for x, y in zip(c, c[1:])), emsg


if __name__ == '__main__':
    main()
