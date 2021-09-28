# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An adapter to remotely access the DUT graphics facade."""


class GraphicsFacadeRemoteAdapter(object):
    """GraphicsFacadeRemoteAdapter remotely monitors graphics hangs."""


    def __init__(self, remote_facade_proxy):
        """Construct a GraphicsFacadeRemoteAdapter.

        @param remote_facade_proxy: RemoteFacadeProxy object.

        """
        self._proxy = remote_facade_proxy


    @property
    def _graphics_proxy(self):
        """Gets the proxy to DUT USB facade.

        @return XML RPC proxy to DUT graphics facade.

        """
        return self._proxy.graphics


    def graphics_state_checker_initialize(self):
        """Create and initialize the graphics state checker object.

        This will establish existing errors and take a snapshot of graphics
        kernel memory.

        """
        self._graphics_proxy.graphics_state_checker_initialize()


    def graphics_state_checker_finalize(self):
        """Throw an error on new GPU hang messages in system logs."""
        self._graphics_proxy.graphics_state_checker_finalize()
