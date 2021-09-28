"""A simple timer class to keep record of the elapsed time."""

import time


class TimeRecorder(object):
    """Main class to keep time records.

    A timer record contains an ID, a start timestamp, and an optional stop
    timestamps. The elapsed time calculated as stop - start.
    If the stop timestamp is not set, current system time will be used.

    Example usage:
    >>> timer = TimeRecorder()
    >>> # start a single timer, ID = 'lunch'
    >>> timer.start_timer('lunch')
    >>> # start two timers at the same time
    >>> timer.start_timer(['salad', 'dessert'])
    >>> # stop a single timer
    >>> timer.stop_timer('salad')
    >>> # get elapsed time of all timers
    >>> timer.elapsed()
    """

    def __init__(self):
        self.recorder = dict()

    def start_timer(self, record_ids='Default', force=False):
        """Start one or more timer.

        Starts one or more timer at current system time with the record ID
        specified in record_ids. Will overwrite/restart existing timer.

        Args:
            record_ids: timer record IDs. Can be a string or a list of strings.
                        If the record ID is a list, will start multiple timers
                        at the same time.
            force: Force update the timer's start time if the specified timer
                   has already started. By default we won't update started timer
                   again.

        Returns:
            Number of timer started.
        """
        if isinstance(record_ids, str):
            record_ids = [record_ids]
        start_time = time.time()
        for rec in record_ids:
            if force or rec not in self.recorder:
                self.recorder[rec] = [start_time, None]
        return len(record_ids)

    def stop_timer(self, record_ids=None, force=False):
        """Stop one or more timer.

        Stops one or more timer at current system time.

        Args:
            record_ids: timer record IDs. Can be a string or a list of strings.
                        If the record ID is a list, will stop multiple timers at
                        the same time. By default, it will stop all timers.
            force: Force update the timer's stop time if the specified timer has
                   already stopped. By default we won't update stopped timer
                   again.

        Returns:
            Number of timer stopped.
        """
        # stop all record if id is not provided.
        if record_ids is None:
            record_ids = self.recorder.keys()
        elif isinstance(record_ids, str):
            record_ids = [record_ids]
        stop_time = time.time()
        num_rec = 0
        for rec in record_ids:
            if rec in self.recorder:
                if force or self.recorder[rec][1] is None:
                    self.recorder[rec][1] = stop_time
                    num_rec += 1
        return num_rec

    def elapsed(self, record_ids=None):
        """Return elapsed time in seconds.

        For records with no stop time, will calculate based on the current
        system time.

        Args:
            record_ids: timer record IDs. Can be a string or a list of strings.
                        If the record ID is a list, will compute the elapsed
                        time for all specified timers. Default value (None)
                        calculates elapsed time for all existing timers.

        Returns:
            The elapsed time. If the record_ids is a string, will return the
            time in seconds as float type. If the record_ids is a list or
            default (None), will return a dict of the <record id, elapsed time>.
        """
        single_record = False
        if record_ids is None:
            record_ids = self.recorder.keys()
        elif isinstance(record_ids, str):
            record_ids = [record_ids]
            single_record = True
        results = dict()
        curr_time = time.time()
        for rec in record_ids:
            if rec in self.recorder:
                if self.recorder[rec][1] is not None:
                    results[rec] = self.recorder[rec][1] - self.recorder[rec][0]
                else:
                    results[rec] = curr_time - self.recorder[rec][0]
        if not results:  # no valid record found
            return None
        elif single_record and len(record_ids) == 1:
            # only 1 record is requested, return results directly
            return results[record_ids[0]]
        else:
            return results  # multiple records, return a dict.

    def clear(self, record_ids=None):
        """Clear existing time records."""
        if record_ids is None:
            self.recorder = dict()
            return

        if isinstance(record_ids, str):
            record_ids = [record_ids]
        for rec in record_ids:
            if rec in self.recorder:
                del self.recorder[rec]
