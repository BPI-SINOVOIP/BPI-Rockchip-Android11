#!/bin/bash

result=$(find car-ui-lib/ -type f \( -iname \*.java -o -iname \*.xml \) -a \( ! -wholename \*/.idea/\* \) \( ! -wholename \*/build/\* \) -print0 | xargs -0 -L1 bash -c 'test "$(tail -c 1 "$0")" && echo "No new line at end of $0"')
if [ \( ! -z "$result" \)  -o \( $(echo "$result" | wc -l) -gt 1 \) ]
then
    echo "$result" && false;
fi
