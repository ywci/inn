# build.py
#
# Copyright (C) 2017 Yi-Wei Ci
#
# Distributed under the terms of the MIT license.
#

import os
import sys
import argparse
import platform
from commands import getoutput, getstatusoutput

_settings = {
    'home':'',
    'logging':''}

def _log(text):
    if _settings['logging']:
        print(text)

def _system(path, cmd, quiet=False):
    os.chdir(path)
    if not quiet:
        status, output = getstatusoutput(cmd)
        if status:
            raise Exception(output)
    else:
        getoutput(cmd)

def _get_home_dir():
    system = platform.system()
    if system == 'Linux':
        readlink = 'readlink'
    elif system == 'Darwin':
        readlink = 'greadlink'
        if not getoutput('which greadlink'):
            raise Exception('greadlink is not installed')
    else:
        raise Exception('%s is not supported.' % system)

    f = getoutput('%s -f %s' % (readlink, sys.argv[0]))
    d = os.path.dirname(f)
    return os.path.dirname(d)

def _build(path):
    _system(path, 'aclocal')
    _system(path, 'autoconf')
    _system(path, 'autoheader')
    _system(path, 'touch NEWS README AUTHORS ChangeLog')
    _system(path, 'automake --add-missing', quiet=True)
    _system(path, 'autoreconf -if')
    _system(path, './configure')
    _system(path, 'make')

def _build_proj():
    _log('Building project ...')
    _build(_settings['home'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--logging', action='store_true')
    args = parser.parse_args(sys.argv[1:])

    _settings['home'] = _get_home_dir()
    _settings['logging'] = args.logging

    _build_proj()
