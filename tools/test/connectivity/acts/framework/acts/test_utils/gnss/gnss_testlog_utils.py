#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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
'''Python Module for GNSS test log utilities.'''

import re as regex
import datetime
import functools as fts
import numpy as npy
import pandas as pds
from acts import logger

# GPS API Log Reading Config
CONFIG_GPSAPILOG = {
    'phone_time':
    r'(?P<date>\d+\/\d+\/\d+)\s+(?P<time>\d+:\d+:\d+)\s+'
    r'Read:\s+(?P<logsize>\d+)\s+bytes',
    'SpaceVehicle':
    r'Fix:\s+(?P<Fix>\w+)\s+Type:\s+(?P<Type>\w+)\s+'
    r'SV:\s+(?P<SV>\d+)\s+C\/No:\s+(?P<CNo>\d+\.\d+)\s+'
    r'Elevation:\s+(?P<Elevation>\d+\.\d+)\s+'
    r'Azimuth:\s+(?P<Azimuth>\d+\.\d+)\s+'
    r'Signal:\s+(?P<Signal>\w+)\s+'
    r'Frequency:\s+(?P<Frequency>\d+\.\d+)\s+'
    r'EPH:\s+(?P<EPH>\w+)\s+ALM:\s+(?P<ALM>\w+)',
    'HistoryAvgTop4CNo':
    r'History\s+Avg\s+Top4\s+:\s+(?P<HistoryAvgTop4CNo>\d+\.\d+)',
    'CurrentAvgTop4CNo':
    r'Current\s+Avg\s+Top4\s+:\s+(?P<CurrentAvgTop4CNo>\d+\.\d+)',
    'HistoryAvgCNo':
    r'History\s+Avg\s+:\s+(?P<HistoryAvgCNo>\d+\.\d+)',
    'CurrentAvgCNo':
    r'Current\s+Avg\s+:\s+(?P<CurrentAvgCNo>\d+\.\d+)',
    'L5inFix':
    r'L5\s+used\s+in\s+fix:\s+(?P<L5inFix>\w+)',
    'L5EngagingRate':
    r'L5\s+engaging\s+rate:\s+(?P<L5EngagingRate>\d+.\d+)%',
    'Provider':
    r'Provider:\s+(?P<Provider>\w+)',
    'Latitude':
    r'Latitude:\s+(?P<Latitude>-?\d+.\d+)',
    'Longitude':
    r'Longitude:\s+(?P<Longitude>-?\d+.\d+)',
    'Altitude':
    r'Altitude:\s+(?P<Altitude>-?\d+.\d+)',
    'GNSSTime':
    r'Time:\s+(?P<Date>\d+\/\d+\/\d+)\s+'
    r'(?P<Time>\d+:\d+:\d+)',
    'Speed':
    r'Speed:\s+(?P<Speed>\d+.\d+)',
    'Bearing':
    r'Bearing:\s+(?P<Bearing>\d+.\d+)',
}

# Space Vehicle Statistics Dataframe List
LIST_SVSTAT = [
    'HistoryAvgTop4CNo', 'CurrentAvgTop4CNo', 'HistoryAvgCNo', 'CurrentAvgCNo',
    'L5inFix', 'L5EngagingRate'
]

# Location Fix Info Dataframe List
LIST_LOCINFO = [
    'Provider', 'Latitude', 'Longitude', 'Altitude', 'GNSSTime', 'Speed',
    'Bearing'
]

# GPS TTFF Log Reading Config
CONFIG_GPSTTFFLOG = {
    'ttff_info':
    r'Loop:(?P<loop>\d+)\s+'
    r'(?P<start_datetime>\d+\/\d+\/\d+-\d+:\d+:\d+.\d+)\s+'
    r'(?P<stop_datetime>\d+\/\d+\/\d+-\d+:\d+:\d+.\d+)\s+'
    r'(?P<ttff>\d+.\d+)\s+'
    r'\[Avg Top4 : (?P<avg_top4_cn0>\d+.\d+)\]\s'
    r'\[Avg : (?P<avg_cn0>\d+.\d+)\]\s+\[(?P<fix_type>\d+\w+ fix)\]\s+'
    r'\[Satellites used for fix : (?P<satnum_for_fix>\d+)\]'
}

LOGPARSE_UTIL_LOGGER = logger.create_logger()


