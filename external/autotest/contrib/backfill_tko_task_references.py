"""A simple script to backfill tko_task_references table with throttling."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import collections
import contextlib
import logging
import time

import MySQLdb


class BackfillException(Exception):
  pass


def _parse_args():
  parser = argparse.ArgumentParser(
      description=__doc__)
  parser.add_argument('--host', required=True, help='mysql server host')
  parser.add_argument('--user', required=True, help='mysql server user')
  parser.add_argument('--password', required=True, help='mysql server password')
  parser.add_argument('--dryrun', action='store_true', default=False)
  parser.add_argument(
      '--num-iterations',
      default=None,
      type=int,
      help='If set, total number of iterations. Default is no limit.',
  )
  parser.add_argument(
      '--batch-size',
      default=1000,
      help='Number of tko_jobs rows to read in one iteration',
  )
  parser.add_argument(
      '--sleep-seconds',
      type=int,
      default=1,
      help='Time to sleep between iterations',
  )

  args = parser.parse_args()
  if args.dryrun:
    if not args.num_iterations:
      logging.info('DRYRUN: Limiting to 5 iterations in dryrun mode.')
      args.num_iterations = 5
  return args



@contextlib.contextmanager
def _mysql_connection(args):
  conn = MySQLdb.connect(user=args.user, host=args.host, passwd=args.password)
  with _mysql_cursor(conn) as c:
    c.execute('USE chromeos_autotest_db;')
  try:
    yield conn
  finally:
    conn.close()


@contextlib.contextmanager
def _autocommit(conn):
  try:
    yield conn
  except:
    conn.rollback()
  else:
    conn.commit()


@contextlib.contextmanager
def _mysql_cursor(conn):
  c = conn.cursor()
  try:
    yield c
  finally:
    c.close()


def _latest_unfilled_job_idx(conn):
  with _mysql_cursor(conn) as c:
    c.execute("""
SELECT tko_job_idx
FROM tko_task_references
ORDER BY tko_job_idx
LIMIT 1
;""")
    r = c.fetchall()
    if r:
      return str(long(r[0][0]) - 1)
  logging.debug('tko_task_references is empty.'
               ' Grabbing the latest tko_job_idx to fill.')
  with _mysql_cursor(conn) as c:
    c.execute("""
SELECT job_idx
FROM tko_jobs
ORDER BY job_idx DESC
LIMIT 1
;""")
    r = c.fetchall()
    if r:
      return r[0][0]
  return None


_TKOTaskReference = collections.namedtuple(
    '_TKOTaskReference',
    ['tko_job_idx', 'task_reference', 'parent_task_reference'],
)

_SQL_SELECT_TASK_REFERENCES = """
SELECT job_idx, afe_job_id, afe_parent_job_id
FROM tko_jobs
WHERE job_idx <= %(latest_job_idx)s
ORDER BY job_idx DESC
LIMIT %(batch_size)s
;"""
_SQL_INSERT_TASK_REFERENCES = """
INSERT INTO tko_task_references(reference_type, tko_job_idx, task_id, parent_task_id)
VALUES %(values)s
;"""
_SQL_SELECT_TASK_REFERENCE = """
SELECT tko_job_idx FROM tko_task_references WHERE tko_job_idx = %(tko_job_idx)s
;"""


def _compute_task_references(conn, latest_job_idx, batch_size):
  with _mysql_cursor(conn) as c:
    sql = _SQL_SELECT_TASK_REFERENCES % {
        'latest_job_idx': latest_job_idx,
        'batch_size': batch_size,
    }
    c.execute(sql)
    rs = c.fetchall()
    if rs is None:
      return []

    return [_TKOTaskReference(r[0], r[1], r[2]) for r in rs]


def _insert_task_references(conn, task_references, dryrun):
  values = ', '.join([
      '("afe", %s, "%s", "%s")' %
      (tr.tko_job_idx, tr.task_reference, tr.parent_task_reference)
      for tr in task_references
  ])
  sql = _SQL_INSERT_TASK_REFERENCES % {'values': values}
  if dryrun:
    if len(sql) < 200:
      sql_log = sql
    else:
      sql_log = '%s... [SNIP] ...%s' % (sql[:150], sql[-49:])
    logging.debug('Would have run: %s', sql_log)
  with _autocommit(conn) as conn:
    with _mysql_cursor(conn) as c:
      c.execute(sql)


def _verify_task_references(conn, task_references):
  # Just verify that the last one was inserted.
  if not task_references:
    return
  tko_job_idx = task_references[-1].tko_job_idx
  sql = _SQL_SELECT_TASK_REFERENCE % {'tko_job_idx': tko_job_idx}
  with _mysql_cursor(conn) as c:
    c.execute(sql)
    r = c.fetchall()
    if not r or r[0][0] != tko_job_idx:
      raise BackfillException(
          'Failed to insert task reference for tko_job_id %s' % tko_job_idx)


def _next_job_idx(task_references):
  return str(long(task_references[-1].tko_job_idx) - 1)

def main():
  logging.basicConfig(level=logging.DEBUG)
  args = _parse_args()
  with _mysql_connection(args) as conn:
    tko_job_idx = _latest_unfilled_job_idx(conn)
    if tko_job_idx is None:
      raise BackfillException('Failed to get last unfilled tko_job_idx')
    logging.info('First tko_job_idx to fill: %s', tko_job_idx)

  while True:
    logging.info('####################################')
    logging.info('Start backfilling from tko_job_idx: %s', tko_job_idx)

    task_references = ()
    with _mysql_connection(args) as conn:
      task_references = _compute_task_references(
          conn, tko_job_idx, args.batch_size)
    if not task_references:
      logging.info('No more unfilled task references. All done!')
      break

    logging.info(
        'Inserting %d task references. tko_job_ids: %d...%d',
        len(task_references),
        task_references[0].tko_job_idx,
        task_references[-1].tko_job_idx,
    )
    with _mysql_connection(args) as conn:
      _insert_task_references(conn, task_references, args.dryrun)
    if not args.dryrun:
      with _mysql_connection(args) as conn:
        _verify_task_references(conn, task_references)

    tko_job_idx = _next_job_idx(task_references)

    if args.num_iterations is not None:
      args.num_iterations -= 1
      if args.num_iterations <= 0:
        break
      logging.info('%d more iterations left', args.num_iterations)
    logging.info('Iteration done. Sleeping for %d seconds', args.sleep_seconds)
    time.sleep(args.sleep_seconds)


if __name__ == '__main__':
  main()
