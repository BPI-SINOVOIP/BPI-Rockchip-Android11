#!/bin/bash

# $1 Path to the new version.
# $2 Path to the old version.

# Copies trove4j_src.jar for GPL compliance.
cp -a -n $2/lib/trove4j_src.jar $1/lib/
