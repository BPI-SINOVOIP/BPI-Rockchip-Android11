#!/bin/bash -e

# Flat buffer schema files
FLATC_SRCS="flatbuffers_types.fbs"

# Flatc arguments
FLATC_ARGS="--cpp --no-includes"

# Generate c++ code
flatc $FLATC_ARGS $FLATC_SRCS
