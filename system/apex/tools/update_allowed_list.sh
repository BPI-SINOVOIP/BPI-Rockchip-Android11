#!/bin/bash

usage () {
  echo "Usage: $0 apex_allowed_list_file apex_contents_file"
}

if [[ $# -ne 2 ]]; then
  usage
  exit 1
fi

echo "Adding following files to $1:"
diff \
  --unchanged-group-format='' \
  --changed-group-format='%<' \
  $2 $1

cat $1 $2 | sort -u > $1
