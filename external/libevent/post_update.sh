#!/bin/bash

# $1 Path to the new version.
# $2 Path to the old version.

cp -a -n $2/evconfig-private.h $1/
cp -a -n $2/include/event2/event-config.h $1/include/event2/
cp -a -n $2/include/event2/event-config-linux.h $1/include/event2/
cp -a -n $2/include/event2/event-config-darwin.h $1/include/event2/
cp -a -n $2/include/event2/event-config-bionic.h $1/include/event2/
