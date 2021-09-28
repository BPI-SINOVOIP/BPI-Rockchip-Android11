#!/bin/bash

# $1 Path to the new version.
# $2 Path to the old version.

cp -a -n $2/Lib/fontTools/Android.bp $1/Lib/fontTools/
