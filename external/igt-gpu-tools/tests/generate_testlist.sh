#!/bin/bash

OUTPUT=$1
shift

echo TESTLIST > $OUTPUT

if [[ $# -gt 0 ]] ; then
	echo -n $1 >> $OUTPUT
	shift
fi

while [[ $# -gt 0 ]] ; do
	echo -n " $1" >> $OUTPUT
	shift
done

echo -e "\nEND TESTLIST" >> $OUTPUT
