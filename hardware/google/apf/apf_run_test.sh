#!/bin/bash

# Find out where we are and what we're called.
cd $(dirname $0)
testname=$(basename $(basename $0 .sh))

# All's well that ends well.
retcode=0

# Loop through testcases and run each one.
# Each testcase is composed of a program, a packet, optionally the starting data, and the output.
for prog in testdata/*.program; do
    testcase=$(basename $prog .program)
    prog=$(cat testdata/$testcase.program)
    pkt=$(cat testdata/$testcase.packet)
    outputpath=testdata/$testcase.output

    args="--trace --program $prog --packet $pkt"
    if [[ -f testdata/$testcase.data ]]; then
        args="$args --data $(cat testdata/$testcase.data)"
    fi

    if diff --color -u <(apf_run $args) <(cat $outputpath); then
        echo $testname: $testcase: PASS
    else
        echo $testname: $testcase: FAIL
        retcode=1
    fi
done

# Report pass/fail to the test runner.
exit $retcode
