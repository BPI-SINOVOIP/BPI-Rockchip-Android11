#!/bin/bash
set -e
docker build -t ndkports .
docker run --rm -v $(pwd):/src ndkports
