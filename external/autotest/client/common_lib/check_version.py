# This file must use Python 1.5 syntax.
import glob
import os
import sys


class check_python_version:

    def __init__(self):
        # The change to prefer 2.4 really messes up any systems which have both
        # the new and old version of Python, but where the newer is default.
        # This is because packages, libraries, etc are all installed into the
        # new one by default. Some things (like running under mod_python) just
        # plain don't handle python restarting properly. I know that I do some
        # development under ipython and whenever I run (or do anything that
        # runs) 'import common' it restarts my shell. Overall, the change was
        # fairly annoying for me (and I can't get around having 2.4 and 2.5
        # installed with 2.5 being default).
        if sys.version_info.major >= 3:
            try:
                # We can't restart when running under mod_python.
                from mod_python import apache
            except ImportError:
                self.restart()


    PYTHON_BIN_GLOB_STRINGS = ['/usr/bin/python2*', '/usr/local/bin/python2*']


    def find_desired_python(self):
        """Returns the path of the desired python interpreter."""
        # CrOS only ever has Python 2.7 available, so pick whatever matches.
        pythons = []
        for glob_str in self.PYTHON_BIN_GLOB_STRINGS:
            pythons.extend(glob.glob(glob_str))
        return pythons[0]


    def restart(self):
        python = self.find_desired_python()
        sys.stderr.write('NOTE: %s switching to %s\n' %
                         (os.path.basename(sys.argv[0]), python))
        sys.argv.insert(0, '-u')
        sys.argv.insert(0, python)
        os.execv(sys.argv[0], sys.argv)
