"""Provides utilities to support bluetooth adapter tests"""

from debug_linux_keymap import linux_input_keymap

from autotest_lib.client.bin.input.linux_input import EV_KEY
from ast import literal_eval as make_tuple
import logging


def reconstruct_string(events):
    """ Tries to reconstruct a string from linux input in a simple way

    @param events: list of event objects received over the BT channel

    @returns: reconstructed string
    """
    recon = []

    for ev in events:
        # If it's a key pressed event
        if ev.type == EV_KEY and ev.value == 1:
            recon.append(linux_input_keymap.get(ev.code, "_"))

    return "".join(recon)


def parse_trace_file(filename):
    """ Reads contents of trace file

    @param filename: location of trace file on disk

    @returns: structure containing contents of filename
    """

    contents = []

    try:
        with open(filename, 'r') as mf:
            for line in mf:
                # Reconstruct tuple and add to trace
                contents.append(make_tuple(line))
    except EnvironmentError:
        logging.error('Unable to open file %s', filename)
        return None

    return contents