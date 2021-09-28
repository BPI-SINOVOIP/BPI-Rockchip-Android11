#!/bin/bash

python3 -m pip install virtualenv

if [ $? -ne 0 ]; then
  echo "Virtualenv must be installed to run the upload tests. Run: " >&2
  echo "    sudo python3 -m pip install virtualenv" >&2
  exit 1
fi

virtualenv='/tmp/acts_preupload_virtualenv'

python3 -m virtualenv -p python3 $virtualenv
cp -r acts/framework $virtualenv/
cd $virtualenv/framework
$virtualenv/bin/python3 setup.py develop
cd -
