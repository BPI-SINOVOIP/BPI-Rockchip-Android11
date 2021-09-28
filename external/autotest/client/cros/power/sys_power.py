# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides utility methods for controlling powerd in ChromiumOS."""

import errno
import fcntl
import logging
import multiprocessing
import os
import time

import common
from autotest_lib.client.cros import rtc
from autotest_lib.client.cros import upstart

SYSFS_POWER_STATE = '/sys/power/state'
SYSFS_WAKEUP_COUNT = '/sys/power/wakeup_count'

PAUSE_ETHERNET_HOOK_FILE = '/run/autotest_pause_ethernet_hook'
pause_ethernet_file = None


class SuspendFailure(Exception):
    """Base class for a failure during a single suspend/resume cycle."""
    pass


class SuspendTimeout(SuspendFailure):
    """Suspend took too long, got wakeup event (RTC tick) before it was done."""
    pass


class KernelError(SuspendFailure):
    """Kernel problem encountered during suspend/resume.

    Whitelist is an array of a 2-entry tuples consisting of (source regexp, text
    regexp).  For example, kernel output might look like,

      [  597.079950] WARNING: CPU: 0 PID: 21415 at \
      <path>/v3.18/drivers/gpu/drm/i915/intel_pm.c:3687 \
      skl_update_other_pipe_wm+0x136/0x18a()
      [  597.079962] WARN_ON(!wm_changed)

    source regexp should match first line above while text regexp can match
    up to 2 lines below the source.  Note timestamps are stripped prior to
    comparing regexp.
    """
    WHITELIST = [
        # crosbug.com/37594: debug tracing clock desync we don't care about
        (r'kernel/trace/ring_buffer.c:\d+ rb_reserve_next_event',
         r'Delta way too big!'),
        # TODO(crosbug.com/p/52008): Remove from whitelist once watermark
        # implementation has landed.
        (r'v3.18/\S+/intel_pm.c:\d+ skl_update_other_pipe_wm',
         r'WARN_ON\(\!wm_changed\)')
    ]


class FirmwareError(SuspendFailure):
    """String 'ERROR' found in firmware log after resume."""
    WHITELIST = [
        # crosbug.com/36762: no one knows, but it has always been there
        ('^stumpy', r'PNP: 002e\.4 70 irq size: 0x0000000001 not assigned'),
        # crbug.com/221538: no one knows what ME errors mean anyway
        ('^parrot', r'ME failed to respond'),
        # b/64684441: eve SKU without eMMC
        ('^eve', r'Card did not respond to voltage select!'),
    ]


class SpuriousWakeupError(SuspendFailure):
    """Received spurious wakeup while suspending or woke before schedule."""
    S3_WHITELIST = [  # (<board>, <eventlog wake source>, <syslog wake source>)
        # crbug.com/220014: spurious trackpad IRQs
        ('^link', 'Wake Source | GPIO | 12', ''),
        # crbug.com/345327: unknown, probably same as crbug.com/290923
        ('^x86-alex', '', ''),   # alex can report neither, blanket ignore
        # crbug.com/355106: unknown, possibly related to crbug.com/290923
        ('^lumpy|^parrot', '', 'PM1_STS: WAK PWRBTN'),
    ]
    S0_WHITELIST = [  # (<board>, <kernel wake source>)
        # crbug.com/290923: spurious keyboard IRQ, believed to be from Servo
        ('^x86-alex|^lumpy|^parrot|^butterfly', 'serio0'),
        # crosbug.com/p/46140: battery event caused by MKBP
        ('^elm|^oak', 'spi32766.0'),
    ]


class MemoryError(SuspendFailure):
    """memory_suspend_test found memory corruption."""
    pass


class SuspendNotAllowed(SuspendFailure):
    """Suspend was not allowed to be performed."""
    pass


class S0ixResidencyNotChanged(SuspendFailure):
    """power_SuspendStress test found CPU/SoC is unable to idle properly
    when suspended to S0ix. """
    pass


def prepare_wakeup(seconds):
    """Prepare the device to wake up from an upcoming suspend.

    @param seconds: The number of seconds to allow the device to suspend.
    """
    # May cause DUT not wake from sleep if the suspend time is 1 second.
    # It happens when the current clock (floating point) is close to the
    # next integer, as the RTC sysfs interface only accepts integers.
    # Make sure it is larger than or equal to 2.
    assert seconds >= 2
    wakeup_count = read_wakeup_count()
    estimated_alarm = int(rtc.get_seconds() + seconds)
    logging.debug('Suspend for %d seconds, estimated wakealarm = %d',
                  seconds, estimated_alarm)
    return (estimated_alarm, wakeup_count)


