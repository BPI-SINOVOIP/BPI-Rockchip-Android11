// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_FILE_UTILS_H_
#define LIBBRILLO_BRILLO_FILE_UTILS_H_

#include <sys/types.h>

#include <base/files/file_path.h>
#include <brillo/brillo_export.h>
#include <brillo/secure_blob.h>

namespace brillo {

// Ensures a regular file owned by user |uid| and group |gid| exists at |path|.
// Any other entity at |path| will be deleted and replaced with an empty
// regular file. If a new file is needed, any missing parent directories will
// be created, and the file will be assigned |new_file_permissions|.
// Should be safe to use in all directories, including tmpdirs with the sticky
// bit set.
// Returns true if the file existed or was able to be created.
BRILLO_EXPORT bool TouchFile(const base::FilePath& path,
                             int new_file_permissions,
                             uid_t uid,
                             gid_t gid);

// Convenience version of TouchFile() defaulting to 600 permissions and the
// current euid/egid.
// Should be safe to use in all directories, including tmpdirs with the sticky
// bit set.
BRILLO_EXPORT bool TouchFile(const base::FilePath& path);

// Writes the entirety of the given data to |path| with 0640 permissions
// (modulo umask).  If missing, parent (and parent of parent etc.) directories
// are created with 0700 permissions (modulo umask).  Returns true on success.
//
// Parameters
//  path      - Path of the file to write
//  blob/data - blob/string/array to populate from
// (size      - array size)
BRILLO_EXPORT bool WriteBlobToFile(const base::FilePath& path,
                                   const Blob& blob);
BRILLO_EXPORT bool WriteStringToFile(const base::FilePath& path,
                                     const std::string& data);
BRILLO_EXPORT bool WriteToFile(const base::FilePath& path,
                               const char* data,
                               size_t size);

// Calls fdatasync() on file if data_sync is true or fsync() on directory or
// file when data_sync is false.  Returns true on success.
//
// Parameters
//   path - File/directory to be sync'ed
//   is_directory - True if |path| is a directory
//   data_sync - True if |path| does not need metadata to be synced
BRILLO_EXPORT bool SyncFileOrDirectory(const base::FilePath& path,
                                       bool is_directory,
                                       bool data_sync);

// Atomically writes the entirety of the given data to |path| with |mode|
// permissions (modulo umask).  If missing, parent (and parent of parent etc.)
// directories are created with 0700 permissions (modulo umask).  Returns true
// if the file has been written successfully and it has physically hit the
// disk.  Returns false if either writing the file has failed or if it cannot
// be guaranteed that it has hit the disk.
//
// Parameters
//   path - Path of the file to write
//   blob/data - blob/array to populate from
//   (size - array size)
//   mode - File permission bit-pattern, eg. 0644 for rw-r--r--
BRILLO_EXPORT bool WriteBlobToFileAtomic(const base::FilePath& path,
                                         const Blob& blob,
                                         mode_t mode);
BRILLO_EXPORT bool WriteToFileAtomic(const base::FilePath& path,
                                     const char* data,
                                     size_t size,
                                     mode_t mode);

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_FILE_UTILS_H_
