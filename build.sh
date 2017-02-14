#!/bin/bash
SYSTEM=`uname`
if [ "$SYSTEM" = "Linux" ]; then
  FILE=`readlink -f $0`
elif [ "$SYSTEM" = "Darwin" ]; then
  FILE=`greadlink -f $0`
else
  echo "Error: unknown system"
  exit
fi
HOME=`dirname "$FILE"`
CURRENT=`pwd`
cd "$HOME"
source conf/build.cfg
if [ "$1" = "-b" ]; then
  length=`echo $@|wc -c`
  args=`echo $@|cut -c 4-$((length))`
  echo "Building ..."
  {
    python scripts/build.py $args
  } || {
    echo "Failed to build :-("
    cd "$CURRENT"
    exit
  }
  echo "Finished"
elif [ "$1" = "-c" ]; then
  length=`echo $@|wc -c`
  args=`echo $@|cut -c 4-$((length))`
  echo "Configuring ..."
  {
    python scripts/configure.py $args
  } || {
    echo "Failed to configure :-("
    cd "$CURRENT"
    exit
  }
  echo "Finished"
elif [ "$1" = "-h" -o "$1" = "--help" -o "$1" = "-help" ]; then
  echo "usage: $0 [-c] [-b] [-h] [--clean|-clean|clean]"
  echo "-c: configure"
  echo "-b: build"
  echo "-h: help"
  echo "--clean or -clean or clean: clean all"
elif [ "$1" = "--clean" -o "$1" = "-clean" -o "$1" = "clean" ]; then
  for name in `ls -a`; do
    i=`basename $name`
    if [ "$i" != "README.md" -a "$i" != "src" -a "$i" != "include" -a "$i" != "scripts" -a "$i" != "tests" -a "$i" != "conf" -a "$i" != "bin" -a "$i" != "build.sh" -a "$i" != "." -a "$i" != ".." ]; then
      rm -rf $i
    fi
  done
  if [ -e .deps ]; then
    rm -rf .deps
  fi
  if [ -e src/.deps ]; then
    rm -rf src/.deps
  fi
  if [ -e src/.dirstamp ]; then
    rm src/.dirstamp
  fi
  if [ -e src/lib/.deps ]; then
    rm -rf src/lib/.deps
  fi
  if [ -e src/lib/.dirstamp ]; then
    rm src/lib/.dirstamp
  fi
  rm src/*.o 2>/dev/null
  rm src/lib/*.o 2>/dev/null
  cd tests
  make clean 1>/dev/null
else
  echo "Configuring ..."
  {
    args=""
    if [ $DEBUG = 1 ]; then
      args="--debug"
    fi
    if [ $EVAL = 1 ]; then
      args=$args" --evaluate"
    fi
    if [ $BALANCE = 1 ]; then
      args=$args" --balance"
    fi
    if [ $FASTMODE = 1 ]; then
      args=$args" --fastmode"
    fi
    if [ $REUSE = 1 ]; then
      args=$args" --reuse"
    fi
    python scripts/configure.py $args 1>/dev/null
  } || {
    echo "Failed to configure :-("
    cd "$CURRENT"
    exit
  }

  echo "Building ..."
  {
    python scripts/build.py 1>/dev/null
    cd tests
    make 1>/dev/null
  } || {
    echo "Failed to build :-("
    cd "$CURRENT"
    exit
  }

  echo "Finished"
fi

cd "$CURRENT"
