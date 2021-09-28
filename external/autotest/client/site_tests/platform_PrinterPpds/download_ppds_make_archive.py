#!/usr/bin/env python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

import helpers as tools
import multithreaded_processor


# output location - temporary directory with all downloaded PPD files
path_dir = 'ppds_all'

# output location - name of the resultant archive with PPD files
path_archive = 'ppds_all.tar.xz'

# list of PPD files
ppd_files = []


def download_PPD(ppd_id):
    """
    Downloads given PPD file and save it to the directory given in path_dir.

    @param ppd_id: an index of the PPD filename in the list ppd_files

    """
    ppd_file = ppd_files[ppd_id]
    ppd = tools.download_PPD_file(ppd_file)
    path_ppd = os.path.join(path_dir, ppd_file)
    with open(path_ppd, 'wb') as file_out:
        file_out.write(ppd)


def main():
    """
    Downloads all available PPD files and creates and archive.

    See readme.txt for more details.

    """
    global path_dir
    global path_archive
    global ppd_files

    if os.path.exists(path_dir) or os.path.exists(path_archive):
        print ( 'Error: The file/directory "%s" or "%s" already exists. Delete'
                ' or move them and run this script again.' % (path_dir,
                path_archive) )
        sys.exit(1)

    # This object is used for running tasks in many threads simultaneously
    processor = multithreaded_processor.MultithreadedProcessor(10)

    # Downloads and extracts PPD filenames from all 20 index files (in parallel)
    outputs = processor.run(tools.get_filenames_from_PPD_index, 20)
    # joins obtained lists and performs deduplication
    set_ppd_files = set()
    for output in outputs:
        set_ppd_files.update(output)
    ppd_files = list(set_ppd_files)

    # Creates an output directory and downloads all PPD files there
    subprocess.call(['mkdir', path_dir])
    processor.run(download_PPD, len(ppd_files))

    # Creates the final archive
    subprocess.call(['tar', 'cJf', path_archive, path_dir])


if __name__ == '__main__':
    main()
