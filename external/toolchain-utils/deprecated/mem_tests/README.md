# Memory tests

## Usage

These scripts are made to parse TCMalloc output in order to extract certain
info from them.

In particular, these scripts rely on the error logging system for ChromeOS in
order to extract information. In order to use a script (e.g. `total_mem.py`),
you just have the command:

```
./total_mem.py FILENAME
```

where `FILENAME` is the name of the log file to be parsed.

## Codebase Changes

There are two ideas that motivate these changes:

1. Turn on TCMalloc sampling.
2. Use perf to collect the sample information.


The following files have to be changed:

in `chrome/browser/metrics/perf_provider_chrome_os`, add:

```
#include "third_party/tcmalloc/chromium/src/gperftools/malloc_extension.h"
```

Change the perf profiling interval to something small (60*1000 milliseconds).

Inside DoPeriodicCollection, insert the following code:

```
std::string output;
char* chr_arr = new char[9999];
MallocExtension::instance() ->GetHeapSample(&output);
MallocExtension::instance() ->GetStats(chr_arr, 9999);
LOG(ERROR) << "Output Heap Data: ";
LOG(ERROR) << output;
LOG(ERROR) << "Output Heap Stats: ";
output = "";
for (unsigned int i = 0; i < strlen(chr_arr); i++) {
    output += chr_arr[i];
}
LOG(ERROR) << output;
delete[] chr_arr;
```
