# configure.py
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
    'name':'',
    'logging':'',
    'version': '0.0.1'}

_toolinfo = {}
_define = ['-DERROR']

def _log(text):
    if _settings['logging']:
        print(text)

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

def _chktools():
    tools = ['aclocal', 'autoconf', 'autoheader', 'automake', 'autoreconf']
    for i in tools:
        if not getoutput('which %s' %i):
            raise Exception('cannot find %s' %i)
    version = None
    ret = getoutput('autoconf -V | head -n1')
    if ret:
        version = ret.split(' ')[-1]
    if not version:
        raise Exception('cannot get the version of autoconf')
    _toolinfo['autoconf'] = {'version':version}

def _conf_proj():
    lines = []
    lines.append('AC_PREREQ([%s])\n' % _toolinfo['autoconf']['version'])
    lines.append('AC_INIT([%s], [%s])\n' % (_settings['name'], _settings['version']))
    lines.append('AC_CONFIG_SRCDIR([config.h.in])\n')
    lines.append('AC_CONFIG_HEADERS([config.h])\n')
    lines.append('AC_CONFIG_MACRO_DIR([m4])\n')
    lines.append('LT_INIT\n')
    lines.append('AC_CHECK_HEADERS([stdint.h yaml.h zmq.h czmq.h])\n')
    lines.append('AC_CHECK_LIB([pthread], [yaml], [zmq], [czmq])\n')
    lines.append('AM_INIT_AUTOMAKE([foreign subdir-objects -Werror])\n')
    lines.append('AC_OUTPUT([Makefile])\n')

    with open(os.path.join(_settings['home'], 'configure.ac'), 'w') as f:
        f.writelines(lines)

    lines = []
    lines.append('ACLOCAL_AMFLAGS = -I m4\n')
    lines.append('LDFLAGS = -L/usr/local/lib\n')
    lines.append('LIBS = -lpthread -lzmq -lczmq -lyaml\n')
    lines.append('AM_CFLAGS = %s -I./include -I./src/lib -I./src -I/usr/local/include\n\n' % ' '.join(_define))

    cnt = 0
    lines.append('bin_PROGRAMS = %s\n' % _settings['name'])
    sources = '%s_SOURCES = ' % _settings['name']
    for i in os.listdir(os.path.join(_settings['home'], 'src')):
        if i.endswith('.c'):
            if cnt % 8 == 7:
                sources += '\\\n\t'
            sources += '%s ' % os.path.join('src', i)
            cnt += 1
    lines.append(sources + '\n')
    lines.append('%s_LDADD = lib%s.a\n\n' % (_settings['name'], _settings['name']))

    cnt = 0
    lines.append('AUTOMAKE_OPTIONS=foreign\n')
    lines.append('noinst_LIBRARIES = lib%s.a\n' % _settings['name'])
    sources = 'lib%s_a_SOURCES = ' % _settings['name']
    path = os.path.join(_settings['home'], 'src', 'lib')
    for i in os.listdir(path):
        if i.endswith('.h') or i.endswith('.c'):
            if cnt % 8 == 7:
                sources += '\\\n\t'
            sources += 'src/lib/%s ' % i
            cnt += 1
    lines.append(sources + '\n')

    with open(os.path.join(_settings['home'], 'Makefile.am'), 'w') as f:
        f.writelines(lines)

def _configure():
    _log('Checking required tools ...')
    _chktools()

    _log('Configuring project ...')
    _conf_proj()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true')
    parser.add_argument('-r', '--reuse', action='store_true')
    parser.add_argument('-b', '--balance', action='store_true')
    parser.add_argument('-l', '--logging', action='store_true')
    parser.add_argument('-e', '--evaluate', action='store_true')
    parser.add_argument('-f', '--fastmode', action='store_true')
    args = parser.parse_args(sys.argv[1:])

    _settings['home'] = _get_home_dir()
    _settings['name'] = os.path.basename(_settings['home'])
    _settings['logging'] = args.logging

    if args.debug:
        _define += ['-DDEBUG']

    if args.evaluate:
        _define += ['-DEVAL']

    if args.balance:
        _define += ['-DBALANCE']

    if args.fastmode:
        _define += ['-DFASTMODE']

    if args.reuse:
        _define += ['-DREUSE']

    if platform.system() == 'Linux':
        _define += ['-DLINUX']

    _configure()
