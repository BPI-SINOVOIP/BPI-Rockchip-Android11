Tests naming convention
-----------------------
Names of tests (suffixes of control.* files) are build from 2 or 3 words
separated by an underscore character. The first one is '100' or 'all', what
defines the set of PPD files to test: '100' means 100 chosen (most often used)
PPD files (stored in the archive ppds_100) while 'all' means all available PPD
files (stored in the archive ppds_all). The second word of the name is either
'test' or 'dump'. The third and optional word is 'debug'. All these three words
influence testing procedure used by the autotest, their meanings are described
in next paragraphs.

Overall testing procedure
-------------------------
The purpose of this autotest is to verify that given subset of PPD files work
in ChromeOS. Each PPD file is tested with the following procedure:
1. A printer driver is added to CUPS server.
2. Test documents are printed on the configured printer.
3. Raw output from the CUPS server is intercepted by, so called, FakePrinter.
4. CUPS logs are parsed to make sure that no errors occured.
5. Obtained outputs are verified (see below) - test only.
6. A printing pipeline used by CUPS is rerun in shell (see below) - debug only.
7. All obtained outputs & logs are saved on the device (see below) - dump only.
8. The printer driver is removed from CUPS server.
This procedure is repeated for every PPD file. The number of PPD files may be
large (3K+ files). To decrease amount of time needed by the autotest, several
PPD files are tested simultaneously in parallel threads. Autotest always run
the procedure for all given PPD files and print a summary report at the end.
If at least one of PPD files fails, whole autotest is finished with failure
(but always all PPD files are processed).

Output verification ('test')
----------------------------
Intercepted output is verified by comparision with the previous results
obtained for the same PPD. We cannot store outputs directly, because their
total size may have hundreds of megabytes. Instead of that short digest is
calculated for each obtained document and it is used for comparison.
A function for digests calculation is in the 'helpers.py' file. Not all
outputs can be tested this way because for some PPD files produced contents
differ between runs. List of PPD files for which we cannot calculate
constant digest is saved in the file digests_blacklist.txt. Files with
expected digests for every test document are stored in the directory "digests".
If a digests for given pair (test document, PPD file) is missing, the test
checks only check if the output is not empty (or not too short).

Rerun printing pipeline ('debug')
---------------------------------
Every PPD file defines printing pipeline used by CUPS, consisting of external
binaries (so called filters). To make sure that every part of the printing
pipeline works correctly, it can be run outside CUPS in Linux shell. In tests
with 'debug' suffix in their names, whole pipeline is rerun outside CUPS. In
this mode every document is processed twice: the first time by CUPS and the
second time by extracted pipeline run as bash script. Additionally, each
pipeline's step is verified by checking its return code (it is supposed to be
equal 0) and the final output is compared with the output returned by CUPS
server. If the test is also defined as 'dump', each intermediate content is
saved to the directory given in path_outputs parameter.
This mode requires more disk space on tested device (~2GB in /tmp) and takes
more execution time (~2 times more). All information needed to reconstruct
a pipeline used by CUPS can are extracted from CUPS logs after the job is
completed. To do that, CUPS configuration must be modified: the LogLevel
option in /etc/cups/cupsd.conf must be set to "debug". This modification is
done when the autotest is initialized; the original setting is restored in
the cleaup stage. 


Save outputs and logs ('dump')
------------------------------
All obtained outputs and logs are saved on the device in the directory
/tmp/PrinterPpds_outputs/. Results obtained from PPD files with the same prefix
are grouped together and stored in single archive to limit usage of disk space.

Test parameters
---------------
path_docs - path to directory with test documents (PDF files)
path_ppds - path to directory with PPD files, if not set then all available PPD
            files are downloaded and tested
path_digests - path to directory with files containing digests for
            verification, if not set then outputs are not verified
path_outputs - if set, then all outputs are dumped there (given directory is
            deleted if already exists); also all digests files are recalculated
            and saved in the same directory
debug_mode - True or False, if True then for every test the printing pipeline
            is extracted from CUPS logs and executed again outside CUPS

Generating new digests
----------------------
The following procedure can be used to update digests:
1. Run the test defined in control.all_dump:
        test_that <device IP>  platform_PrinterPpds.all_dump
2. Download generated files with digests to your workstation
        rsync root@<device IP>:/tmp/PrinterPpds_outputs/*.digests <local dir>
3. Replace the files from the "digests" directory and commit changes

Updating the archive with PPD files
-----------------------------------
Currently, all tests are based on PPD files stored in local directories. The
autotest can download all PPD files by itself, but we do not use this option
to limit the number of possible points of failures during testing. PPD files
are stored in the archive 'ppds_all.tar.xz'. To replace the archive with the
current list of supported PPD files, one can use the script 
'download_ppds_make_archive.py'.

Others
------
* How to get comma-separated list of all used ghostscript devices?
  1. Go to the directory generated by all_dump_debug
  2. Unpack all tar.xz archives (not on the device)
        for d in *.pdf;
        do
            cd $d;
            for f in *.tar.xz; do tar xf $f; done ;
            cd ..;
        done
  3. Run the following
        grep -o sDEVICE=[^\ ]*\  ./*.pdf/*.err[12345] | cut -d \= -f 2 | sort | uniq | tr -d ' ' | tr '\n' ','
  4. Add to the list uniprint (it is not caught by the command above)
* How to get a list of all used cups filters?
  1. Go to the directory generated by all_dump_debug
  2. Unpack all tar.xz archives (not on the device)
        for d in *.pdf;
        do
            cd $d;
            for f in *.tar.xz; do tar xf $f; done ;
            cd ..;
        done
  3. Run the following
        grep -o '(exec -a "Fake[^"]*" [^ ]* ' ./*.pdf/*.sh | cut -d \  -f 4 | sort | uniq