def parse_log_to_df(filename, configs, index_rownum=True):
    r"""Parse log to a dictionary of Pandas dataframes.

    Args:
      filename: log file name.
        Type String.
      configs: configs dictionary of parsed Pandas dataframes.
        Type dictionary.
        dict key, the parsed pattern name, such as 'Speed',
        dict value, regex of the config pattern,
          Type Raw String.
      index_rownum: index row number from raw data.
        Type Boolean.
        Default, True.

    Returns:
      parsed_data: dictionary of parsed data.
        Type dictionary.
        dict key, the parsed pattern name, such as 'Speed',
        dict value, the corresponding parsed dataframe.

    Examples:
      configs = {
          'GNSSTime':
          r'Time:\s+(?P<Date>\d+\/\d+\/\d+)\s+
          r(?P<Time>\d+:\d+:\d+)')},
          'Speed': r'Speed:\s+(?P<Speed>\d+.\d+)',
      }
    """
    # Init a local config dictionary to hold compiled regex and match dict.
    configs_local = {}
    # Construct parsed data dictionary
    parsed_data = {}

    # Loop the config dictionary to compile regex and init data list
    for key, regex_string in configs.items():
        configs_local[key] = {
            'cregex': regex.compile(regex_string),
            'datalist': [],
        }

    # Open the file, loop and parse
    with open(filename, 'r') as fid:

        for idx_line, current_line in enumerate(fid):
            for _, config in configs_local.items():
                matched_log_object = config['cregex'].search(current_line)

                if matched_log_object:
                    matched_data = matched_log_object.groupdict()
                    matched_data['rownumber'] = idx_line + 1
                    config['datalist'].append(matched_data)

    # Loop to generate parsed data from configs list
    for key, config in configs_local.items():
        parsed_data[key] = pds.DataFrame(config['datalist'])
        if index_rownum and not parsed_data[key].empty:
            parsed_data[key].set_index('rownumber', inplace=True)
        elif parsed_data[key].empty:
            LOGPARSE_UTIL_LOGGER.warning(
                'The parsed dataframe of "%s" is empty.', key)

    # Return parsed data list
    return parsed_data


def parse_gpstool_ttfflog_to_df(filename):
    """Parse GPSTool ttff log to Pandas dataframes.

    Args:
      filename: full log file name.
        Type, String.

    Returns:
      ttff_df: TTFF Data Frame.
        Type, Pandas DataFrame.
    """
    # Get parsed dataframe list
    parsed_data = lputil.parse_log_to_df(
        filename=filename,
        configs=CONFIG_GPSTTFFLOG,
    )
    ttff_df = parsed_data['ttff_info']

    # Data Conversion
    ttff_df['loop'] = ttff_df['loop'].astype(int)
    ttff_df['start_datetime'] = pds.to_datetime(ttff_df['start_datetime'])
    ttff_df['stop_datetime'] = pds.to_datetime(ttff_df['stop_datetime'])
    ttff_df['ttff'] = ttff_df['ttff'].astype(float)
    ttff_df['avg_top4_cn0'] = ttff_df['avg_top4_cn0'].astype(float)
    ttff_df['avg_cn0'] = ttff_df['avg_cn0'].astype(float)
    ttff_df['satnum_for_fix'] = ttff_df['satnum_for_fix'].astype(int)

    # return ttff dataframe
    return ttff_df


