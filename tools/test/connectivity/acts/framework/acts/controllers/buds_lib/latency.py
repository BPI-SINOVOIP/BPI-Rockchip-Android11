#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

"""Processes profiling data to output latency numbers."""
#
# Type "python latency.py -h" for help
#
# Currently the log data is assumed to be in the following format:
# PROF:<event-id> <timestamp>
# The <event-id> and <timestamp> can be in the form of any valid
# (positive)integer literal in Python
# Examples:
#   PROF:0x0006 0x00000155e0d043f1
#   PROF:6 1468355593201

import argparse
from collections import defaultdict
import csv
import logging
import math
import os
import string
import xml.etree.ElementTree as ET

valid_fname_chars = '-_.()%s%s' % (string.ascii_letters, string.digits)
PERCENTILE_STEP = 1
PROFILER_DATA_PREFIX = 'PROF:'


class EventPair(object):

    def __init__(self, pair_id, latency, name):
        self.pair_id = pair_id
        self.latency = latency
        self.name = name


class LatencyEntry(object):

    def __init__(self, start_timestamp, latency):
        self.start_timestamp = start_timestamp
        self.latency = latency


def parse_xml(xml_file):
    """
    Parse the configuration xml file.

    Returns:
      event_pairs_by_pair_id: dict mapping event id to event pair object
      event_pairs_by_start_id: dict mapping starting event to list of event pairs
                               with that starting event.
      event_pairs_by_end_id: dict mapping ending event to list of event pairs
                             with that ending event.
    """
    root = ET.parse(xml_file).getroot()
    event_pairs = root.findall('event-pair')
    event_pairs_by_pair_id = {}
    event_pairs_by_start_id = defaultdict(list)
    event_pairs_by_end_id = defaultdict(list)

    for event_pair in event_pairs:
        start_evt = root.find(
            "./event[@id='{0:}']".format(event_pair.attrib['start-event']))
        end_evt = root.find(
            "./event[@id='{0:}']".format(event_pair.attrib['end-event']))
        start = int(start_evt.attrib['id'], 0)
        end = int(end_evt.attrib['id'], 0)
        paird_id = start << 32 | end
        if paird_id in event_pairs_by_pair_id:
            logging.error('Latency event repeated: start id = %d, end id = %d',
                          start,
                          end)
            continue
        # Create the output file name base by concatenating:
        # "input file name base" + start event name + "_to_" + end event name
        evt_pair_name = start_evt.attrib['name'] + '_to_' + end_evt.attrib[
            'name']
        evt_pair_name = [
            c if c in valid_fname_chars else '_' for c in evt_pair_name
        ]
        evt_pair_name = ''.join(evt_pair_name)
        evt_list = EventPair(paird_id, 0, evt_pair_name)
        event_pairs_by_pair_id[paird_id] = evt_list
        event_pairs_by_start_id[start].append(evt_list)
        event_pairs_by_end_id[end].append(evt_list)
    return (event_pairs_by_pair_id, event_pairs_by_start_id,
            event_pairs_by_end_id)


def percentile_to_index(num_entries, percentile):
    """
    Returns the index in an array corresponding to a percentile.

    Arguments:
      num_entries: the number of entries in the array.
      percentile: which percentile to calculate the index for.
    Returns:
      ind: the index in the array corresponding to the percentile.
    """
    ind = int(math.floor(float(num_entries) * percentile / 100))
    if ind > 0:
        ind -= 1
    return ind


def compute_latencies(input_file, event_pairs_by_start_id,
                      event_pairs_by_end_id):
    """Parse the input data file and compute latencies."""
    line_num = 0
    lat_tables_by_pair_id = defaultdict(list)
    while True:
        line_num += 1
        line = input_file.readline()
        if not line:
            break
        data = line.partition(PROFILER_DATA_PREFIX)[2]
        if not data:
            continue
        try:
            event_id, timestamp = [int(x, 0) for x in data.split()]
        except ValueError:
            logging.error('Badly formed event entry at line #%s: %s', line_num,
                          line)
            continue
        # We use event_pair.latency to temporarily store the timestamp
        # of the start event
        for event_pair in event_pairs_by_start_id[event_id]:
            event_pair.latency = timestamp
        for event_pair in event_pairs_by_end_id[event_id]:
            # compute the latency only if we have seen the corresponding
            # start event already
            if event_pair.latency:
                lat_tables_by_pair_id[event_pair.pair_id].append(
                    LatencyEntry(event_pair.latency,
                                 timestamp - event_pair.latency))
                event_pair.latency = 0
    return lat_tables_by_pair_id


def write_data(fname_base, event_pairs_by_pair_id, lat_tables_by_pair_id):
    for event_id, lat_table in lat_tables_by_pair_id.items():
        event_pair = event_pairs_by_pair_id[event_id]
        with open(fname_base + '_' + event_pair.name + '_data.csv',
                  'wb') as out_file:
            csv_writer = csv.writer(out_file)
            for dat in lat_table:
                csv_writer.writerow([dat.start_timestamp, dat.latency])


