#!/bin/bash
#
# Copyright © 2014 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

#
# Check that command line handling works consistently across all tests
#

# top_builddir is not set during distcheck. Distcheck executes this
# script in the directory where the built binaries are so just use '.'
# as the directory if top_builddir is not set.

tests_dir="$top_builddir"
if [ -z "$tests_dir" ]; then
	tests_dir="."
fi

# Manually running this script is possible in the source root or the
# tests directory.

fail () {
	echo "FAIL: $1"
	exit 1
}

check_test ()
{
	local test

	test=$1

	if [ "$test" = "TESTLIST" -o "$test" = "END" ]; then
		return
	fi

	testname="$test"
	if [ -x "$tests_dir/$test" ]; then
		test="$tests_dir/$test"
	else
		# Possibly a script, not found in builddir but in srcdir
		if [ -x "$srcdir/$test" ]; then
			test="$srcdir/$test"
		else
			fail "Cannot execute $test"
		fi
	fi

	echo "$test:"

	# check invalid option handling
	echo "  Checking invalid option handling..."
	./$test --invalid-option 2> /dev/null && fail $test

	# check valid options succeed
	echo "  Checking valid option handling..."
	./$test --help > /dev/null || fail $test

	# check --list-subtests works correctly
	echo "  Checking subtest enumeration..."
	LIST=`./$test --list-subtests`
	RET=$?
	if [ $RET -ne 0 -a $RET -ne 79 ]; then
		fail $test
	fi

	if [ $RET -eq 79 -a -n "$LIST" ]; then
		fail $test
	fi

	if [ $RET -eq 0 -a -z "$LIST" ]; then
		# Subtest enumeration of kernel selftest launchers depends
		# on the running kernel. If selftests are not enabled,
		# they will output nothing and exit with 0.
		if [ "$testname" != "i915_selftest" -a "$testname" != "drm_mm" -a "$testname" != "kms_selftest" -a "$testname" != "dmabuf" ]; then
			fail $test
		fi
	fi
}

TESTLISTFILE="$tests_dir/test-list.txt"
if [ ! -r "$TESTLISTFILE" ]; then
	tests_dir="tests"
	TESTLISTFILE="$tests_dir/test-list.txt"
fi

TESTLIST=`cat $TESTLISTFILE`
if [ $? -ne 0 ]; then
	echo "Error: Could not read test lists"
	exit 99
fi

if [ -n "$1" ] ; then
	check_test $1
	exit 0
fi

for test in $TESTLIST; do
	check_test $test
done
