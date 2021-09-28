"""Django settings for lightweight in-memory model database.
"""

import common

LIGHTWEIGHT_DEFAULT = {
    'ENGINE': 'django.db.backends.sqlite3',
    'NAME': ':memory:'
}

DATABASES = {'default': LIGHTWEIGHT_DEFAULT}

INSTALLED_APPS = (
    'frontend.afe',
#    'frontend.tko',
)

# Use UTC, and let the logging module handle local time.
USE_TZ = False
TIME_ZONE = None

# Required for Django to start, even though not used.
SECRET_KEY = 'Three can keep a secret if two are dead.'

AUTOTEST_CREATE_ADMIN_GROUPS = False
