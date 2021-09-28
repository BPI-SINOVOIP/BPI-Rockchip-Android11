#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import socket

from fake_printer import FakePrinter


def main():
    """
    Unit tests for fake_printer.py.

    """
    # Simple unit test - it should start and stop without errors
    p = FakePrinter(12345)
    p.stop()

    # The same test but other way
    with FakePrinter(23456) as p:
        pass

    # Another test - let's try to send something
    message = 'lkds;fsdjfsdjflsdjfsd;lfsad;adfsfa324dsfcxvdsvdf'
    port = 12345
    with FakePrinter(port) as printer:
        # Opens a socket and sends the message
        my_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        my_socket.connect( ('localhost', port) )
        my_socket.send(message)
        my_socket.close()
        # Reads the message from the FakePrinter
        doc = printer.fetch_document(10)
        # The printer is stopped at the end of "with" statement
    assert message == doc


if __name__ == '__main__':
    main()
