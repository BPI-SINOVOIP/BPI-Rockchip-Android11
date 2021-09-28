#!/bin/bash

# $1 Path to the new version.
# $2 Path to the old version.

cp -a -n $2/build.gradle $1/
