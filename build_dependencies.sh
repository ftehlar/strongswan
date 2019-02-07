#!/usr/bin/env bash

# Copyright (c) 2018 Cisco and/or its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# builds all dependencies used by kernel_vpp and socket_vpp plugins
set -e

WS="`pwd`"
LOG="/tmp/build.log"

echo "" > $LOG

git submodule update --init 1>> $LOG

echo "Building dependency: grpc-c"
cd ${WS}/third_party/grpc-c 1>> $LOG
git submodule update --init 1>> $LOG
autoreconf --install 1>> $LOG

echo "Building gRPC"
cd third_party/grpc
git submodule update --init 1>> $LOG
(CFLAGS="-Wno-implicit-fallthrough -Wno-stringop-overflow -Wno-error=conversion" make && sudo make install) 1>> $LOG
cd third_party/protobuf
make
sudo make install
cd ../../
cd ../../

echo "Building protobuf-c"
cd third_party/protobuf-c
(./autogen.sh && ./configure && make && sudo make install) 1>> $LOG
cd ../../

mkdir build -p; cd build
(../configure && make && sudo make install) 1>> $LOG

echo "Building hiredis"
cd ${WS}/third_party/hiredis
git submodule update --init 1>> $LOG
make && sudo make install

echo "Generating vpp-agent API files"
cd ${WS}/third_party/vpp-agent
git submodule update --init 1>> $LOG
git checkout dev 1>> $LOG

cd ${WS}
./gen_api.sh

echo "Building vpp-agent API C lib"
cd ${WS}/third_party/vpp_agent_c_api
(./autogen.sh && ./configure && make && sudo make install) 1>> $LOG
sudo ldconfig
