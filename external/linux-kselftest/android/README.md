VTS Kselftest Workflow
======================

A Guide to Developing Kselftest for VTS/Android

What is Kselftest?
------------------

Kselftest is a collection of tests included within the Linux kernel repository
at tools/testing/selftests. The tests cover a broad range of areas but total
coverage (as of 4.14 anyway) is very spotty. Many of the tests do not apply to
Android platforms.

As of mid 2018 there are 23 tests in kselftest which run on ARM in VTS.
Almost all of them get executed in both 32-bit and 64-bit mode. There are many
tests which are not enabled because they have dependencies which are not met on
Android platforms.

How is Kselftest Run in VTS?
----------------------------

The Kselftest source is located at external/linux-kselftest in the Android
tree. It is a set of files (tools/testing/selftests/) copied into its own
repository from the Linux kernel repository. This is not an exact mirror of
upstream; there are outstanding changes to kselftest for it to work on Android
which have not yet been pushed upstream (contained in android/patches). In
addition to the kselftest source there is also the VTS wrapper around it. This
is located at test/vts-testcase/kernel/linux_kselftest. Some noteworthy
directories/files:

* `external/linux-kselftest/Android.bp`: Lists build rules for the kselftest modules built in Soong.
* `external/linux-kselftest/android/`: Contains Android-specific files, aside from Android.[bp, mk] at top level.
* `external/linux-kselftest/android/Android.kselftest.mk`: Lists build rules for the kselftest modules built under make.
* `external/linux-kselftest/android/kselftest_test_list.mk`: Lists all tests that will get pulled into VTS - VTS depends on this list.
* `external/linux-kselftest/android/README`: Lists details on why particular tests are not enabled in Android.
* `external/linux-kselftest/tools/testing/selftests`: Kselftest sources.
* `test/vts-testcase/kernel/linux_kselftest/testcases/`: tests to run in VTS kselftest suites are
specified in each of the test XML file, e.g, vts_linux_kselftest_arm_32.xml contains tests for 32 bit ARM platform.

Running Kselftest through atest
-------------------------------
You can run Kselftest tests with atest, which handles all the setup and build steps.

To run all 32 bit Kselftest tests:
* `atest vts_linux_kselftest_arm`

To run all 64 bit Kselftest tests:
* `atest vts_linux_kselftest_arm_64`

To run a single test:
* `atest vts_linux_kselftest_arm_64:timers_valid-adjtimex_arm_64`

Running Kselftest through VTS
-----------------------------
To run VTS kselftest through VTS, it must first be built. VTS is not device specific,
you need not compile it specifically for the device you wish to run it on, assuming
it is the same architecture.
* `. build/envsetup.sh`
* `lunch`
* `make -j vts`

Then open vts-tradefed and run the VTS tests (64 bit ARM platform as an example):
* `vts-tradefed`
* `vts-tf > run vts-kernel -m vts_linux_kselftest_arm_64`

If you regularly work with multiple devices it may be useful to specify the
specific device you wish to run VTS on via the serial number:
* `vts-tf > run vts-kernel -m vts_linux_kselftest_arm_64 -s 000123456789`

Or a specific test:
* `vts-tf > run vts-kernel -m vts_linux_kselftest_arm_64 -t net_socket`

Running Kselftest Directly
--------------------------

Running kselftest tests within VTS can be quite cumbersome, especially if you
are iterating a lot trying to debug something. Build and run kselftest tests
faster by doing

* `external/linux-kselftest$ mma`
* `external/linux-kselftest$ adb sync data`

The test cases will be located at `/data/nativetest{64,}/linux-kselftest/`.

Build just the kselftest test you care about with `make -j kselftest_<testname>` (listed in `external/linux-kselftest/android/kselftest_test_list.mk`).

What Outstanding Issues Exist?
------------------------------

