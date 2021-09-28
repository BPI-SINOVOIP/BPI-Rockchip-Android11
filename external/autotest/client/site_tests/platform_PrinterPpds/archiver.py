# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import os
import shutil
import subprocess
import threading

class Archiver():
    """
    An instance of this class stores set of files in given directory on local
    filesystem. Stored files are automatically compressed and organized into
    tar.xz archives based on their filenames prefixes. It is a very useful tool
    when one has to deal with many files with similar content that are generated
    continuously. Packing similar files together into tar.xz archive can
    singificantly reduce amount of required disk space (even for gzipped files).
    As a parameter, the constructor takes set of filenames prefixes. These
    prefixes are automatically clustered into archives by their common prefixes
    (yes, prefixes of prefixes). These archives are automatically created, when
    all files assigned to the given set of prefixes is added to Archiver object.
    Methods provided by this class are synchronized and can be called from
    different Python threads.

    """

    def _split_names_by_prefixes(
            self, names, max_names_per_prefix, prefix_length=0):
        """
        Recursive function used to split given set of names into groups by
        common prefixes. It tries to find configuration with minimum number of
        groups (prefixes) where the number of elements (names) in each group is
        not larger than given parameter.

        @param names: list of names to split into groups (names MUST BE sorted
                and unique).
        @param max_names_per_prefix: maximum number of names assigned to
                group (single prefix).
        @param prefix_length: current length of the prefix (for recursive
                calls); all elements in the list given as the parameter 'names'
                MUST HAVE the same prefix with this length.
        @returns dictionary with prefixes (each one represents single group) and
                size (a number of names in the group).

        """
        assert max_names_per_prefix > 1
        # Returns the current prefix if the group is small enough
        if len(names) <= max_names_per_prefix:
            return { names[0][0:prefix_length] : len(names) }
        # Increases prefix_length until a difference is found:
        # - elements in 'names' are sorted and unique
        # - elements in 'names' have a common prefix with a length of
        #   'prefix_length' characters
        while ( len(names[0]) > prefix_length and
                names[0][prefix_length] == names[-1][prefix_length] ):
            prefix_length += 1
        # Checks for special case, when the first name == prefix
        if len(names[0]) == prefix_length:
            return { names[0][0:prefix_length] : len(names) }
        # Calculates resultant list of prefixes
        results = dict()
        i_begin = 0
        # Calculates all prefixes (groups) using recursion:
        # - 'prefix_length' points to the first character that differentiates
        #   elements from the 'names' list
        while i_begin < len(names):
            char = names[i_begin][prefix_length]
            i_end = i_begin + 1
            while i_end < len(names) and char == names[i_end][prefix_length]:
                i_end += 1
            results.update(self._split_names_by_prefixes(names[i_begin:i_end],
                    max_names_per_prefix, prefix_length+1))
            i_begin = i_end
        return results


    def __init__(self, path_directory, prefixes, max_prefixes_per_archive):
        """
        Constructor.

        @param path_directory: directory where files and archives are stored.
                It is created if not exists.
        @param prefixes: a set of allowed filenames prefixes.
        @param max_prefixes_per_archive: maximum number of filenames prefixes
                assigned to single group (archive).

        """
        self._lock = threading.Lock()
        self._path_directory = path_directory
        if not os.path.exists(self._path_directory):
            os.makedirs(self._path_directory)

        prefixes = sorted(set(prefixes))
        self._archives_names = self._split_names_by_prefixes(prefixes,
                max_prefixes_per_archive)
        self._filenames_prefixes = dict()
        prefixes.reverse()
        for ap, fc in sorted(self._archives_names.iteritems()):
            self._archives_names[ap] = [fc, []]
            while fc > 0:
                self._filenames_prefixes[prefixes.pop()] = [ap, set()]
                fc -= 1


    def save_file(self, prefix, name, content, apply_gzip=False):
        """
        Add a new file with given content to the archive.

        @param prefix: prefix of filename that the new file will be saved with
        @param name: the rest of the filename of the new file; in summary, the
                resultant filename of the new file will be prefix+name
        @param content: a content of the file
        @param apply_gzip: if true, the added file will be gzipped, the suffix
                .gz will be added to its resultant filename

        """
        if apply_gzip:
            name += ".gz"
        path_target = os.path.join(self._path_directory, prefix + name)

        with self._lock:
            assert prefix in self._filenames_prefixes
            assert self._filenames_prefixes[prefix][1] is not None
            assert name not in self._filenames_prefixes[prefix][1]
            self._filenames_prefixes[prefix][1].add(name)

        if apply_gzip:
            file_target = gzip.GzipFile(path_target, 'wb', 9, None, 0)
        else:
            file_target = open(path_target, 'wb')
        with file_target:
            file_target.write(content)


    def copy_file(self, prefix, name, path_file, apply_gzip=False):
        """
        Add a new file to the archive. The file is copied from given location.

        @param prefix: prefix of filename that the new file will be saved with
        @param name: the rest of the filename of the new file; in summary, the
                resultant filename of the new file will be prefix+name
        @param path_file: path to the source file
        @param apply_gzip: if true, the added file will be gzipped, the suffix
                .gz will be added to its resultant filename

        """
        with open(path_file, 'rb') as file_source:
            content = file_source.read()
        self.save_file(prefix, name, content, apply_gzip)


    def move_file(self, prefix, name, path_file, apply_gzip=False):
        """
        Add a new file to the archive. The file is moved, i.e. an original
        file is deleted.

        @param prefix: prefix of filename that the new file will be saved with
        @param name: the rest of the filename of the new file; in summary, the
                resultant filename of the new file will be prefix+name
        @param path_file: path to the source file, it will be deleted
        @param apply_gzip: if true, the added file will be gzipped, the suffix
                .gz will be added to its resultant filename

        """
        if apply_gzip:
            self.copy_file(prefix, name, path_file, apply_gzip)
            os.remove(path_file)
        else:
            path_target = os.path.join(self._path_directory, prefix + name)
            with self._lock:
                assert prefix in self._filenames_prefixes
                assert self._filenames_prefixes[prefix][1] is not None
                assert name not in self._filenames_prefixes[prefix][1]
                self._filenames_prefixes[prefix][1].add(name)
            shutil.move(path_file, path_target)


    def finalize_prefix(self, prefix):
        """
        This method is called to mark that there is no more files to add with
        given prefix. This method creates a tar archive when the last prefix
        assigned to the corresponding group is finalized. This method must be
        called for all prefixes given to the constructor.

        @param prefix: prefix to finalize, no more files with this prefix can
                be added to the archive

        """
        with self._lock:
            assert prefix in self._filenames_prefixes
            assert self._filenames_prefixes[prefix][1] is not None

            filenames = []
            for name in sorted(self._filenames_prefixes[prefix][1]):
                filenames.append(prefix + name)
            self._filenames_prefixes[prefix][1] = None
            archive_name = self._filenames_prefixes[prefix][0]

            self._archives_names[archive_name][0] -= 1
            self._archives_names[archive_name][1] += filenames
            if self._archives_names[archive_name][0] == 0:
                archive_is_complete = True
                filenames = self._archives_names[archive_name][1]
            else:
                archive_is_complete = False

        if archive_is_complete and len(filenames) > 0:
            argv = ['tar', 'cJf', 'archive_' + archive_name + '.tar.xz']
            argv += filenames
            process_tar = subprocess.Popen(argv, cwd=self._path_directory)
            if process_tar.wait() != 0:
                raise Exception("Process 'tar cJf' failed!")
            for filename in filenames:
                os.remove(os.path.join(self._path_directory, filename))
