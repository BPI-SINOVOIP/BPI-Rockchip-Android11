#!/usr/bin/python -B

"""Regenerates (just) ICU data source files used to build ICU data files."""

from __future__ import print_function

import os
import pipes
import shutil
import subprocess
import sys

import i18nutil
import icuutil


# Run with no arguments from any directory, with no special setup required.
# See icu4c/source/data/cldr-icu-readme.txt for the upstream ICU instructions.
def main():
  cldr_dir = icuutil.cldrDir()
  print('Found cldr in %s ...' % cldr_dir)
  icu_dir = icuutil.icuDir()
  print('Found icu in %s ...' % icu_dir)

  # Ant doesn't have any mechanism for using a build directory separate from the
  # source directory so this build script creates a temporary directory and then
  # copies all necessary ICU4J and CLDR source code to here before building it:
  i18nutil.SwitchToNewTemporaryDirectory()

  print('Copying ICU4J source code ...')
  shutil.copytree(os.path.join(icu_dir, 'icu4j'), 'icu4j', True)
  print('Building ICU4J ...')
  subprocess.check_call([
      'ant',
      '-f', 'icu4j/build.xml',
      'jar',
      'cldrUtil',
  ])

  # Append the newly built JAR files to the Java CLASSPATH to use these instead
  # of the pre-built JAR files in the CLDR source tree, to ensure that the same
  # version of ICU is being used to build the data as will use the data:
  cp = []
  try:
    cp.append(os.environ['CLASSPATH'])
  except KeyError:
    pass
  cp.append('icu4j/icu4j.jar')
  cp.append('icu4j/out/cldr_util/lib/utilities.jar')
  os.environ['CLASSPATH'] = ':'.join(cp)

  # This is the location of the original CLDR source tree (not the temporary
  # copy of the tools source code) from where the data files are to be read:
  os.environ['CLDR_DIR'] = os.path.join(os.getcwd(), 'cldr')

  print('Copying CLDR source code ...')
  shutil.copytree(cldr_dir, 'cldr', True)
  print('Building CLDR tools ...')
  subprocess.check_call([
      'ant',
      '-f', os.path.join('cldr', 'tools', 'java', 'build.xml'),
      'jar',
      'AddPseudolocales',
  ])

  # This is the temporary directory in which the CLDR tools have been built:
  os.environ['CLDR_CLASSES'] = os.path.join(os.getcwd(), 'cldr', 'tools', 'java', 'classes')

  # It's essential to set CLDR_DTD_CACHE for otherwise the repeated requests for
  # the same file will cause the unicode.org web server to block this machine:
  os.makedirs('cldr-dtd-cache')
  os.environ['ANT_OPTS'] = '-DCLDR_DTD_CACHE=%s' % pipes.quote(os.path.join(
      os.getcwd(), 'cldr-dtd-cache'))

  print('Building ICU data source files ...')
  subprocess.check_call([
      'ant',
      '-f', os.path.join(icu_dir, 'icu4c/source/data/build.xml'),
      'clean',
      'all',
  ])

  print('Look in %s/icu4c/source/data for new data source files' % icu_dir)
  sys.exit(0)

if __name__ == '__main__':
  main()
