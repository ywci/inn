#!/bin/sh
SYS=`uname`
if [ $SYS = 'Linux' ]; then
    IFACE=`grep iface ../conf/inn.yaml | awk -F ' ' '{print $2}'`
    sed -i 's/eth0/'$IFACE'/g' back2back.c
elif [ $SYS = 'Darwin' ]; then
    IFACE=`grep iface ../conf/inn.yaml | gawk -F ' ' '{print $2}'`
    gsed -i 's/eth0/'$IFACE'/g' back2back.c
fi
