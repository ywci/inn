#!/bin/sh
SYS=`uname`
if [ $SYS = 'Linux' ]; then
    IFACE=`grep iface ../conf/inn.yaml | awk -F ' ' '{print $2}'`
    FILE=`readlink -f $0`
elif [ $SYS = 'Darwin' ]; then
    IFACE=`grep iface ../conf/inn.yaml | gawk -F ' ' '{print $2}'`
    FILE=`greadlink -f $0`
fi
CWD=`dirname "$FILE"`
HOME=`dirname "$CWD"`
CONF=$CWD/conf.h
if [ ! -e "$CONF" ]; then
    source "$HOME/conf/build.cfg"
    echo "#define IFACE \"$IFACE\"">>"$HOME/tests/conf.h"
    if [ "$CLONE_TS" = "1" ]; then
        echo "#define CLONE_TS">>"$HOME/tests/conf.h"
    fi
    if [ "$COUNTABLE" = "1" ]; then
        echo "#define COUNTABLE">>"$HOME/tests/conf.h"
    fi
fi
