#!/usr/bin/env python
import sys
import os
import re
import zipfile
import shutil

def main(argv):
  found=False
  parameter_file = open(argv[1], 'r')

  parten_str='0[xX][a-zA-Z0-9]+@0[xX][a-zA-Z0-9]+\(' + argv[2] + '\)'
  for line in parameter_file.readlines():
    p = re.search(parten_str, line)
    if p:
      part_info = p.group(0)
      size = re.search(r"0[xX][a-zA-Z0-9]+", part_info)
      if size:
        found=True
        print(int(size.group(0), 16)*512)
  if found == False:
    print(0)
  parameter_file.close()

if __name__=="__main__":
  main(sys.argv)
