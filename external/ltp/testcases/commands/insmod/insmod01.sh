#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (c) 2016 Fujitsu Ltd.
# Author: Guangwen Feng <fenggw-fnst@cn.fujitsu.com>
#
# Test basic functionality of insmod command.

TST_CLEANUP=cleanup
TST_TESTFUNC=do_test
TST_NEEDS_ROOT=1
TST_NEEDS_CMDS="rmmod insmod"
TST_NEEDS_MODULE="ltp_insmod01.ko"
. tst_test.sh

inserted=0

cleanup()
{
	if [ $inserted -ne 0 ]; then
		tst_res TINFO "running rmmod ltp_insmod01"
		rmmod ltp_insmod01
		if [ $? -ne 0 ]; then
			tst_res TWARN "failed to rmmod ltp_insmod01"
		fi
		inserted=0
	fi
}

do_test()
{
	insmod "$TST_MODPATH"
	if [ $? -ne 0 ]; then
		tst_res TFAIL "insmod failed"
		return
	fi
	inserted=1

	grep -q ltp_insmod01 /proc/modules
	if [ $? -ne 0 ]; then
		tst_res TFAIL "ltp_insmod01 not found in /proc/modules"
		return
	fi

	cleanup

	tst_res TPASS "insmod passed"
}

tst_run
