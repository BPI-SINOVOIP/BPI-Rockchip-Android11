#!/bin/bash -eu

GOOD_STATUS=0
BAD_STATUS=1
SKIP_STATUS=125
PROBLEM_STATUS=127

tmp_file=$(mktemp)
trap "rm -f '${tmp_file}'" EXIT
grep -v '^ ' "$1" > "${tmp_file}"

if grep -q bad "${tmp_file}"; then
  exit $BAD_STATUS
fi

# func_a containing '2' in its top line is BAD
if grep -q 'func_a.*2' "${tmp_file}"; then
  exit $BAD_STATUS
fi

# func_b, func_c, and func_d with even values are bad in conjunction
if grep -q 'func_b.*4' "${tmp_file}" && \
  grep -q 'func_c.*6' "${tmp_file}" && \
  grep -q 'func_d.*8' "${tmp_file}"; then
  exit $BAD_STATUS
fi

# If none of the BADness conditions are met
exit $GOOD_STATUS