def check_wakeup(estimated_alarm):
    """Verify that the device did not wakeup early.

    @param estimated_alarm: The lower bound time at which the device was
                            expected to wake up.
    """
    now = rtc.get_seconds()
    if now < estimated_alarm:
        logging.error('Woke up early at %d', now)
        raise SpuriousWakeupError('Woke from suspend early')


def pause_check_network_hook():
    """Stop check_ethernet.hook from running.

    Lock will be held until test exits, or resume_check_network_hook() is
    called. check_ethernet.hook may also "break" the lock if it judges you've
    held it too long.

    Can be called multiple times, to refresh the lock.
    """
    # If this function is called multiple times, we intentionally re-assign
    # this global, which closes out the old lock and grabs it anew.
    # We intentionally "touch" the file to update its mtime, so we can judge
    # how long locks are held.
    global pause_ethernet_file
    pause_ethernet_file = open(PAUSE_ETHERNET_HOOK_FILE, 'w+')
    try:
        # This is a blocking call unless an error occurs.
        fcntl.flock(pause_ethernet_file, fcntl.LOCK_EX)
    except IOError:
        pass


def resume_check_network_hook():
    """Resume check_ethernet.hook.

    Must call pause_check_network_hook() at least once before calling this.
    """
    global pause_ethernet_file
    # Closing the file descriptor releases the lock.
    pause_ethernet_file.close()
    pause_ethernet_file = None


def do_suspend(wakeup_timeout, delay_seconds=0):
    """Suspend using the power manager with a wakeup timeout.

    Wait for |delay_seconds|, suspend the system(S3/S0iX) using
    powerd_dbus_suspend program. powerd_dbus_suspend first sets a wakeup alarm
    on the dut for |current_time + wakeup_timeout|. Thus the device tries to
    resume at |current_time + wakeup_timeout| irrespective of when it suspended.
    This means that RTC can trigger an early resume and prevent suspend.

    Function will block until suspend/resume has completed or failed.
    Returns the wake alarm time from the RTC as epoch.

    @param wakeup_timeout: time from now after which the device has to.
    @param delay_seconds: Number of seconds wait before suspending the DUT.

    """
    pause_check_network_hook()
    estimated_alarm, wakeup_count = prepare_wakeup(wakeup_timeout)
    upstart.ensure_running('powerd')
    command = ('/usr/bin/powerd_dbus_suspend --delay=%d --timeout=30 '
               '--wakeup_count=%d --wakeup_timeout=%d' %
               (delay_seconds, wakeup_count, wakeup_timeout))
    logging.info("Running '%s'", command)
    os.system(command)
    check_wakeup(estimated_alarm)
    return estimated_alarm


def suspend_for(time_in_suspend, delay_seconds=0):
    """Suspend using the power manager and spend |time_in_suspend| in suspend.

    Wait for |delay_seconds|, suspend the system(S3/S0iX) using
    powerd_dbus_suspend program. power manager sets a wakeup alarm on the dut
    for |time_in_suspend| just before suspending
    (writing power state to /sys/power/state). Thus the device spends most of
    |time_in_suspend| in S0iX/S3.

    Function will block until suspend/resume has completed or failed.
    Returns the wake alarm time from the RTC as epoch.

    @param time_in_suspend: Number of seconds to suspend the DUT.
    @param delay_seconds: Number of seconds wait before suspending the DUT.
    """
    pause_check_network_hook()

    wakeup_count = read_wakeup_count()
    upstart.ensure_running('powerd')
    command = ('/usr/bin/powerd_dbus_suspend --delay=%d --timeout=30 '
               '--wakeup_count=%d --suspend_for_sec=%d' %
               (delay_seconds, wakeup_count, time_in_suspend))
    logging.info("Running '%s'", command)
    os.system(command)


def suspend_bg_for_dark_resume(suspend_seconds, delay_seconds=0):
    """Do a non-blocking suspend using power manager.

    Wait for |delay_seconds|, then suspend with an rtc alarm for
    waking up again after having been suspended for |suspend_seconds|, using
    the powerd_dbus_suspend program all in the background.

    @param suspend_seconds: Number of seconds to suspend the DUT.
    @param delay_seconds: Number of seconds wait before suspending the DUT.

    """
    upstart.ensure_running('powerd')
    # Disarm any existing wake alarms so as to prevent early wakeups.
    os.system('echo 0 > /sys/class/rtc/rtc0/wakealarm')
    wakeup_count = read_wakeup_count()
    command = ('/usr/bin/powerd_dbus_suspend --delay=%d --timeout=30 '
               '--wakeup_count=%d --wakeup_timeout=%d '
               '--disable_dark_resume=false' %
               (delay_seconds, wakeup_count, suspend_seconds))
    logging.info("Running '%s'", command)
    process = multiprocessing.Process(target=os.system, args=(command,))
    process.start()


