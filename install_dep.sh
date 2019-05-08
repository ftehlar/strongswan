#!/usr/bin/env bash

set -e

WS="`pwd`"

sudo apt-get install libgmp-dev automake autoconf libtool pkg-config gettext \
    perl python flex bison gperf protobuf-compiler libprotobuf-dev libprotoc-dev

git submodule update --init

cd ${WS}/grpc
git checkout v1.3.0
git submodule update --init
CFLAGS="-Wno-implicit-fallthrough -Wno-stringop-overflow -Wno-error=conversion" make
sudo make install

sudo ldconfig

cd ${WS}/api
./gen.sh
autoreconf --install
./configure
make
sudo make install-strip
sudo ldconfig
