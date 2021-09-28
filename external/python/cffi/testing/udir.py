import py
import sys

udir = py.path.local.make_numbered_dir(prefix = 'ffi-')


# Windows-only workaround for some configurations: see
# https://bugs.python.org/issue23246 (Python 2.7.9)
if sys.platform == 'win32':
    try:
        import setuptools
    except ImportError:
        pass
