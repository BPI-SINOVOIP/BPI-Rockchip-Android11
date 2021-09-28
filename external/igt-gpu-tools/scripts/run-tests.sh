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


ROOT="`dirname $0`"
ROOT="`readlink -f $ROOT/..`"
IGT_CONFIG_PATH="`readlink -f ${IGT_CONFIG_PATH:-$HOME/.igtrc}`"
RESULTS="$ROOT/results"
PIGLIT=`which piglit 2> /dev/null`

if [ -z "$IGT_TEST_ROOT" ]; then
	paths=("$ROOT/build/tests/test-list.txt"
	       "$ROOT/tests/test-list.txt")
	for p in "${paths[@]}"; do
		if [ -f "$p" ]; then
			echo "Found test list: \"$p\""
			IGT_TEST_ROOT=$(dirname "$p")
			break
		fi
	done
fi

if [ -z "$IGT_TEST_ROOT" ]; then
	echo "Error: test list not found."
	echo "Please build tests to generate the test list or use IGT_TEST_ROOT env var."
	exit 1
fi

IGT_TEST_ROOT="`readlink -f ${IGT_TEST_ROOT}`"

function download_piglit {
	git clone https://anongit.freedesktop.org/git/piglit.git "$ROOT/piglit"
}

function run_piglit # as-root <args>
{
	local need_root=$1
	shift
	local sudo

	export IGT_TEST_ROOT IGT_CONFIG_PATH

	if [ "$need_root" -ne 0 -a "$EUID" -ne 0 ]; then
		sudo="sudo --preserve-env=IGT_TEST_ROOT,IGT_CONFIG_PATH"
	fi

	$sudo $PIGLIT "$@"
}

function print_help {
	echo "Usage: run-tests.sh [options]"
	echo "Available options:"
	echo "  -d              download Piglit to $ROOT/piglit"
	echo "  -h              display this help message"
	echo "  -l              list all available tests"
	echo "  -r <directory>  store the results in directory"
	echo "                  (default: $RESULTS)"
	echo "  -s              create html summary"
	echo "  -t <regex>      only include tests that match the regular expression"
	echo "                  (can be used more than once)"
	echo "  -T <filename>   run tests listed in testlist"
	echo "                  (overrides -t and -x)"
	echo "  -v              enable verbose mode"
	echo "  -x <regex>      exclude tests that match the regular expression"
	echo "                  (can be used more than once)"
	echo "  -R              resume interrupted test where the partial results"
	echo "                  are in the directory given by -r"
	echo "  -n              do not retry incomplete tests when resuming a"
	echo "                  test run with -R"
	echo ""
	echo "Useful patterns for test filtering are described in the API documentation."
}

while getopts ":dhlr:st:T:vx:Rn" opt; do
	case $opt in
		d) download_piglit; exit ;;
		h) print_help; exit ;;
		l) LIST_TESTS="true" ;;
		r) RESULTS="$OPTARG" ;;
		s) SUMMARY="html" ;;
		t) FILTER="$FILTER -t $OPTARG" ;;
		T) FILTER="$FILTER --test-list $OPTARG" ;;
		v) VERBOSE="-v" ;;
		x) EXCLUDE="$EXCLUDE -x $OPTARG" ;;
		R) RESUME="true" ;;
		n) NORETRY="--no-retry" ;;
		:)
			echo "Option -$OPTARG requires an argument."
			exit 1
			;;
		\?)
			echo "Unknown option: -$OPTARG"
			print_help
			exit 1
			;;
	esac
done
shift $(($OPTIND-1))

if [ "x$1" != "x" ]; then
	echo "Unknown option: $1"
	print_help
	exit 1
fi

if [ "x$PIGLIT" == "x" ]; then
	PIGLIT="$ROOT/piglit/piglit"
fi

if [ ! -x "$PIGLIT" ]; then
	echo "Could not find Piglit."
	echo "Please install Piglit or use -d to download Piglit locally."
	exit 1
fi

if [ "x$LIST_TESTS" != "x" ]; then
	run_piglit 0 print-cmd --format "{name}" igt
	exit
fi

if [ "x$RESUME" != "x" ]; then
	run_piglit 1 resume "$RESULTS" $NORETRY
else
	mkdir -p "$RESULTS"
	run_piglit 1 run igt --ignore-missing -o "$RESULTS" -s $VERBOSE $EXCLUDE $FILTER
fi

if [ "$SUMMARY" == "html" ]; then
	run_piglit 0 summary html --overwrite "$RESULTS/html" "$RESULTS"
	echo "HTML summary has been written to $RESULTS/html/index.html"
fi