def kernel_suspend(seconds, state='mem'):
    """Do a kernel suspend.

    Suspend the system to @state, waking up again after |seconds|, by directly
    writing to /sys/power/state. Function will block until suspend/resume has
    completed or failed.

    @param seconds: The number of seconds to suspend the device.
    @param state: power state to suspend to.  DEFAULT mem.
    """
    alarm, wakeup_count = prepare_wakeup(seconds)
    rtc.set_wake_alarm(alarm)
    logging.debug('Saving wakeup count: %d', wakeup_count)
    write_wakeup_count(wakeup_count)
    try:
        logging.info('Suspending at %d', rtc.get_seconds())
        with open(SYSFS_POWER_STATE, 'w') as sysfs_file:
            sysfs_file.write(state)
    except IOError as e:
        logging.exception('Writing to %s failed', SYSFS_POWER_STATE)
        if e.errno == errno.EBUSY and rtc.get_seconds() >= alarm:
            # The kernel returns EBUSY if it has to abort because
            # another wakeup fires before we've reached suspend.
            raise SpuriousWakeupError('Received spurious wakeup in kernel.')
        else:
            # Some driver probably failed to suspend properly.
            # A hint as to what failed is in errno.
            raise KernelError('Suspend failed: %s' % e.strerror)
    else:
        logging.info('Woke from suspend at %d', rtc.get_seconds())
    logging.debug('New wakeup count: %d', read_wakeup_count())
    check_wakeup(alarm)


def idle_suspend(seconds):
    """
    Wait for the system to suspend to RAM (S3), scheduling the RTC to wake up
    |seconds| after this function was called. Caller must ensure that the system
    will idle-suspend in time for this to happen. Returns the wake alarm time
    from the RTC as epoch.

    @param seconds: The number of seconds before wakeup.
    """
    alarm, _ = prepare_wakeup(seconds)
    rtc.set_wake_alarm(alarm)
    while rtc.get_seconds() < alarm:
        time.sleep(0.2)

    # tell powerd something happened, or it will immediately idle-suspend again
    # TODO: switch to cros.power_utils#call_powerd_dbus_method once this
    # horrible mess with the WiFi tests and this file's imports is solved
    logging.debug('Simulating user activity after idle suspend...')
    os.system('dbus-send --type=method_call --system '
              '--dest=org.chromium.PowerManager /org/chromium/PowerManager '
              'org.chromium.PowerManager.HandleUserActivity')

    return alarm


def memory_suspend(seconds, size=0):
    """Do a memory suspend.

    Suspend the system to RAM (S3), waking up again after |seconds|, using
    the memory_suspend_test tool. Function will block until suspend/resume has
    completed or failed.

    @param seconds: The number of seconds to suspend the device.
    @param size: Amount of memory to allocate, in bytes.
                 Set to 0 to let memory_suspend_test determine amount of memory.
    """
    # since we cannot have utils.system_output in here, we need a workaround
    output = '/tmp/memory_suspend_output'
    wakeup_count = read_wakeup_count()
    status = os.system('/usr/bin/memory_suspend_test --wakeup_count=%d '
                       '--size=%d > %s --suspend_for_sec=%d' %
                       (wakeup_count, size, output, seconds))
    status = os.WEXITSTATUS(status)
    if status == 2:
        logging.error('memory_suspend_test found the following errors:')
        for line in open(output, 'r'):
            logging.error(line)
        raise MemoryError('Memory corruption found after resume')
    elif status == 1:
        raise SuspendFailure('Failure in powerd_suspend during memory test')
    elif status:
        raise SuspendFailure('Unknown failure in memory_suspend_test (crash?)')


def read_wakeup_count():
    """
    Retrieves the current value of /sys/power/wakeup_count.
    """
    with open(SYSFS_WAKEUP_COUNT) as sysfs_file:
        return int(sysfs_file.read().rstrip('\n'))


def write_wakeup_count(wakeup_count):
    """Writes a value to /sys/power/wakeup_count.

    @param wakeup_count: The wakeup count value to write.
    """
    try:
        with open(SYSFS_WAKEUP_COUNT, 'w') as sysfs_file:
            sysfs_file.write(str(wakeup_count))
    except IOError as e:
        if (e.errno == errno.EINVAL and read_wakeup_count() != wakeup_count):
            raise SpuriousWakeupError('wakeup_count changed before suspend')
        else:
            raise
