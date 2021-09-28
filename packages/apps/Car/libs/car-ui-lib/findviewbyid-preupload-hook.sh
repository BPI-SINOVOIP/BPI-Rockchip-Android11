#!/bin/bash

if grep -rq --exclude=CarUiUtils.java "findViewById\|requireViewById" car-ui-lib/src/com/android/car/ui/; then
    grep -r --exclude-CarUiUtils.java "findViewById\|requireViewById" car-ui-lib/src/com/android/car/ui/;
    echo "Illegal use of findViewById or requireViewById in car-ui-lib. Please consider using CarUiUtils#findViewByRefId or CarUiUtils#requireViewByRefId" && false;
fi

