# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for display info."""

class DisplayInfo(object):
    """The class match displayInfo object from chrome.system.display API.
    """

    class Bounds(object):
        def __init__(self, d):
            """The class match Bounds object from chrome.system.display API.

            @param d: Map of display properties.
            """

            self.left = d['left']
            self.top = d['top']
            self.width = d['width']
            self.height = d['height']


    class Insets(object):
        def __init__(self, d):
            """The class match Insets object from chrome.system.display API.

            @param d: Map of display properties.
            """

            self.left = d['left']
            self.top = d['top']
            self.right = d['right']
            self.bottom = d['bottom']


    class Edid(object):
        def __init__(self, edid):
            """The class match the Edid object from chrome.system.display API.

            @param edid: Map of Edid properties.
            """

            self.manufacturer_id = edid['manufacturerId']
            self.year_of_manufacture = edid['yearOfManufacture']
            self.product_id = edid['productId']


    def __init__(self, d):
        self.display_id = d['id']
        self.name = d['name']
        self.mirroring_source_id = d['mirroringSourceId']
        self.is_primary = d['isPrimary']
        self.is_internal = d['isInternal']
        self.is_enabled = d['isEnabled']
        self.dpi_x = d['dpiX']
        self.dpi_y = d['dpiY']
        self.rotation = d['rotation']
        self.bounds = self.Bounds(d['bounds'])
        self.overscan = self.Insets(d['overscan'])
        self.work_area = self.Bounds(d['workArea'])
        self.edid = self.Edid(d['edid'])
