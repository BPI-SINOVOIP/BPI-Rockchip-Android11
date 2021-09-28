#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import filecmp
import logging
import os
import shutil
import tempfile

from acts.libs.proc import job
from acts.libs.proto import proto_utils

COMMIT_ID_ENV_KEY = 'PREUPLOAD_COMMIT'
GIT_FILE_NAMES_CMD = 'git diff-tree --no-commit-id --name-only -r %s'


def proto_generates_gen_file(proto_file, proto_gen_file):
    """Check that the proto_gen_file matches the compilation result of the
    proto_file.

    Params:
        proto_file: The proto file.
        proto_gen_file: The generated file.

    Returns: True if the compiled proto matches the given proto_gen_file.
    """
    with tempfile.TemporaryDirectory() as tmp_dir:
        module_name = proto_utils.compile_proto(proto_file, tmp_dir)
        if not module_name:
            return False
        tmp_proto_gen_file = os.path.join(tmp_dir, '%s.py' % module_name)
        return filecmp.cmp(tmp_proto_gen_file, proto_gen_file)


def verify_protos_update_generated_files(proto_files, proto_gen_files):
    """For each proto file in proto_files, find the corresponding generated
    file in either proto_gen_files, or in the directory tree of the proto.
    Verify that the generated file matches the compilation result of the proto.

    Params:
        proto_files: List of proto files.
        proto_gen_files: List of generated files.
    """
    success = True
    gen_files = set(proto_gen_files)
    for proto_file in proto_files:
        gen_filename = os.path.basename(proto_file).replace('.proto', '_pb2.py')
        gen_file = ''
        # search the gen_files set first
        for path in gen_files:
            if (os.path.basename(path) == gen_filename
                    and path.startswith(os.path.dirname(proto_file))):
                gen_file = path
                gen_files.remove(path)
                break
        else:
            # search the proto file's directory
            for root, _, filenames in os.walk(os.path.dirname(proto_file)):
                for filename in filenames:
                    if filename == gen_filename:
                        gen_file = os.path.join(root, filename)
                        break
                if gen_file:
                    break

        # check that the generated file matches the compiled proto
        if gen_file and not proto_generates_gen_file(proto_file, gen_file):
            logging.error('Proto file %s does not compile to %s'
                          % (proto_file, gen_file))
            protoc = shutil.which('protoc')
            if protoc:
                protoc_command = ' '.join([
                    protoc, '-I=%s' % os.path.dirname(proto_file),
                    '--python_out=%s' % os.path.dirname(gen_file), proto_file])
                logging.error('Run this command to re-generate file from proto'
                              '\n%s' % protoc_command)
            success = False

    # fail if there are unpaired gen_files
    if gen_files:
        logging.error('Generated *_pb2.py files modified without updating '
                      'source .proto files: \n\t%s' % '\n\t'.join(gen_files))
        success = False

    return success


def main():
    if COMMIT_ID_ENV_KEY not in os.environ:
        logging.error('Missing commit id in environment.')
        exit(1)

    # get list of *.proto and *_pb2.py files from commit, then compare
    proto_files = []
    proto_gen_files = []
    git_cmd = GIT_FILE_NAMES_CMD % os.environ[COMMIT_ID_ENV_KEY]
    files = job.run(git_cmd).stdout.splitlines()
    for f in files:
        if f.endswith('.proto'):
            proto_files.append(os.path.abspath(f))
        if f.endswith('_pb2.py'):
            proto_gen_files.append(os.path.abspath(f))
    exit(not verify_protos_update_generated_files(proto_files, proto_gen_files))


if __name__ == '__main__':
    main()
