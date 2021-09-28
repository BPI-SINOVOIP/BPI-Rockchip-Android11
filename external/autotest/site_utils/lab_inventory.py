#!/usr/bin/env python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create e-mail reports of the Lab's DUT inventory.

Gathers a list of all DUTs of interest in the Lab, segregated by
model and pool, and determines whether each DUT is working or
broken.  Then, send one or more e-mail reports summarizing the
status to e-mail addresses provided on the command line.

usage:  lab_inventory.py [ options ] [ model ... ]

Options:
--duration / -d <hours>
    How far back in time to search job history to determine DUT
    status.

--model-notify <address>[,<address>]
    Send the "model status" e-mail to all the specified e-mail
    addresses.

--pool-notify <address>[,<address>]
    Send the "pool status" e-mail to all the specified e-mail
    addresses.

--recommend <number>
    When generating the "model status" e-mail, include a list of
    <number> specific DUTs to be recommended for repair.

--report-untestable
    Scan the inventory for DUTs that can't test because they're stuck in
    repair loops, or because the scheduler can't give them work.

--logdir <directory>
    Log progress and actions in a file under this directory.  Text
    of any e-mail sent will also be logged in a timestamped file in
    this directory.

--debug
    Suppress all logging, metrics reporting, and sending e-mail.
    Instead, write the output that would be generated onto stdout.

<model> arguments:
    With no arguments, gathers the status for all models in the lab.
    With one or more named models on the command line, restricts
    reporting to just those models.
