#!/bin/sh

SRCDIR="${srcdir-`pwd`}"
BUILDDIR="${top_builddir-`pwd`}"

test="TEST"

if [ -n "$1" ] ; then
	test="$1"
fi

test -d "${BUILDDIR}/test" || mkdir "${BUILDDIR}/test/"

"${BUILDDIR}/intel-gen4asm" -o "${BUILDDIR}/${test}.out" "$SRCDIR/${test}.g4a"
if cmp "${BUILDDIR}/${test}.out" "${SRCDIR}/${test}.expected" 2> /dev/null; then : ; else
  echo "Output comparison for ${test}"
  diff -u "${SRCDIR}/${test}.expected" "${test}.out"
  exit 1;
fi