def write_summary(fname_base, event_pairs_by_pair_id, lat_tables_by_pair_id):
    summaries = get_summaries(event_pairs_by_pair_id, lat_tables_by_pair_id)
    for event_id, lat_table in lat_tables_by_pair_id.items():
        event_pair = event_pairs_by_pair_id[event_id]
        summary = summaries[event_pair.name]
        latencies = summary['latencies']
        num_latencies = summary['num_latencies']
        with open(fname_base + '_' + event_pair.name + '_summary.txt',
                  'wb') as out_file:
            csv_writer = csv.writer(out_file)
            csv_writer.writerow(['Percentile', 'Latency'])
            # Write percentile table
            for percentile in range(1, 101):
                ind = percentile_to_index(num_latencies, percentile)
                csv_writer.writerow([percentile, latencies[ind]])

            # Write summary
            print('\n\nTotal number of samples = {}'.format(num_latencies),
                  file=out_file)
            print('Min = {}'.format(summary['min_lat']), file=out_file)
            print('Max = {}'.format(summary['max_lat']), file=out_file)
            print('Average = {}'.format(summary['average_lat']), file=out_file)
            print('Median = {}'.format(summary['median_lat']), file=out_file)
            print('90 %ile = {}'.format(summary['90pctile']), file=out_file)
            print('95 %ile = {}'.format(summary['95pctile']), file=out_file)


def process_latencies(config_xml, input_file):
    """
    End to end function to compute latencies and summaries from input file.
    Writes latency results to files in current directory.

    Arguments:
       config_xml: xml file specifying which event pairs to compute latency
                   btwn.
       input_file: text file containing the timestamped events, like a log file.
    """
    # Parse the event configuration file
    (event_pairs_by_pair_id, event_pairs_by_start_id,
     event_pairs_by_end_id) = parse_xml(config_xml)
    # Compute latencies
    lat_tables_by_pair_id = compute_latencies(input_file,
                                              event_pairs_by_start_id,
                                              event_pairs_by_end_id)
    fname_base = os.path.splitext(os.path.basename(input_file.name))[0]
    # Write the latency data and summary to respective files
    write_data(fname_base, event_pairs_by_pair_id, lat_tables_by_pair_id)
    write_summary(fname_base, event_pairs_by_pair_id, lat_tables_by_pair_id)


def get_summaries(event_pairs_by_pair_id, lat_tables_by_pair_id):
    """
    Process significant summaries from a table of latencies.

    Arguments:
      event_pairs_by_pair_id: dict mapping event id to event pair object
      lat_tables_by_pair_id: dict mapping event id to latency table
    Returns:
      summaries: dict mapping event pair name to significant summary metrics.
    """
    summaries = {}
    for event_id, lat_table in lat_tables_by_pair_id.items():
        event_summary = {}
        event_pair = event_pairs_by_pair_id[event_id]
        latencies = [entry.latency for entry in lat_table]
        latencies.sort()
        event_summary['latencies'] = latencies
        event_summary['num_latencies'] = len(latencies)
        event_summary['min_lat'] = latencies[0]
        event_summary['max_lat'] = latencies[-1]
        event_summary['average_lat'] = sum(latencies) / len(latencies)
        event_summary['median'] = latencies[len(latencies) // 2]
        event_summary['90pctile'] = latencies[percentile_to_index(
            len(latencies), 90)]
        event_summary['95pctile'] = latencies[percentile_to_index(
            len(latencies), 95)]
        summaries[event_pair.name] = event_summary
    return summaries


def get_summaries_from_log(input_file_name, config_xml=None):
    """
    End to end function to compute latencies and summaries from input file.
    Returns a summary dictionary.

    Arguments:
      input_file_name: text file containing the timestamped events, like a
                       log file.
      config_xml: xml file specifying which event pairs to compute latency btwn.
    Returns:
      summaries: dict mapping event pair name to significant summary metrics.
    """
    config_xml = config_xml or os.path.join(os.path.dirname(__file__),
                                            'latency.xml')
    (event_pairs_by_pair_id, event_pairs_by_start_id,
     event_pairs_by_end_id) = parse_xml(config_xml)
    # Compute latencies
    input_file = open(input_file_name, 'r')
    lat_tables_by_pair_id = compute_latencies(input_file,
                                              event_pairs_by_start_id,
                                              event_pairs_by_end_id)
    return get_summaries(event_pairs_by_pair_id, lat_tables_by_pair_id)


if __name__ == '__main__':
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description='Processes profiling data to output latency numbers')
    parser.add_argument(
        '--events-config',
        type=argparse.FileType('r'),
        default=os.path.join(os.path.dirname(__file__), 'latency.xml'),
        help='The configuration XML file for events.'
             ' If not specified uses latency.xml from current folder')
    parser.add_argument(
        'input', type=argparse.FileType('r'), help='The input log')
    args = parser.parse_args()
    process_latencies(args.events_config, args.input)