"""


import argparse
import collections
import logging
import logging.handlers
import os
import re
import sys
import time

import common
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import time_utils
from autotest_lib.frontend.afe.json_rpc import proxy
from autotest_lib.server import constants
from autotest_lib.server import site_utils
from autotest_lib.server.cros.dynamic_suite import frontend_wrappers
from autotest_lib.server.hosts import servo_host
from autotest_lib.server.lib import status_history
from autotest_lib.site_utils import gmail_lib
from chromite.lib import metrics


CRITICAL_POOLS = constants.Pools.CRITICAL_POOLS
SPARE_POOL = constants.Pools.SPARE_POOL
MANAGED_POOLS = constants.Pools.MANAGED_POOLS

# _EXCLUDED_LABELS - A set of labels that disqualify a DUT from
#     monitoring by this script.  Currently, we're excluding these:
#   + 'adb' - We're not ready to monitor Android or Brillo hosts.
#   + 'board:guado_moblab' - These are maintained by a separate
#     process that doesn't use this script.
#   + 'board:veyron_rialto' due to crbug.com/854404

_EXCLUDED_LABELS = {'adb', 'board:guado_moblab',
                    'board:veyron_rialto'}

# _DEFAULT_DURATION:
#     Default value used for the --duration command line option.
#     Specifies how far back in time to search in order to determine
#     DUT status.

_DEFAULT_DURATION = 24

# _LOGDIR:
#     Relative path used in the calculation of the default setting for
#     the --logdir option.  The full path is relative to the root of the
#     autotest directory, as determined from sys.argv[0].
# _LOGFILE:
#     Basename of a file to which general log information will be
#     written.
# _LOG_FORMAT:
#     Format string for log messages.

_LOGDIR = os.path.join('logs', 'dut-data')
_LOGFILE = 'lab-inventory.log'
_LOG_FORMAT = '%(asctime)s | %(levelname)-10s | %(message)s'

# Pattern describing location-based host names in the Chrome OS test
# labs.  Each DUT hostname designates the DUT's location:
#   * A lab (room) that's physically separated from other labs
#     (i.e. there's a door).
#   * A row (or aisle) of DUTs within the lab.
#   * A vertical rack of shelves on the row.
#   * A specific host on one shelf of the rack.

_HOSTNAME_PATTERN = re.compile(
        r'(chromeos\d+)-row(\d+)-rack(\d+)-host(\d+)')

# _REPAIR_LOOP_THRESHOLD:
#    The number of repeated Repair tasks that must be seen to declare
#    that a DUT is stuck in a repair loop.

_REPAIR_LOOP_THRESHOLD = 4


_METRICS_PREFIX = 'chromeos/autotest/inventory'
_UNTESTABLE_PRESENCE_METRIC = metrics.BooleanMetric(
    _METRICS_PREFIX + '/untestable',
    'DUTs that cannot be scheduled for testing')

_MISSING_DUT_METRIC = metrics.Counter(
    _METRICS_PREFIX + '/missing', 'DUTs which cannot be found by lookup queries'
    ' because they are invalid or deleted')

# _Diagnosis - namedtuple corresponding to the return value from
# `HostHistory.last_diagnosis()`
_Diagnosis = collections.namedtuple('_Diagnosis', ['status', 'task'])

def _get_diagnosis(history):
    dut_present = True
    try:
        diagnosis = _Diagnosis(*history.last_diagnosis())
        if (diagnosis.status == status_history.BROKEN
                and diagnosis.task.end_time < history.start_time):
            return _Diagnosis(status_history.UNUSED, diagnosis.task)
        else:
            return diagnosis
    except proxy.JSONRPCException as e:
        logging.warn(e)
        dut_present = False
    finally:
        _MISSING_DUT_METRIC.increment(
            fields={'host': history.hostname, 'presence': dut_present})
    return _Diagnosis(None, None)


def _host_is_working(history):
    return _get_diagnosis(history).status == status_history.WORKING


def _host_is_broken(history):
    return _get_diagnosis(history).status == status_history.BROKEN


def _host_is_idle(history):
    idle_statuses = {status_history.UNUSED, status_history.UNKNOWN}
    return _get_diagnosis(history).status in idle_statuses


class _HostSetInventory(object):
    """Maintains a set of related `HostJobHistory` objects.

    Current usage of this class is that all DUTs are part of a single
    scheduling pool of DUTs for a single model; however, this class make
    no assumptions about the actual relationship among the DUTs.

    The collection is segregated into disjoint categories of "working",
    "broken", and "idle" DUTs.  Accessor methods allow finding both the
    list of DUTs in each category, as well as counts of each category.

    Performance note:  Certain methods in this class are potentially
    expensive:
      * `get_working()`
      * `get_working_list()`
      * `get_broken()`
      * `get_broken_list()`
      * `get_idle()`
      * `get_idle_list()`
    The first time any one of these methods is called, it causes
    multiple RPC calls with a relatively expensive set of database
    queries.  However, the results of the queries are cached in the
    individual `HostJobHistory` objects, so only the first call
    actually pays the full cost.

    Additionally, `get_working_list()`, `get_broken_list()` and
    `get_idle_list()` cache their return values to avoid recalculating
    lists at every call; this caching is separate from the caching of
    RPC results described above.

    This class is deliberately constructed to delay the RPC cost until
    the accessor methods are called (rather than to query in
    `record_host()`) so that it's possible to construct a complete
    `_LabInventory` without making the expensive queries at creation
    time.  `_populate_model_counts()`, below, assumes this behavior.
    """

    def __init__(self):
        self._histories = []
        self._working_list = None
        self._broken_list = None
        self._idle_list = None

    def record_host(self, host_history):
        """Add one `HostJobHistory` object to the collection.

        @param host_history The `HostJobHistory` object to be
                            remembered.
        """
        self._working_list = None
        self._broken_list = None
        self._idle_list = None
        self._histories.append(host_history)

    def get_working_list(self):
        """Return a list of all working DUTs in the pool.

        Filter `self._histories` for histories where the DUT is
        diagnosed as working.

        Cache the result so that we only cacluate it once.

        @return A list of HostJobHistory objects.
        """
        if self._working_list is None:
            self._working_list = [h for h in self._histories
                                  if _host_is_working(h)]
        return self._working_list

    def get_working(self):
        """Return the number of working DUTs in the pool."""
        return len(self.get_working_list())

    def get_broken_list(self):
        """Return a list of all broken DUTs in the pool.

        Filter `self._histories` for histories where the DUT is
        diagnosed as broken.

        Cache the result so that we only cacluate it once.

        @return A list of HostJobHistory objects.
        """
        if self._broken_list is None:
            self._broken_list = [h for h in self._histories
                                 if _host_is_broken(h)]
        return self._broken_list

    def get_broken(self):
        """Return the number of broken DUTs in the pool."""
        return len(self.get_broken_list())

    def get_idle_list(self):
        """Return a list of all idle DUTs in the pool.

        Filter `self._histories` for histories where the DUT is
        diagnosed as idle.

        Cache the result so that we only cacluate it once.

        @return A list of HostJobHistory objects.
        """
        if self._idle_list is None:
            self._idle_list = [h for h in self._histories
                               if _host_is_idle(h)]
        return self._idle_list

    def get_idle(self):
        """Return the number of idle DUTs in the pool."""
        return len(self.get_idle_list())

    def get_total(self):
        """Return the total number of DUTs in the pool."""
        return len(self._histories)

    def get_all_histories(self):
        return self._histories


class _PoolSetInventory(object):
    """Maintains a set of `HostJobHistory`s for a set of pools.

    The collection is segregated into disjoint categories of "working",
    "broken", and "idle" DUTs.  Accessor methods allow finding both the
    list of DUTs in each category, as well as counts of each category.
    Accessor queries can be for an individual pool, or against all
    pools.

    Performance note:  This class relies on `_HostSetInventory`.  Public
    methods in this class generally rely on methods of the same name in
    the underlying class, and so will have the same underlying
    performance characteristics.
    """

    def __init__(self, pools):
        self._histories_by_pool = {
            pool: _HostSetInventory() for pool in pools
        }

    def record_host(self, host_history):
        """Add one `HostJobHistory` object to the collection.

        @param host_history The `HostJobHistory` object to be
                            remembered.
        """
        pool = host_history.host_pool
        self._histories_by_pool[pool].record_host(host_history)

    def _count_pool(self, get_pool_count, pool=None):
        """Internal helper to count hosts in a given pool.

        The `get_pool_count` parameter is a function to calculate
        the exact count of interest for the pool.

        @param get_pool_count  Function to return a count from a
                               _PoolCount object.
        @param pool            The pool to be counted.  If `None`,
                               return the total across all pools.
        """
        if pool is None:
            return sum([get_pool_count(cached_history) for cached_history in
                        self._histories_by_pool.values()])
        else:
            return get_pool_count(self._histories_by_pool[pool])

    def get_working_list(self):
        """Return a list of all working DUTs (across all pools).

        Go through all HostJobHistory objects across all pools,
        selecting all DUTs identified as working.

        @return A list of HostJobHistory objects.
        """
        l = []
        for p in self._histories_by_pool.values():
            l.extend(p.get_working_list())
        return l

    def get_working(self, pool=None):
        """Return the number of working DUTs in a pool.

        @param pool  The pool to be counted.  If `None`, return the
                     total across all pools.

        @return The total number of working DUTs in the selected
                pool(s).
        """
        return self._count_pool(_HostSetInventory.get_working, pool)

    def get_broken_list(self):
        """Return a list of all broken DUTs (across all pools).

        Go through all HostJobHistory objects across all pools,
        selecting all DUTs identified as broken.

        @return A list of HostJobHistory objects.
        """
        l = []
        for p in self._histories_by_pool.values():
            l.extend(p.get_broken_list())
        return l

    def get_broken(self, pool=None):
        """Return the number of broken DUTs in a pool.

        @param pool  The pool to be counted.  If `None`, return the
                     total across all pools.

        @return The total number of broken DUTs in the selected pool(s).
        """
        return self._count_pool(_HostSetInventory.get_broken, pool)

    def get_idle_list(self, pool=None):
        """Return a list of all idle DUTs in the given pool.

        Go through all HostJobHistory objects across all pools,
        selecting all DUTs identified as idle.

        @param pool: The pool to be counted. If `None`, return the total list
                     across all pools.

        @return A list of HostJobHistory objects.
        """
        if pool is None:
            l = []
            for p in self._histories_by_pool.itervalues():
                l.extend(p.get_idle_list())
            return l
        else:
            return self._histories_by_pool[pool].get_idle_list()

    def get_idle(self, pool=None):
        """Return the number of idle DUTs in a pool.

        @param pool: The pool to be counted. If `None`, return the total
                     across all pools.

        @return The total number of idle DUTs in the selected pool(s).
        """
        return self._count_pool(_HostSetInventory.get_idle, pool)

    def get_spares_buffer(self, spare_pool=SPARE_POOL):
        """Return the the nominal number of working spares.

        Calculates and returns how many working spares there would
        be in the spares pool if all broken DUTs were in the spares
        pool.  This number may be negative, indicating a shortfall
        in the critical pools.

        @return The total number DUTs in the spares pool, less the total
                number of broken DUTs in all pools.
        """
        return self.get_total(spare_pool) - self.get_broken()

    def get_total(self, pool=None):
        """Return the total number of DUTs in a pool.

        @param pool  The pool to be counted.  If `None`, return the
                     total across all pools.

        @return The total number of DUTs in the selected pool(s).
        """
        return self._count_pool(_HostSetInventory.get_total, pool)

    def get_all_histories(self, pool=None):
        if pool is None:
            for p in self._histories_by_pool.itervalues():
                for h in p.get_all_histories():
                    yield h
        else:
            for h in self._histories_by_pool[pool].get_all_histories():
                yield h


def _is_migrated_to_skylab(afehost):
    """Return True if the provided frontend.Host has been migrated to skylab."""
    return afehost.hostname.endswith('-migrated-do-not-use')


def _eligible_host(afehost):
    """Return whether this host is eligible for monitoring.

    @param afehost  The host to be tested for eligibility.
    """
    if _is_migrated_to_skylab(afehost):
        return False

    # DUTs without an existing, unique 'model' or 'pool' label aren't meant to
    # exist in the managed inventory; their presence generally indicates an
    # error in the database. The _LabInventory constructor requires hosts to
    # conform to the label restrictions. Failing an inventory run for a single
    # bad entry is wrong, so we ignore these hosts.
    models = [l for l in afehost.labels
                 if l.startswith(constants.Labels.MODEL_PREFIX)]
    pools = [l for l in afehost.labels
                 if l.startswith(constants.Labels.POOL_PREFIX)]
    excluded = _EXCLUDED_LABELS.intersection(afehost.labels)
    return len(models) == 1 and len(pools) == 1 and not excluded


class _LabInventory(collections.Mapping):
    """Collection of `HostJobHistory` objects for the Lab's inventory.

    This is a dict-like collection indexed by model.  Indexing returns
    the _PoolSetInventory object associated with the model.
    """

    @classmethod
    def create_inventory(cls, afe, start_time, end_time, modellist=[]):
        """Return a Lab inventory with specified parameters.

        By default, gathers inventory from `HostJobHistory` objects for
        all DUTs in the `MANAGED_POOLS` list.  If `modellist` is
        supplied, the inventory will be restricted to only the given
        models.

        @param afe          AFE object for constructing the
                            `HostJobHistory` objects.
        @param start_time   Start time for the `HostJobHistory` objects.
        @param end_time     End time for the `HostJobHistory` objects.
        @param modellist    List of models to include.  If empty,
                            include all available models.
        @return A `_LabInventory` object for the specified models.
        """
        target_pools = MANAGED_POOLS
        label_list = [constants.Labels.POOL_PREFIX + l for l in target_pools]
        afehosts = afe.get_hosts(labels__name__in=label_list)
        if modellist:
            # We're deliberately not checking host eligibility in this
            # code path.  This is a debug path, not used in production;
            # it may be useful to include ineligible hosts here.
            modelhosts = []
            for model in modellist:
                model_label = constants.Labels.MODEL_PREFIX + model
                host_list = [h for h in afehosts
                                  if model_label in h.labels]
                modelhosts.extend(host_list)
            afehosts = modelhosts
        else:
            afehosts = [h for h in afehosts if _eligible_host(h)]
        create = lambda host: (
                status_history.HostJobHistory(afe, host,
                                              start_time, end_time))
        return cls([create(host) for host in afehosts], target_pools)

    def __init__(self, histories, pools):
        models = {h.host_model for h in histories}
        self._modeldata = {model: _PoolSetInventory(pools) for model in models}
        self._dut_count = len(histories)
        for h in histories:
            self[h.host_model].record_host(h)
        self._boards = {h.host_board for h in histories}

    def __getitem__(self, key):
        return self._modeldata.__getitem__(key)

    def __len__(self):
        return self._modeldata.__len__()

    def __iter__(self):
        return self._modeldata.__iter__()

    def get_num_duts(self):
        """Return the total number of DUTs in the inventory."""
        return self._dut_count

    def get_num_models(self):
        """Return the total number of models in the inventory."""
        return len(self)

    def get_pool_models(self, pool):
        """Return all models in `pool`.

        @param pool The pool to be inventoried for models.
        """
        return {m for m, h in self.iteritems() if h.get_total(pool)}

    def get_boards(self):
        return self._boards


def _reportable_models(inventory, spare_pool=SPARE_POOL):
    """Iterate over all models subject to reporting.

    Yields the contents of `inventory.iteritems()` filtered to include
    only reportable models.  A model is reportable if it has DUTs in
    both `spare_pool` and at least one other pool.

    @param spare_pool  The spare pool to be tested for reporting.
    """
    for model, poolset in inventory.iteritems():
        spares = poolset.get_total(spare_pool)
        total = poolset.get_total()
        if spares != 0 and spares != total:
            yield model, poolset


def _all_dut_histories(inventory):
    for poolset in inventory.itervalues():
        for h in poolset.get_all_histories():
            yield h


def _sort_by_location(inventory_list):
    """Return a list of DUTs, organized by location.

    Take the given list of `HostJobHistory` objects, separate it
    into a list per lab, and sort each lab's list by location.  The
    order of sorting within a lab is
      * By row number within the lab,
      * then by rack number within the row,
      * then by host shelf number within the rack.

    Return a list of the sorted lists.

    Implementation note: host locations are sorted by converting
    each location into a base 100 number.  If row, rack or
    host numbers exceed the range [0..99], then sorting will
    break down.

    @return A list of sorted lists of DUTs.
    """
    BASE = 100
    lab_lists = {}
    for history in inventory_list:
        location = _HOSTNAME_PATTERN.match(history.host.hostname)
        if location:
            lab = location.group(1)
            key = 0
            for idx in location.group(2, 3, 4):
                key = BASE * key + int(idx)
            lab_lists.setdefault(lab, []).append((key, history))
    return_list = []
    for dut_list in lab_lists.values():
        dut_list.sort(key=lambda t: t[0])
        return_list.append([t[1] for t in dut_list])
    return return_list


def _score_repair_set(buffer_counts, repair_list):
    """Return a numeric score rating a set of DUTs to be repaired.

    `buffer_counts` is a dictionary mapping model names to the size of
    the model's spares buffer.

    `repair_list` is a list of `HostJobHistory` objects for the DUTs to
    be repaired.

    This function calculates the new set of buffer counts that would
    result from the proposed repairs, and scores the new set using two
    numbers:
      * Worst case buffer count for any model (higher is better).  This
        is the more significant number for comparison.
      * Number of models at the worst case (lower is better).  This is
        the less significant number.

    Implementation note:  The score could fail to reflect the intended
    criteria if there are more than 1000 models in the inventory.

    @param spare_counts   A dictionary mapping models to buffer counts.
    @param repair_list    A list of `HostJobHistory` objects for the
                          DUTs to be repaired.
    @return A numeric score.
    """
    # Go through `buffer_counts`, and create a list of new counts
    # that records the buffer count for each model after repair.
    # The new list of counts discards the model names, as they don't
    # contribute to the final score.
    _NMODELS = 1000
    pools = {h.host_pool for h in repair_list}
    repair_inventory = _LabInventory(repair_list, pools)
    new_counts = []
    for m, c in buffer_counts.iteritems():
        if m in repair_inventory:
            newcount = repair_inventory[m].get_total()
        else:
            newcount = 0
        new_counts.append(c + newcount)
    # Go through the new list of counts.  Find the worst available
    # spares count, and count how many times that worst case occurs.
    worst_count = new_counts[0]
    num_worst = 1
    for c in new_counts[1:]:
        if c == worst_count:
            num_worst += 1
        elif c < worst_count:
            worst_count = c
            num_worst = 1
    # Return the calculated score
    return _NMODELS * worst_count - num_worst


def _generate_repair_recommendation(inventory, num_recommend):
    """Return a summary of selected DUTs needing repair.

    Returns a message recommending a list of broken DUTs to be repaired.
    The list of DUTs is selected based on these criteria:
      * No more than `num_recommend` DUTs will be listed.
      * All DUTs must be in the same lab.
      * DUTs should be selected for some degree of physical proximity.
      * DUTs for models with a low spares buffer are more important than
        DUTs with larger buffers.

    The algorithm used will guarantee that at least one DUT from a model
    with the lowest spares buffer will be recommended.  If the worst
    spares buffer number is shared by more than one model, the algorithm
    will tend to prefer repair sets that include more of those models
    over sets that cover fewer models.

    @param inventory      `_LabInventory` object from which to generate
                          recommendations.
    @param num_recommend  Number of DUTs to recommend for repair.
    """
    logging.debug('Creating DUT repair recommendations')
    model_buffer_counts = {}
    broken_list = []
    for model, counts in _reportable_models(inventory):
        logging.debug('Listing failed DUTs for %s', model)
        if counts.get_broken() != 0:
            model_buffer_counts[model] = counts.get_spares_buffer()
            broken_list.extend(counts.get_broken_list())
    # N.B. The logic inside this loop may seem complicated, but
    # simplification is hard:
    #   * Calculating an initial recommendation outside of
    #     the loop likely would make things more complicated,
    #     not less.
    #   * It's necessary to calculate an initial lab slice once per
    #     lab _before_ the while loop, in case the number of broken
    #     DUTs in a lab is less than `num_recommend`.
    recommendation = None
    best_score = None
    for lab_duts in _sort_by_location(broken_list):
        start = 0
        end = num_recommend
        lab_slice = lab_duts[start : end]
        lab_score = _score_repair_set(model_buffer_counts, lab_slice)
        while end < len(lab_duts):
            start += 1
            end += 1
            new_slice = lab_duts[start : end]
            new_score = _score_repair_set(model_buffer_counts, new_slice)
            if new_score > lab_score:
                lab_slice = new_slice
                lab_score = new_score
        if recommendation is None or lab_score > best_score:
            recommendation = lab_slice
            best_score = lab_score
    # N.B. The trailing space in `line_fmt` is manadatory:  Without it,
    # Gmail will parse the URL wrong.  Don't ask.  If you simply _must_
    # know more, go try it yourself...
    line_fmt = '%-30s %-16s %-6s\n    %s '
    message = ['Repair recommendations:\n',
               line_fmt % ( 'Hostname', 'Model', 'Servo?', 'Logs URL')]
    if recommendation:
        for h in recommendation:
            servo_name = servo_host.make_servo_hostname(h.host.hostname)
            servo_present = utils.host_is_in_lab_zone(servo_name)
            event = _get_diagnosis(h).task
            line = line_fmt % (
                    h.host.hostname, h.host_model,
                    'Yes' if servo_present else 'No', event.job_url)
            message.append(line)
    else:
        message.append('(No DUTs to repair)')
    return '\n'.join(message)


def _generate_model_inventory_message(inventory):
    """Generate the "model inventory" e-mail message.

    The model inventory is a list by model summarizing the number of
    working, broken, and idle DUTs, and the total shortfall or surplus
    of working devices relative to the minimum critical pool
    requirement.

    The report omits models with no DUTs in the spare pool or with no
    DUTs in a critical pool.

    N.B. For sample output text formattted as users can expect to
    see it in e-mail and log files, refer to the unit tests.

    @param inventory  `_LabInventory` object to be reported on.
    @return String with the inventory message to be sent.
    """
    logging.debug('Creating model inventory')
    nworking = 0
    nbroken = 0
    nidle = 0
    nbroken_models = 0
    ntotal_models = 0
    summaries = []
    column_names = (
        'Model', 'Avail', 'Bad', 'Idle', 'Good', 'Spare', 'Total')
    for model, counts in _reportable_models(inventory):
        logging.debug('Counting %2d DUTS for model %s',
                      counts.get_total(), model)
        # Summary elements laid out in the same order as the column
        # headers:
        #     Model Avail   Bad  Idle  Good  Spare Total
        #      e[0]  e[1]  e[2]  e[3]  e[4]  e[5]  e[6]
        element = (model,
                   counts.get_spares_buffer(),
                   counts.get_broken(),
                   counts.get_idle(),
                   counts.get_working(),
                   counts.get_total(SPARE_POOL),
                   counts.get_total())
        if element[2]:
            summaries.append(element)
            nbroken_models += 1
        ntotal_models += 1
        nbroken += element[2]
        nidle += element[3]
        nworking += element[4]
    ntotal = nworking + nbroken + nidle
    summaries = sorted(summaries, key=lambda e: (e[1], -e[2]))
    broken_percent = int(round(100.0 * nbroken / ntotal))
    idle_percent = int(round(100.0 * nidle / ntotal))
    working_percent = 100 - broken_percent - idle_percent
    message = ['Summary of DUTs in inventory:',
               '%10s %10s %10s %6s' % ('Bad', 'Idle', 'Good', 'Total'),
               '%5d %3d%% %5d %3d%% %5d %3d%% %6d' % (
                   nbroken, broken_percent,
                   nidle, idle_percent,
                   nworking, working_percent,
                   ntotal),
               '',
               'Models with failures: %d' % nbroken_models,
               'Models in inventory:  %d' % ntotal_models,
               '', '',
               'Full model inventory:\n',
               '%-22s %5s %5s %5s %5s %5s %5s' % column_names]
    message.extend(
            ['%-22s %5d %5d %5d %5d %5d %5d' % e for e in summaries])
    return '\n'.join(message)


_POOL_INVENTORY_HEADER = '''\
Notice to Infrastructure deputies:  All models shown below are at
less than full strength, please take action to resolve the issues.
Once you're satisified that failures won't recur, failed DUTs can
be replaced with spares by running `balance_pool`.  Detailed
instructions can be found here:
    http://go/cros-manage-duts
'''


def _generate_pool_inventory_message(inventory):
    """Generate the "pool inventory" e-mail message.

    The pool inventory is a list by pool and model summarizing the
    number of working and broken DUTs in the pool.  Only models with
    at least one broken DUT are included in the list.

    N.B. For sample output text formattted as users can expect to see it
    in e-mail and log files, refer to the unit tests.

    @param inventory  `_LabInventory` object to be reported on.
    @return String with the inventory message to be sent.
    """
    logging.debug('Creating pool inventory')
    message = [_POOL_INVENTORY_HEADER]
    newline = ''
    for pool in CRITICAL_POOLS:
        message.append(
            '%sStatus for pool:%s, by model:' % (newline, pool))
        message.append(
            '%-20s   %5s %5s %5s %5s' % (
                'Model', 'Bad', 'Idle', 'Good', 'Total'))
        data_list = []
        for model, counts in inventory.iteritems():
            logging.debug('Counting %2d DUTs for %s, %s',
                          counts.get_total(pool), model, pool)
            broken = counts.get_broken(pool)
            idle = counts.get_idle(pool)
            # models at full strength are not reported
            if not broken and not idle:
                continue
            working = counts.get_working(pool)
            total = counts.get_total(pool)
            data_list.append((model, broken, idle, working, total))
        if data_list:
            data_list = sorted(data_list, key=lambda d: -d[1])
            message.extend(
                ['%-20s   %5d %5d %5d %5d' % t for t in data_list])
        else:
            message.append('(All models at full strength)')
        newline = '\n'
    return '\n'.join(message)


_IDLE_INVENTORY_HEADER = '''\
Notice to Infrastructure deputies:  The hosts shown below haven't
run any jobs for at least 24 hours. Please check each host; locked
hosts should normally be unlocked; stuck jobs should normally be
aborted.
'''


def _generate_idle_inventory_message(inventory):
    """Generate the "idle inventory" e-mail message.

    The idle inventory is a host list with corresponding pool and model,
    where the hosts are identified as idle.

    N.B. For sample output text format as users can expect to
    see it in e-mail and log files, refer to the unit tests.

    @param inventory  `_LabInventory` object to be reported on.
    @return String with the inventory message to be sent.
    """
    logging.debug('Creating idle inventory')
    message = [_IDLE_INVENTORY_HEADER]
    message.append('Idle Host List:')
    message.append('%-30s %-20s %s' % ('Hostname', 'Model', 'Pool'))
    data_list = []
    for pool in MANAGED_POOLS:
        for model, counts in inventory.iteritems():
            logging.debug('Counting %2d DUTs for %s, %s',
                          counts.get_total(pool), model, pool)
            data_list.extend([(dut.host.hostname, model, pool)
                                  for dut in counts.get_idle_list(pool)])
    if data_list:
        message.extend(['%-30s %-20s %s' % t for t in data_list])
    else:
        message.append('(No idle DUTs)')
    return '\n'.join(message)


def _send_email(arguments, tag, subject, recipients, body):
    """Send an inventory e-mail message.

    The message is logged in the selected log directory using `tag` for
    the file name.

    If the --debug option was requested, the message is neither logged
    nor sent, but merely printed on stdout.

    @param arguments   Parsed command-line options.
    @param tag         Tag identifying the inventory for logging
                       purposes.
    @param subject     E-mail Subject: header line.
    @param recipients  E-mail addresses for the To: header line.
    @param body        E-mail message body.
    """
    logging.debug('Generating email: "%s"', subject)
    all_recipients = ', '.join(recipients)
    report_body = '\n'.join([
            'To: %s' % all_recipients,
            'Subject: %s' % subject,
            '', body, ''])
    if arguments.debug:
        print report_body
    else:
        filename = os.path.join(arguments.logdir, tag)
        try:
            report_file = open(filename, 'w')
            report_file.write(report_body)
            report_file.close()
        except EnvironmentError as e:
            logging.error('Failed to write %s:  %s', filename, e)
        try:
            gmail_lib.send_email(all_recipients, subject, body)
        except Exception as e:
            logging.error('Failed to send e-mail to %s:  %s',
                          all_recipients, e)


def _populate_model_counts(inventory):
    """Gather model counts while providing interactive feedback.

    Gathering the status of all individual DUTs in the lab can take
    considerable time (~30 minutes at the time of this writing).
    Normally, we pay that cost by querying as we go.  However, with
    the `--debug` option, we expect a human being to be watching the
    progress in real time.  So, we force the first (expensive) queries
    to happen up front, and provide simple ASCII output on sys.stdout
    to show a progress bar and results.

    @param inventory  `_LabInventory` object from which to gather
                      counts.
    """
    n = 0
    total_broken = 0
    for counts in inventory.itervalues():
        n += 1
        if n % 10 == 5:
            c = '+'
        elif n % 10 == 0:
            c = '%d' % ((n / 10) % 10)
        else:
            c = '.'
        sys.stdout.write(c)
        sys.stdout.flush()
        # This next call is where all the time goes - it forces all of a
        # model's `HostJobHistory` objects to query the database and
        # cache their results.
        total_broken += counts.get_broken()
    sys.stdout.write('\n')
    sys.stdout.write('Found %d broken DUTs\n' % total_broken)


def _perform_model_inventory(arguments, inventory, timestamp):
    """Perform the model inventory report.

    The model inventory report consists of the following:
      * A list of DUTs that are recommended to be repaired.  This list
        is optional, and only appears if the `--recommend` option is
        present.
      * A list of all models that have failed DUTs, with counts
        of working, broken, and spare DUTs, among others.

    @param arguments  Command-line arguments as returned by
                      `ArgumentParser`
    @param inventory  `_LabInventory` object to be reported on.
    @param timestamp  A string used to identify this run's timestamp
                      in logs and email output.
    """
    if arguments.recommend:
        recommend_message = _generate_repair_recommendation(
                inventory, arguments.recommend) + '\n\n\n'
    else:
        recommend_message = ''
    model_message = _generate_model_inventory_message(inventory)
    _send_email(arguments,
                'models-%s.txt' % timestamp,
                'DUT model inventory %s' % timestamp,
                arguments.model_notify,
                recommend_message + model_message)


def _perform_pool_inventory(arguments, inventory, timestamp):
    """Perform the pool inventory report.

    The pool inventory report consists of the following:
      * A list of all critical pools that have failed DUTs, with counts
        of working, broken, and idle DUTs.
      * A list of all idle DUTs by hostname including the model and
        pool.

    @param arguments  Command-line arguments as returned by
                      `ArgumentParser`
    @param inventory  `_LabInventory` object to be reported on.
    @param timestamp  A string used to identify this run's timestamp in
                      logs and email output.
    """
    pool_message = _generate_pool_inventory_message(inventory)
    idle_message = _generate_idle_inventory_message(inventory)
    _send_email(arguments,
                'pools-%s.txt' % timestamp,
                'DUT pool inventory %s' % timestamp,
                arguments.pool_notify,
                pool_message + '\n\n\n' + idle_message)


def _dut_in_repair_loop(history):
    """Return whether a DUT's history indicates a repair loop.

    A DUT is considered looping if it runs no tests, and no tasks pass
    other than repair tasks.

    @param history  An instance of `status_history.HostJobHistory` to be
                    scanned for a repair loop.  The caller guarantees
                    that this history corresponds to a working DUT.
    @returns  Return a true value if the DUT's most recent history
              indicates a repair loop.
    """
    # Our caller passes only histories for working DUTs; that means
    # we've already paid the cost of fetching the diagnosis task, and
    # we know that the task was successful.  The diagnosis task will be
    # one of the tasks we must scan to find a loop, so if the task isn't
    # a repair task, then our history includes a successful non-repair
    # task, and we're not looping.
    #
    # The for loop below is very expensive, because it must fetch the
    # full history, regardless of how many tasks we examine.  At the
    # time of this writing, this check against the diagnosis task
    # reduces the cost of finding loops in the full inventory from hours
    # to minutes.
    if _get_diagnosis(history).task.name != 'Repair':
        return False
    repair_ok_count = 0
    for task in history:
        if not task.is_special:
            # This is a test, so we're not looping.
            return False
        if task.diagnosis == status_history.BROKEN:
            # Failed a repair, so we're not looping.
            return False
        if (task.diagnosis == status_history.WORKING
                and task.name != 'Repair'):
            # Non-repair task succeeded, so we're not looping.
            return False
        # At this point, we have either a failed non-repair task, or
        # a successful repair.
        if task.name == 'Repair':
            repair_ok_count += 1
            if repair_ok_count >= _REPAIR_LOOP_THRESHOLD:
                return True


def _report_untestable_dut(history, state):
    fields = {
        'dut_hostname': history.hostname,
        'model': history.host_model,
        'pool': history.host_pool,
        'state': state,
    }
    logging.info('DUT in state %(state)s: %(dut_hostname)s, '
                 'model: %(model)s, pool: %(pool)s', fields)
    _UNTESTABLE_PRESENCE_METRIC.set(True, fields=fields)


def _report_untestable_dut_metrics(inventory):
    """Scan the inventory for DUTs unable to run tests.

    DUTs in the inventory are judged "untestable" if they meet one of
    two criteria:
      * The DUT is stuck in a repair loop; that is, it regularly passes
        repair, but never passes other operations.
      * The DUT runs no tasks at all, but is not locked.

    This routine walks through the given inventory looking for DUTs in
    either of these states.  Results are reported via a Monarch presence
    metric.

    Note:  To make sure that DUTs aren't flagged as "idle" merely
    because there's no work, a separate job runs prior to regular
    inventory runs which schedules trivial work on any DUT that appears
    idle.

    @param inventory  `_LabInventory` object to be reported on.
    """
    logging.info('Scanning for untestable DUTs.')
    for history in _all_dut_histories(inventory):
        # Managed DUTs with names that don't match
        # _HOSTNAME_PATTERN shouldn't be possible.  However, we
        # don't want arbitrary strings being attached to the
        # 'dut_hostname' field, so for safety, we exclude all
        # anomalies.
        if not _HOSTNAME_PATTERN.match(history.hostname):
            continue
        if _host_is_working(history):
            if _dut_in_repair_loop(history):
                _report_untestable_dut(history, 'repair_loop')
        elif _host_is_idle(history):
            if not history.host.locked:
                _report_untestable_dut(history, 'idle_unlocked')


def _log_startup(arguments, startup_time):
    """Log the start of this inventory run.

    Print various log messages indicating the start of the run.  Return
    a string based on `startup_time` that will be used to identify this
    run in log files and e-mail messages.

    @param startup_time   A UNIX timestamp marking the moment when
                          this inventory run began.
    @returns  A timestamp string that will be used to identify this run
              in logs and email output.
    """
    timestamp = time.strftime('%Y-%m-%d.%H',
                              time.localtime(startup_time))
    logging.debug('Starting lab inventory for %s', timestamp)
    if arguments.model_notify:
        if arguments.recommend:
            logging.debug('Will include repair recommendations')
        logging.debug('Will include model inventory')
    if arguments.pool_notify:
        logging.debug('Will include pool inventory')
    return timestamp


def _create_inventory(arguments, end_time):
    """Create the `_LabInventory` instance to use for reporting.

    @param end_time   A UNIX timestamp for the end of the time range
                      to be searched in this inventory run.
    """
    start_time = end_time - arguments.duration * 60 * 60
    afe = frontend_wrappers.RetryingAFE(server=None)
    inventory = _LabInventory.create_inventory(
            afe, start_time, end_time, arguments.modelnames)
    logging.info('Found %d hosts across %d models',
                     inventory.get_num_duts(),
                     inventory.get_num_models())
    return inventory


def _perform_inventory_reports(arguments):
    """Perform all inventory checks requested on the command line.

    Create the initial inventory and run through the inventory reports
    as called for by the parsed command-line arguments.

    @param arguments  Command-line arguments as returned by
                      `ArgumentParser`.
    """
    startup_time = time.time()
    timestamp = _log_startup(arguments, startup_time)
    inventory = _create_inventory(arguments, startup_time)
    if arguments.debug:
        _populate_model_counts(inventory)
    if arguments.model_notify:
        _perform_model_inventory(arguments, inventory, timestamp)
    if arguments.pool_notify:
        _perform_pool_inventory(arguments, inventory, timestamp)
    if arguments.report_untestable:
        _report_untestable_dut_metrics(inventory)


def _separate_email_addresses(address_list):
    """Parse a list of comma-separated lists of e-mail addresses.

    @param address_list  A list of strings containing comma
                         separate e-mail addresses.
    @return A list of the individual e-mail addresses.
    """
    newlist = []
    for arg in address_list:
        newlist.extend([email.strip() for email in arg.split(',')])
    return newlist


def _verify_arguments(arguments):
    """Validate command-line arguments.

    Join comma separated e-mail addresses for `--model-notify` and
    `--pool-notify` in separate option arguments into a single list.

    For non-debug uses, require that at least one inventory report be
    requested.  For debug, if a report isn't specified, treat it as "run
    all the reports."

    The return value indicates success or failure; in the case of
    failure, we also write an error message to stderr.

    @param arguments  Command-line arguments as returned by
                      `ArgumentParser`
    @return True if the arguments are semantically good, or False
            if the arguments don't meet requirements.
    """
    arguments.model_notify = _separate_email_addresses(
            arguments.model_notify)
    arguments.pool_notify = _separate_email_addresses(
            arguments.pool_notify)
    if not any([arguments.model_notify, arguments.pool_notify,
                arguments.report_untestable]):
        if not arguments.debug:
            sys.stderr.write('Must request at least one report via '
                             '--model-notify, --pool-notify, or '
                             '--report-untestable\n')
            return False
        else:
            # We want to run all the e-mail reports.  An empty notify
            # list will cause a report to be skipped, so make sure the
            # lists are non-empty.
            arguments.model_notify = ['']
            arguments.pool_notify = ['']
    return True


def _get_default_logdir(script):
    """Get the default directory for the `--logdir` option.

    The default log directory is based on the parent directory
    containing this script.

    @param script  Path to this script file.
    @return A path to a directory.
    """
    basedir = os.path.dirname(os.path.abspath(script))
    basedir = os.path.dirname(basedir)
    return os.path.join(basedir, _LOGDIR)


def _parse_command(argv):
    """Parse the command line arguments.

    Create an argument parser for this command's syntax, parse the
    command line, and return the result of the ArgumentParser
    parse_args() method.

    @param argv Standard command line argument vector; argv[0] is
                assumed to be the command name.
    @return Result returned by ArgumentParser.parse_args().
    """
    parser = argparse.ArgumentParser(
            prog=argv[0],
            description='Gather and report lab inventory statistics')
    parser.add_argument('-d', '--duration', type=int,
                        default=_DEFAULT_DURATION, metavar='HOURS',
                        help='number of hours back to search for status'
                             ' (default: %d)' % _DEFAULT_DURATION)
    parser.add_argument('--model-notify', action='append',
                        default=[], metavar='ADDRESS',
                        help='Generate model inventory message, '
                        'and send it to the given e-mail address(es)')
    parser.add_argument('--pool-notify', action='append',
                        default=[], metavar='ADDRESS',
                        help='Generate pool inventory message, '
                             'and send it to the given address(es)')
    parser.add_argument('-r', '--recommend', type=int, default=None,
                        help=('Specify how many DUTs should be '
                              'recommended for repair (default: no '
                              'recommendation)'))
    parser.add_argument('--report-untestable', action='store_true',
                        help='Check for devices unable to run tests.')
    parser.add_argument('--debug', action='store_true',
                        help='Print e-mail, metrics messages on stdout '
                             'without sending them.')
    parser.add_argument('--no-metrics', action='store_false',
                        dest='use_metrics',
                        help='Suppress generation of Monarch metrics.')
    parser.add_argument('--logdir', default=_get_default_logdir(argv[0]),
                        help='Directory where logs will be written.')
    parser.add_argument('modelnames', nargs='*',
                        metavar='MODEL',
                        help='names of models to report on '
                             '(default: all models)')
    arguments = parser.parse_args(argv[1:])
    if not _verify_arguments(arguments):
        return None
    return arguments


def _configure_logging(arguments):
    """Configure the `logging` module for our needs.

    How we log depends on whether the `--debug` option was provided on
    the command line.
      * Without the option, we configure the logging to capture all
        potentially relevant events in a log file.  The log file is
        configured to rotate once a week on Friday evening, preserving
        ~3 months worth of history.
      * With the option, we expect stdout to contain other
        human-readable output (including the contents of the e-mail
        messages), so we restrict the output to INFO level.

    For convenience, when `--debug` is on, the logging format has
    no adornments, so that a call like `logging.info(msg)` simply writes
    `msg` to stdout, plus a trailing newline.

    @param arguments  Command-line arguments as returned by
                      `ArgumentParser`
    """
    root_logger = logging.getLogger()
    if arguments.debug:
        root_logger.setLevel(logging.INFO)
        handler = logging.StreamHandler(sys.stdout)
        handler.setFormatter(logging.Formatter())
    else:
        if not os.path.exists(arguments.logdir):
            os.mkdir(arguments.logdir)
        root_logger.setLevel(logging.DEBUG)
        logfile = os.path.join(arguments.logdir, _LOGFILE)
        handler = logging.handlers.TimedRotatingFileHandler(
                logfile, when='W4', backupCount=13)
        formatter = logging.Formatter(_LOG_FORMAT,
                                      time_utils.TIME_FMT)
        handler.setFormatter(formatter)
    # TODO(jrbarnette) This is gross.  Importing client.bin.utils
    # implicitly imported logging_config, which calls
    # logging.basicConfig() *at module level*.  That gives us an
    # extra logging handler that we don't want.  So, clear out all
    # the handlers here.
    for h in root_logger.handlers:
        root_logger.removeHandler(h)
    root_logger.addHandler(handler)


def main(argv):
    """Standard main routine.

    @param argv  Command line arguments, including `sys.argv[0]`.
    """
    arguments = _parse_command(argv)
    if not arguments:
        sys.exit(1)
    _configure_logging(arguments)

    try:
        if arguments.use_metrics:
            if arguments.debug:
                logging.info('Debug mode: Will not report metrics to monarch.')
                metrics_file = '/dev/null'
            else:
                metrics_file = None
            with site_utils.SetupTsMonGlobalState(
                    'lab_inventory', debug_file=metrics_file,
                    auto_flush=False):
                success = False
                try:
                    with metrics.SecondsTimer('%s/duration' % _METRICS_PREFIX):
                        _perform_inventory_reports(arguments)
                    success = True
                finally:
                    metrics.Counter('%s/tick' % _METRICS_PREFIX).increment(
                            fields={'success': success})
                    metrics.Flush()
        else:
            _perform_inventory_reports(arguments)
    except KeyboardInterrupt:
        pass
    except Exception:
        # Our cron setup doesn't preserve stderr, so drop extra breadcrumbs.
        logging.exception('Error escaped main')
        raise


def get_inventory(afe):
    end_time = int(time.time())
    start_time = end_time - 24 * 60 * 60
    return _LabInventory.create_inventory(afe, start_time, end_time)


def get_managed_boards(afe):
    return get_inventory(afe).get_boards()


if __name__ == '__main__':
    main(sys.argv)
