#!/usr/bin/env python2
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import logging
import os
import re
import sys

import common   # pylint: disable=unused-import

from autotest_lib.client.common_lib import utils


class ImageGeneratorError(Exception):
    """Error in ImageGenerator."""
    pass


class ImageGenerator(object):
    """A class to generate the calibration images with different sizes.

    It generates the SVG images which are easy to be produced by changing its
    XML text content.

    """

    TEMPLATE_WIDTH = 1680
    TEMPLATE_HEIGHT = 1052
    TEMPLATE_FILENAME = 'template-%dx%d.svg' % (TEMPLATE_WIDTH, TEMPLATE_HEIGHT)
    GS_URL = ('http://commondatastorage.googleapis.com/chromeos-localmirror/'
              'distfiles/chameleon_calibration_images')


    def __init__(self):
        """Construct an ImageGenerator.
        """
        module_dir = os.path.dirname(sys.modules[__name__].__file__)
        template_path = os.path.join(module_dir, 'calibration_images',
                                     self.TEMPLATE_FILENAME)
        self._image_template = open(template_path).read()


    def generate_image(self, width, height, filename):
        """Generate the image with the given width and height.

        @param width: The width of the image.
        @param height: The height of the image.
        @param filename: The filename to output, svg file or png file.
        """
        if filename.endswith('.svg'):
            with open(filename, 'w+') as f:
                logging.debug('Generate the image with size %dx%d to %s',
                              width, height, filename)
                f.write(self._image_template.format(
                        scale_width=float(width)/self.TEMPLATE_WIDTH,
                        scale_height=float(height)/self.TEMPLATE_HEIGHT))
        elif filename.endswith('.png'):
            pregen_filename = 'image-%dx%d.png' % (width, height)
            pregen_path = os.path.join(self.GS_URL, pregen_filename)
            logging.debug('Fetch the image from: %s', pregen_path)
            if utils.system('wget -q -O %s %s' % (filename, pregen_path),
                            ignore_status=True):
                raise ImageGeneratorError('Failed to fetch the image: %s' %
                                          pregen_filename)
        else:
            raise ImageGeneratorError('The image format not supported')

    @staticmethod
    def get_extrema(image):
        """Returns a 2-tuple containing minimum and maximum values of the image.

        @param image: the calibration image projected by DUT.
        @return a tuple of (minimum, maximum)
        """
        w, h = image.size
        min_value = 255
        max_value = 0
        # scan the middle vertical line
        x = w / 2
        for i in range(0, h/2):
            v = image.getpixel((x, i))[0]
            if v < min_value:
                min_value = v
            if v > max_value:
                max_value = v
        return (min_value, max_value)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=str,
                        help='filename of the calibration image')
    args = parser.parse_args()

    matched = re.search(r'image-(\d+)x(\d+).(png|svg)', args.output)
    if not matched:
        logging.error('The filename should be like: image-1920x1080.svg')
        parser.print_help()
        sys.exit(errno.EINVAL)

    width = int(matched.group(1))
    height = int(matched.group(2))
    generator = ImageGenerator()
    generator.generate_image(width, height, args.output)
