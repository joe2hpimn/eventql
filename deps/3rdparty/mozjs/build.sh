#!/bin/sh

mkdir __build
cd __build

CC=$1 CXX=$2 RANLIB=$3 AR=$4 CFLAGS="-DJS_THREADSAFE" CXXFLAGS="-DJS_THREADSAFE" ../js/src/configure --disable-shared-js || exit 1
CC=$1 CXX=$2 RANLIB=$3 AR=$4 CFLAGS="-DJS_THREADSAFE" CXXFLAGS="-DJS_THREADSAFE" make JS_THREADSAFE=1 || exit 1
