#!/bin/bash
TEMP_MODULES_PATH=$1
find $TEMP_MODULES_PATH -type f -name *.ko | xargs basename -a > modules_scan_result.load
echo -e "\033[32mSave modules scan result as ./modules_scan_result.load \033[0m"