The hotlist for kselftest bugs is
[kselftest-todo](https://buganizer.corp.google.com/hotlists/745928).

When you begin working on a kselftest bug please assign the bug to yourself so
that others know it is being worked on.

Testing x86_64
--------------

It is not advisable to run kselftest tests directly on your host unless you are fully
aware of what the tests will do and are okay with it. These tests may
destabilize your box or cause data loss. If you need to run tests on an x86
platform and are unsure if they are safe you should run them in emulation, in a
virtualized environment, or on a dedicated development x86 platform.

To run Kselftest tests for x86 platform, you can do:
* `atest vts_linux_kselftest_x86`
* `atest vts_linux_kselftest_x86_64`

Sending Fixes Upstream
----------------------

Kselftest is part of the upstream Linux kernel and as such you can use the
get_maintainers.pl script there to see who to send patches to. Here’s an
example using a commit already upstream:

```
smuckle@smuckle:~/repos/linux$ git show 352909b49ba | scripts/get_maintainer.pl
Shuah Khan <shuah@kernel.org> (maintainer:KERNEL SELFTEST FRAMEWORK,commit_signer:2/6=33%,authored:2/6=33%,added_lines:2/6=33%,removed_lines:3/6=50%)
Thomas Gleixner <tglx@linutronix.de> (commit_signer:2/6=33%)
Greg Kroah-Hartman <gregkh@linuxfoundation.org> (commit_signer:1/6=17%,authored:1/6=17%,added_lines:1/6=17%)
"Radim Krčmář" <rkrcmar@redhat.com> (commit_signer:1/6=17%)
Ingo Molnar <mingo@kernel.org> (commit_signer:1/6=17%,commit_signer:1/1=100%)
Andy Lutomirski <luto@kernel.org> (authored:2/6=33%,added_lines:2/6=33%,removed_lines:2/6=33%,authored:1/1=100%,added_lines:500/500=100%)
"Kirill A. Shutemov" <kirill.shutemov@linux.intel.com> (authored:1/6=17%,added_lines:1/6=17%,removed_lines:1/6=17%)
linux-kernel@vger.kernel.org (open list)
linux-kselftest@vger.kernel.org (open list:KERNEL SELFTEST FRAMEWORK)
```

In summary patches should be sent to linux-kselftest@vger.kernel.org and
linux-kernel@vger.kernel.org, Shuah Khan (kselftest maintainer), and anyone
else who has touched the code in question. Standard Linux kernel coding style
and patch rules apply. They should be checkpatch (scripts/checkpatch.pl in the
kernel repository) clean and sent in plain text in canonical patch format. One
easy way to do this is by using git format-patch and git send-email.

Merging Fixes
-------------

When possible please merge fixes upstream first. Then cherrypick the change
onto aosp/master in external/linux-kselftest.

If your change cannot go upstream first for whatever reason, please commit a
patch for it into external/linux-kselftest/android/patches. This allows easier
tracking of the delta with upstream and streamlines upgrades to new kselftest
releases.

Updating Kselftest
------------------

To merge in a new upstream version of kselftest:
1. Do a git merge of the upstream tag with the "ours" policy, dropping all upstream changes. Do not commit yet.
2. Delete tools/testing/selftests and replace it with a copy from the upstream kernel at the same tag as used above.
3. Apply the patches in android/patches/, resolving conflicts as required.
4. Test on all supported kernel versions, ensuring that any tests currently enabled in VTS do not generate new failures.
5. Commit the merge.

If the runtime of kselftest changes significantly be sure to update the runtime-hint and test-timeout parameters to VTS in
test config files under
`test/vts-testcase/kernel/linux_kselftest/testcases/`.

Notes on Individual Tests
-------------------------

### bpf/

The bpf tests depend on the bpf and elf libraries, and several kernel
config options that are not required by Android.

### cpufreq/

Cpufreq is not required by Android.

### cpu-hotplug/

Cpu hotplug is not required by Android.

### filesystems/dnotify_test.c

This test has an endless loop in it. It is not currently run as part of the
kselftest suite.

### firmware/

These tests depend on a kernel module enabled by CONFIG_TEST_FIRMWARE, which is
not required by Android.

### ftrace/

Ftrace is a debug feature and not required by Android.

### gpio/

The gpio test requires a test kernel module and gpiolib, neither of which are
required by Android.

### ipc/

The ipc test requires CONFIG_EXPERT and CONFIG_CHECKPOINT_RESTORE be enabled,
neither of which are required by Android.

### kvm/

KVM is not required by Android.

### lib/

The lib/ tests rely on kernel modules enabled by CONFIG_TEST_PRINTF,
CONFIG_TEST_BITMAP, and CONFIG_PRIME_NUMBERS. None of these are required by
Android.

### locking/

The wait-wound mutex test depends on a kernel module enabled by
CONFIG_WW_MUTEX_SELFTEST which is not required by Android.

### media_tests/

The media_tests depend on /dev/mediaX and /dev/videoX files which may not be
present. They also require manual inspection of dmesg and are not part of
the normal kselftest suite.

### membarrier/

The membarrier test depends on CONFIG_EXPERT and CONFIG_MEMBARRIER being
enabled, neither of which are required by Android.

### memfd/

These tests depend on FUSE which is not present in Android.

### memory-hotplug/

Memory hotplug is not required by Android.

### mount/

The mount test depends on CONFIG_USER_NS which is not required by Android.

### mqueue/

These tests depend on -lrt and -lpopt which are not present in Android.

### net/

The test_bpf.sh test requires a kernel module enabled by CONFIG_TEST_BPF.
The netdevice.sh test operates on an ethernet interface.
The psock_fanout test depends on CONFIG_BPF_SYSCALL which is not required on
Android.

### netfilter/

These tests depend on CONFIG_NET_NS and CONFIG_NF_TABLES_INET, neither of which
are required by Android.

### nsfs/

These tests depend on CONFIG_USER_NS and CONFIG_PID_NS, neither of which are
required by Android.

### ntb/

NTB (non-transparent bridge) is not required by Android.

### pstore/

Pstore is recommended, not required, for Android.

### ptp/

PTP support is not required by Android.

### rseq/

The rseq system call, enabled by CONFIG_RSEQ, is not required by Android.

### sigaltstack/

This is a test for functionality that is not available in bionic (swapcontext()).

### static_keys/

This test depends on a kernel module enabled by CONFIG_TEST_STATIC_KEYS. This
kernel module is not required by Android.

### sync/

These tests verify the legacy sync API which is deprecated on Android.

### sysctl

This test depends on a kernel module enabled by CONFIG_TEST_SYSCTL. This kernel
module is not required by Android.

### timers/

The adjtick test adjusts the system time and takes 2 minutes.

The change_skew test takes too long to run as part of VTS.

The clocksource_switch test takes too long to run as part of VTS and also tries
all clocksources.

The freq_step test relies on clocks having a greater precision than what may be
available.

The leap-a-day test repeatedly modifies the system date in order to test leap
second behavior. This may disrupt the target environment.

The leapcrash test adjusts the system date and takes several minutes to run.

The mqueue_lat test relies on mq_* calls which are not present on Android.

The rtctest_setdate test modifies the RTC's time and date which may disrupt the
target environment.

The set-2038 test repeatedly sets the time in order to test edge cases. This
may disrupt the target enviornment. The test also takes a long time.

The set-tz test sets the timezone which may disrupt the target environment.

The skew_consistency test takes too long to run as part of VTS.

### uevent/

This test depends on CONFIG_USER_NS which is not required by Android.

### user/

This test depends on a kernel module enabled by CONFIG_TEST_USER_COPY. This
kernel module is not required by Android.

### vm/

The hugepage-mmap, hugepage-shm, compaction, and map_hugetlb tests rely on
hugetlbfs/hugetlb page support which is not required by Android.

The userfaultfd test relies on the userfaultfd syscall which is not required by
Android.

### watchdog/

The watchdog test depends on CONFIG_WATCHDOG which is not required by Android.

### zram/

The zram test depends on CONFIG_ZSMALLOC and CONFIG_ZRAM which are not required
by Android.
