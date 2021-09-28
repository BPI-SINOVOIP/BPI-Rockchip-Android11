#!/bin/bash

# Just in case realpath doesn't exist, try a perl approach.
function get_real_path()
{
  if realpath $0 &> /dev/null; then
    realpath $0
    return
  fi
  perl -e 'use Cwd "abs_path";print abs_path(shift)' $0
}
