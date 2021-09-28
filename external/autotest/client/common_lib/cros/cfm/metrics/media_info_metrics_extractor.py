"""Extracts media info metrics from media info data points."""

import collections
import enum


# Direction and type numbers map to constants in the DataPoint group in
# callstats.proto
class Direction(enum.Enum):
    """
    Possible directions for media entries of a data point.
    """
    SENDER = 0
    RECEIVER = 1
    BANDWIDTH_ESTIMATION = 2


class MediaType(enum.Enum):
    """
    Possible media types for media entries of a data point.
    """
    AUDIO = 1
    VIDEO = 2
    DATA = 3


TimestampedValues = collections.namedtuple('TimestampedValues',
                                           ['TimestampEpochSeconds',
                                            'ValueList'])


class MediaInfoMetricsExtractor(object):
    """
    Extracts media metrics from a list of raw media info data points.

    Media info datapoints are expected to be dictionaries in the format returned
    by cfm_facade.get_media_info_data_points.
    """

    def __init__(self, data_points):
        """
        Initializes with a set of data points.

        @param data_points Data points as a list of dictionaries. Dictionaries
            should be in the format returned by
            cfm_facade.get_media_info_data_points.  I.e., as returned when
            querying the Browser Window for datapoints when the ExportMediaInfo
            mod is active.
        """
        self._data_points = data_points

    def get_top_level_metric(self, name):
        """
        Gets top level metrics.

        Gets metrics that are global for one datapoint. I.e., metrics that
        are not in the media list, such as CPU usage.

        @param name Name of the metric. Names map to the names in the DataPoint
            group in callstats.proto.

        @returns A list with TimestampedValues. The ValueList is guaranteed to
            not contain any None values.
        """
        metrics = []
        for data_point in self._data_points:
            value = data_point.get(name)
            if value is not None:
                timestamp = data_point['timestamp']
                metrics.append(TimestampedValues(timestamp, [value]))
        return metrics

    def get_media_metric(self,
                         name,
                         direction=None,
                         media_type=None,
                         post_process_func=lambda x: x):
        """
        Gets media metrics.

        Gets metrics that are in the media part of the datapoint. A DataPoint
        often contains many media entries, why there are multiple values for a
        specific name.

        @param name Name of the metric. Names map to the names in the DataPoint
            group in callstats.proto.
        @param direction: Only include metrics with this direction in the media
            stream. See the Direction constants in this module. If None, all
            directions are included.
        @param media_type: Only include metrics with this media type in the
            media stream.  See the MediaType constants in this module. If None,
            all media types are included.
        @param post_process_func: Function that takes a list of values and
            optionally transforms it, returning the updated list or a scalar
            value.  Default is to return the unmodified list. This method is
            called for the list of values in the same datapoint. Example usage
            is to sum the received audio bytes for all streams for one logged
            line.

        @returns A list with TimestampedValues. The ValueList is guaranteed to
            not contain any None values.
        """
        metrics = []
        for data_point in self._data_points:
            timestamp = data_point['timestamp']
            values = [
                media[name] for media in data_point['media']
                if name in media
                    and _media_matches(media, direction, media_type)
            ]
            # Filter None values and only add the metric if there is at least
            # one value left.
            values = [x for x in values if x is not None]
            if values:
                values = post_process_func(values)
                values = values if isinstance(values, list) else [values]
                metrics.append(TimestampedValues(timestamp, values))
        return metrics

def _media_matches(media, direction, media_type):
    direction_match = (True if direction is None
                       else media['direction'] == direction.value)
    type_match = (True if media_type is None
                  else media['mediatype'] == media_type.value)
    return direction_match and type_match
