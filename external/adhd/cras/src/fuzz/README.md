# OSS-Fuzz for CRAS

This directory contains source code and build scripts for coverage-guided
fuzzers.

Detailed instructions are available at: https://github.com/google/oss-fuzz/blob/master/docs/

## Quick start

### Sudoless Docker
```
sudo adduser $USER docker
```

### Build a container from the cras directory
```
docker build -t ossfuzz/cras -f src/fuzz/Dockerfile .
```

### Build fuzzers
```
docker run --cap-add=SYS_PTRACE -ti --rm -v $(pwd):/src/cras -v /tmp/fuzzers:/out \
    ossfuzz/cras
```

### Look in /tmp/fuzzers to see the executables. Run them like so:
```
docker run --cap-add=SYS_PTRACE -ti -v $(pwd)/src/fuzz/corpus:/corpus \
    -v /tmp/fuzzers:/out ossfuzz/base-runner /out/rclient_message \
    /corpus -runs=100
```

### Debug in docker

Go into docker console by
```
docker run --cap-add=SYS_PTRACE -ti -v $(pwd)/src/fuzz/corpus:/corpus \
    -v /tmp/fuzzers:/out ossfuzz/base-runner /bin/bash
```
and start debugging.
