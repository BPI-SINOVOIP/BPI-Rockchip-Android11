# coding: utf-8
from __future__ import unicode_literals, division, absolute_import, print_function

import os

from . import package_name, package_root

import flake8
if not hasattr(flake8, '__version_info__') or flake8.__version_info__ < (3,):
    from flake8.engine import get_style_guide
else:
    from flake8.api.legacy import get_style_guide


def run():
    """
    Runs flake8 lint

    :return:
        A bool - if flake8 did not find any errors
    """

    print('Running flake8 %s' % flake8.__version__)

    flake8_style = get_style_guide(config_file=os.path.join(package_root, 'tox.ini'))

    paths = []
    for _dir in [package_name, 'dev', 'tests']:
        for root, _, filenames in os.walk(_dir):
            for filename in filenames:
                if not filename.endswith('.py'):
                    continue
                paths.append(os.path.join(root, filename))
    report = flake8_style.check_files(paths)
    success = report.total_errors == 0
    if success:
        print('OK')
    return success