def parse_gpsapilog_to_df(filename):
    """Parse GPS API log to Pandas dataframes.

    Args:
      filename: full log file name.
        Type, String.

    Returns:
      timestamp_df: Timestamp Data Frame.
        Type, Pandas DataFrame.
      sv_info_df: GNSS SV info Data Frame.
        Type, Pandas DataFrame.
      loc_info_df: Location Information Data Frame.
        Type, Pandas DataFrame.
        include Provider, Latitude, Longitude, Altitude, GNSSTime, Speed, Bearing
    """
    def get_phone_time(target_df_row, timestamp_df):
        """subfunction to get the phone_time."""

        try:
            row_num = timestamp_df[
                timestamp_df.index < target_df_row.name].iloc[-1].name
            phone_time = timestamp_df.loc[row_num]['phone_time']
        except IndexError:
            row_num = npy.NaN
            phone_time = npy.NaN

        return phone_time, row_num

    # Get parsed dataframe list
    parsed_data = parse_log_to_df(
        filename=filename,
        configs=CONFIG_GPSAPILOG,
    )

    # get DUT Timestamp
    timestamp_df = parsed_data['phone_time']
    timestamp_df['phone_time'] = timestamp_df.apply(
        lambda row: datetime.datetime.strptime(row.date + '-' + row.time,
                                               '%Y/%m/%d-%H:%M:%S'),
        axis=1)

    # Add phone_time from timestamp_df dataframe by row number
    for key in parsed_data:
        if key != 'phone_time':
            current_df = parsed_data[key]
            time_n_row_num = current_df.apply(get_phone_time,
                                              axis=1,
                                              timestamp_df=timestamp_df)
            current_df[['phone_time', 'time_row_num'
                        ]] = pds.DataFrame(time_n_row_num.apply(pds.Series))

    # Get space vehicle info dataframe
    sv_info_df = parsed_data['SpaceVehicle']

    # Get space vehicle statistics dataframe
    # First merge all dataframe from LIST_SVSTAT[1:],
    # Drop duplicated 'phone_time', based on time_row_num
    sv_stat_df = fts.reduce(
        lambda item1, item2: pds.merge(item1, item2, on='time_row_num'), [
            parsed_data[key].drop(['phone_time'], axis=1)
            for key in LIST_SVSTAT[1:]
        ])
    # Then merge with LIST_SVSTAT[0]
    sv_stat_df = pds.merge(sv_stat_df,
                           parsed_data[LIST_SVSTAT[0]],
                           on='time_row_num')

    # Get location fix information dataframe
    # First merge all dataframe from LIST_LOCINFO[1:],
    # Drop duplicated 'phone_time', based on time_row_num
    loc_info_df = fts.reduce(
        lambda item1, item2: pds.merge(item1, item2, on='time_row_num'), [
            parsed_data[key].drop(['phone_time'], axis=1)
            for key in LIST_LOCINFO[1:]
        ])
    # Then merge with LIST_LOCINFO[8]
    loc_info_df = pds.merge(loc_info_df,
                            parsed_data[LIST_LOCINFO[0]],
                            on='time_row_num')
    # Convert GNSS Time
    loc_info_df['gnsstime'] = loc_info_df.apply(
        lambda row: datetime.datetime.strptime(row.Date + '-' + row.Time,
                                               '%Y/%m/%d-%H:%M:%S'),
        axis=1)

    return timestamp_df, sv_info_df, sv_stat_df, loc_info_df


def parse_gpsapilog_to_df_v2(filename):
    """Parse GPS API log to Pandas dataframes, by using merge_asof.

    Args:
      filename: full log file name.
        Type, String.

    Returns:
      timestamp_df: Timestamp Data Frame.
        Type, Pandas DataFrame.
      sv_info_df: GNSS SV info Data Frame.
        Type, Pandas DataFrame.
      loc_info_df: Location Information Data Frame.
        Type, Pandas DataFrame.
        include Provider, Latitude, Longitude, Altitude, GNSSTime, Speed, Bearing
    """
    # Get parsed dataframe list
    parsed_data = parse_log_to_df(
        filename=filename,
        configs=CONFIG_GPSAPILOG,
    )

    # get DUT Timestamp
    timestamp_df = parsed_data['phone_time']
    timestamp_df['phone_time'] = timestamp_df.apply(
        lambda row: datetime.datetime.strptime(row.date + '-' + row.time,
                                               '%Y/%m/%d-%H:%M:%S'),
        axis=1)
    # drop logsize, date, time
    parsed_data['phone_time'] = timestamp_df.drop(['logsize', 'date', 'time'],
                                                  axis=1)

    # Add phone_time from timestamp dataframe by row number
    for key in parsed_data:
        if key != 'phone_time':
            parsed_data[key] = pds.merge_asof(parsed_data[key],
                                              parsed_data['phone_time'],
                                              left_index=True,
                                              right_index=True)

    # Get space vehicle info dataframe
    sv_info_df = parsed_data['SpaceVehicle']

    # Get space vehicle statistics dataframe
    # First merge all dataframe from LIST_SVSTAT[1:],
    sv_stat_df = fts.reduce(
        lambda item1, item2: pds.merge(item1, item2, on='phone_time'),
        [parsed_data[key] for key in LIST_SVSTAT[1:]])
    # Then merge with LIST_SVSTAT[0]
    sv_stat_df = pds.merge(sv_stat_df,
                           parsed_data[LIST_SVSTAT[0]],
                           on='phone_time')

    # Get location fix information dataframe
    # First merge all dataframe from LIST_LOCINFO[1:],
    loc_info_df = fts.reduce(
        lambda item1, item2: pds.merge(item1, item2, on='phone_time'),
        [parsed_data[key] for key in LIST_LOCINFO[1:]])
    # Then merge with LIST_LOCINFO[8]
    loc_info_df = pds.merge(loc_info_df,
                            parsed_data[LIST_LOCINFO[0]],
                            on='phone_time')
    # Convert GNSS Time
    loc_info_df['gnsstime'] = loc_info_df.apply(
        lambda row: datetime.datetime.strptime(row.Date + '-' + row.Time,
                                               '%Y/%m/%d-%H:%M:%S'),
        axis=1)

    return timestamp_df, sv_info_df, sv_stat_df, loc_info_df
