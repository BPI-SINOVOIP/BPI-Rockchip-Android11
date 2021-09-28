# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import xml.etree.ElementTree as ET

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import arc


class ArcDeviceCapability(object):
    """
    Generates capabilities status on DUT from media_codecs.xml in Android
    container. Answers from the capabilities whether some capability is
    satisfied on DUT.
    """
    MEDIA_CODECS_XML_PATH = '/vendor/etc/media_codecs.xml'
    MEDIA_CODEC_TO_CAPABILITY = {
        'ARC.h264.decode': 'hw_dec_h264_arc',
        'ARC.vp8.decode' : 'hw_dec_vp8_arc',
        'ARC.vp9.decode' : 'hw_dec_vp9_arc',
        'OMX.arc.h264.encode': 'hw_enc_h264_arc',
        'OMX.arc.vp8.encode' : 'hw_enc_vp8_arc',
        'OMX.arc.vp9.encode' : 'hw_enc_vp9_arc',
    }


    def __init__(self):
        self.capabilities = self._get_arc_capability()

    def _get_arc_capability(self):
        """
        Reads media_codecs.xml in android container and returns available ARC++
        capability.

        @returns list: Available ARC++ capabilities on DUT.
        """
        if not arc.is_android_container_alive():
            raise error.TestFail('Android container is not alive.')

        # Reads media_codecs.xml
        media_codecs_data = arc.read_android_file(self.MEDIA_CODECS_XML_PATH)
        if not media_codecs_data:
            raise error.TestFail('media_codecs.xml is empty.')

        logging.debug('%s', media_codecs_data)

        # Parses media_codecs.xml
        root = ET.fromstring(media_codecs_data)
        arc_caps = dict.fromkeys(self.MEDIA_CODEC_TO_CAPABILITY.values(), 'no')
        for mc in root.iter('MediaCodec'):
            codec_name = mc.get('name')
            if codec_name in self.MEDIA_CODEC_TO_CAPABILITY:
                arc_caps[self.MEDIA_CODEC_TO_CAPABILITY[codec_name]] = 'yes'
        logging.debug('%r', arc_caps)
        return arc_caps

    def get_capability(self, cap):
        """
        Decides if a device satisfies a required capability for an autotest.

        @param cap: string, denoting one capability. It must be one of
                    self.MEDIA_CODEC_TO_CAPABILITY.
        @returns 'yes' or 'no'.
        """
        try:
            return self.capabilities[cap]
        except KeyError:
            raise error.TestFail('Unexpected capability: %s' % cap)

    def ensure_capability(self, cap):
        """
        Raises TestNAError if a device doesn't satisfy cap.

        @param cap: string, denoting one capability.
        """
        if not have_capability(cap):
            raise error.TestNAError('Missing Capability: %s' % cap)

    def have_capability(self, cap):
        """
        Return whether cap is available.
        """
        return self.get_capability(cap) == 'yes'
