#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from multithreaded_processor import MultithreadedProcessor


class EvenNumbersGenerator():
    """
    An example and a simple test for multithreaded_processor.py.

    These class is supposed to generate a table of even numbers using
    many threads.

    """
    def __init__(self):
        # creates a processor class with 13 threads
        self._processor = MultithreadedProcessor(13)

    def getEvenNumbers(self, count):
        """
        @param count: a count of even numbers to return

        @returns a list of first 'count' even numbers in ascending order

        """
        # prepares input data
        self._data = range(count)
        # runs the calculations and returns an output
        return self._processor.run(self._thread, count)

    def _thread(self, task_id):
        # calculates single even number
        return (self._data[task_id]*2)


def main():
    """
    Unit test for MultithreadedProcessor

    """
    # runs the test
    gen = EvenNumbersGenerator()
    evenNumbers = gen.getEvenNumbers(12345)
    # checks the output
    assert len(evenNumbers) == 12345
    for i, number in enumerate(evenNumbers):
        assert number == i*2


if __name__ == '__main__':
    main()
