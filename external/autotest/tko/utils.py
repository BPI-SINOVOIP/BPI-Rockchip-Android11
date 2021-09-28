import datetime
import os
import re
import sys


_debug_logger = sys.stderr
def dprint(msg):
    #pylint: disable-msg=C0111
    print >> _debug_logger, 'tko parser: %s' % msg


def redirect_parser_debugging(ostream):
    #pylint: disable-msg=C0111
    global _debug_logger
    _debug_logger = ostream


def get_timestamp(mapping, field):
    #pylint: disable-msg=C0111
    val = mapping.get(field, None)
    if val is not None:
        val = datetime.datetime.fromtimestamp(int(val))
    return val


def find_toplevel_job_dir(start_dir):
    """ Starting from start_dir and moving upwards, find the top-level
    of the job results dir. We can't just assume that it corresponds to
    the actual job.dir, because job.dir may just be a subdir of the "real"
    job dir that autoserv was launched with. Returns None if it can't find
    a top-level dir.
    @param start_dir: starting directing for the upward search"""
    job_dir = start_dir
    while not os.path.exists(os.path.join(job_dir, ".autoserv_execute")):
        if job_dir == "/" or job_dir == '':
            return None
        job_dir = os.path.dirname(job_dir)
    return job_dir


def drop_redundant_messages(messages):
    """ Given a set of message strings discard any 'redundant' messages which
    are simple a substring of the existing ones.

    @param messages - a set of message strings

    @return - a subset of messages with unnecessary strings dropped
    """
    sorted_messages = sorted(messages, key=len, reverse=True)
    filtered_messages = set()
    for message in sorted_messages:
        for filtered_message in filtered_messages:
            if message in filtered_message:
                break
        else:
            filtered_messages.add(message)
    return filtered_messages


def get_afe_job_id(tag):
    """ Given a tag return the afe_job_id (if any).

    Tag is in the format of JOB_ID-OWNER/HOSTNAME

    @param tag: afe_job_id and hostname are extracted from this tag.
                e.g. "1234-chromeos-test/chromeos1-row1-host1"
    @return: the afe_job_id as a string if regex matches, else return ''
    """
    match = re.search('^([0-9]+)-.+/(.+)$', tag)
    return match.group(1) if match else None


def get_skylab_task_id(tag):
    """ Given a tag return the skylab_task's id (if any).

    Tag is in the format of swarming-TASK_ID/HOSTNAME

    @param tag: afe_job_id and hostname are extracted from this tag.
                e.g. "1234-chromeos-test/chromeos1-row1-host1"
    @return: the afe_job_id as a string if regex matches, else return ''
    """
    match = re.search('^swarming-([A-Fa-f0-9]+)/(.+)$', tag)
    return match.group(1) if match else None


def is_skylab_task(tag):
    return get_skylab_task_id(tag) is not None
